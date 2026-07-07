#include "ota.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "ota";

// ========== OTA 完成回调 ==========
static ota_complete_cb_t s_complete_cb = NULL;
static void *s_complete_ctx = NULL;

// ========== 异步 OTA 任务专用变量 ==========
static SemaphoreHandle_t s_ota_semaphore = NULL;
static TaskHandle_t s_ota_task_handle = NULL;
static char s_ota_url[256];

// 内部函数声明
static void ota_task(void *pvParameters);
static esp_err_t ota_download_start(const char *url);

// ========== 公开 API ==========

esp_err_t ota_init(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "OTA 组件初始化完成，当前运行分区: %s", running ? running->label : "unknown");

    // 创建二进制信号量
    s_ota_semaphore = xSemaphoreCreateBinary();
    if (s_ota_semaphore == NULL) {
        ESP_LOGE(TAG, "创建 OTA 信号量失败");
        return ESP_FAIL;
    }

    // 创建专用 OTA 任务（栈 4096，优先级 5）
    BaseType_t ret = xTaskCreate(ota_task, "ota_task", 4096, NULL, 5, &s_ota_task_handle);
    if (ret != pdTRUE) {
        ESP_LOGE(TAG, "创建 OTA 任务失败");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "OTA 异步任务已创建");
    return ESP_OK;
}

void ota_set_complete_callback(ota_complete_cb_t cb, void *user_ctx)
{
    s_complete_cb = cb;
    s_complete_ctx = user_ctx;
}

esp_err_t ota_start_download_async(const char *url)
{
    if (url == NULL || strlen(url) == 0) {
        ESP_LOGE(TAG, "固件下载地址为空");
        return ESP_ERR_INVALID_ARG;
    }
    strncpy(s_ota_url, url, sizeof(s_ota_url) - 1);
    s_ota_url[sizeof(s_ota_url) - 1] = '\0';
    ESP_LOGI(TAG, "已接收 OTA 升级请求，触发下载任务: %s", s_ota_url);
    xSemaphoreGive(s_ota_semaphore);
    return ESP_OK;
}

// ========== 专用 OTA 任务 ==========

static void ota_task(void *pvParameters)
{
    ESP_LOGI(TAG, "OTA 异步任务已启动，等待升级信号...");
    while (1) {
        if (xSemaphoreTake(s_ota_semaphore, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "OTA 任务收到升级信号，开始执行");
            esp_err_t ret = ota_download_start(s_ota_url);
            if (s_complete_cb) {
                s_complete_cb(ret, s_complete_ctx);
            }
        }
    }
}

// ========== 核心下载逻辑 ==========

/**
 * ota_download_start - 从 HTTP 下载完整固件并写入空闲 OTA 分区
 * 
 * 设计思路：
 *   1. 找空房子（空闲 OTA 分区）
 *   2. 快递员（HTTP）从仓库（服务器）拉货
 *   3. 每拉一件就搬进新房子（esp_ota_write）
 *   4. 每搬一件就让让路（vTaskDelay），免得挡着别人
 *   5. 全部搬完 → 清点（CRC 校验）→ 换锁（设置启动分区）→ 搬家（重启）
 * 
 * 关于断点续传：
 *   当初尝试了 HTTP Range 续传，但因 esp_ota_write() 只能从偏移 0 顺序写入
 *   而作罢。1MB 固件局域网下载约 3-5 秒，全量重下的代价完全可以接受。
 *   详见 docs/ignore_docs/断点续传开发记录.md
 */
static esp_err_t ota_download_start(const char *url)
{
    if (url == NULL || strlen(url) == 0) {
        ESP_LOGE(TAG, "固件下载地址为空");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "开始 OTA 升级，固件地址: %s", url);

    // 1. 找出空闲的 OTA 分区
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "找不到空闲 OTA 分区");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "目标分区: %s (偏移 0x%08x, 大小 %d KB)",
             update_partition->label,
             update_partition->address,
             update_partition->size / 1024);

    // 2. 开始 OTA 会话
    esp_ota_handle_t ota_handle;
    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin 失败: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "OTA 会话已开始，准备下载");

    // 3. 配置 HTTP 客户端
    esp_http_client_config_t http_config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .buffer_size = 1024,
        .buffer_size_tx = 1024,
        .keep_alive_enable = false,
    };

    esp_http_client_handle_t http_client = esp_http_client_init(&http_config);
    if (http_client == NULL) {
        ESP_LOGE(TAG, "HTTP 客户端初始化失败");
        esp_ota_abort(ota_handle);
        return ESP_FAIL;
    }

    // 4. 打开 HTTP 连接
    err = esp_http_client_open(http_client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP 连接打开失败: %s", esp_err_to_name(err));
        esp_http_client_cleanup(http_client);
        esp_ota_abort(ota_handle);
        return err;
    }

    int content_length = esp_http_client_fetch_headers(http_client);
    int status_code = esp_http_client_get_status_code(http_client);
    ESP_LOGI(TAG, "HTTP 响应: 状态码 %d, 内容长度 %d 字节",
             status_code, content_length);

    if (status_code != 200) {
        ESP_LOGE(TAG, "服务器返回非 200 状态码: %d", status_code);
        esp_http_client_close(http_client);
        esp_http_client_cleanup(http_client);
        esp_ota_abort(ota_handle);
        return ESP_FAIL;
    }

    // 5. 分块读取并写入 Flash（从字节 0 顺序写入）
    char buffer[1024];
    int total_read = 0;
    int last_percent = -1;

    while (1) {
        int read_len = esp_http_client_read(http_client, buffer, sizeof(buffer));
        if (read_len == -1) {
            ESP_LOGE(TAG, "HTTP 读取错误（已接收 %d 字节）", total_read);
            esp_http_client_close(http_client);
            esp_http_client_cleanup(http_client);
            esp_ota_abort(ota_handle);
            return ESP_FAIL;
        }
        if (read_len == 0) break;

        // 顺序写入 Flash（始终从偏移 0 开始）
        err = esp_ota_write(ota_handle, buffer, read_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Flash 写入失败: %s", esp_err_to_name(err));
            esp_http_client_close(http_client);
            esp_http_client_cleanup(http_client);
            esp_ota_abort(ota_handle);
            return err;
        }

        total_read += read_len;

        // 让出 CPU，防止空闲任务饿死触发 TG1WDT 复位
        vTaskDelay(pdMS_TO_TICKS(5));

        // 每 10% 打印一次进度
        if (content_length > 0) {
            int percent = (total_read * 100) / content_length;
            if (percent != last_percent && percent % 10 == 0) {
                ESP_LOGI(TAG, "下载进度: %d%% (%d/%d KB)",
                         percent, total_read / 1024, content_length / 1024);
                last_percent = percent;
            }
        }
    }

    ESP_LOGI(TAG, "固件下载完成，共 %d 字节，正在校验...", total_read);

    // 6. 关闭 HTTP 连接
    esp_http_client_close(http_client);
    esp_http_client_cleanup(http_client);

    // 7. 结束 OTA 会话（内部做 CRC 校验）
    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "固件校验失败: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "固件校验通过");

    // 8. 设置新分区为启动目标，重启
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "设置启动分区失败: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "OTA 升级成功，3 秒后重启到 %s...", update_partition->label);
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
    return ESP_OK;
}

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "wifi.h"
#include "camera.h"
#include "http_server.h"
#include "mqtt.h"
#include "spi_flash_mmap.h"
#include "esp_flash.h"
#include "esp_partition.h"  // 新增：用于操作Flash分区表

static const char *TAG = "cam_http_server";

void app_main(void) {
    // 初始化NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "ESP32-CAM项目启动");

    // 检查PSRAM（ESP32-CAM必需）
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) < 1024 * 1024) {
        ESP_LOGW(TAG, "PSRAM可用不足1MB，可能导致卡顿");
    }

    ESP_LOGI(TAG, "内部内存可用: %d 字节", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "PSRAM可用: %d 字节", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "总可用内存: %d 字节", esp_get_free_heap_size());

    // 1. 获取Flash总物理容量
    uint32_t flash_total_size = 0;
    esp_err_t err = esp_flash_get_physical_size(esp_flash_default_chip, &flash_total_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "获取Flash总容量失败: %s", esp_err_to_name(err));
        return;  // 总容量获取失败，后续计算无意义
    }

    // 2. 遍历所有分区，计算已使用容量（分区表中已定义的所有分区大小总和）
    uint32_t flash_used_size = 0;
    // 创建分区迭代器（遍历所有类型的分区）
    esp_partition_iterator_t iter = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    if (iter == NULL) {
        ESP_LOGE(TAG, "创建分区迭代器失败，无法计算已使用容量");
    } else {
        // 循环遍历每个分区
        for (; iter != NULL; iter = esp_partition_next(iter)) {
            const esp_partition_t *part = esp_partition_get(iter);
            // 累加每个分区的大小（跳过“未分配”的分区，若存在）
            if (part->type != ESP_PARTITION_TYPE_ANY) {
                flash_used_size += part->size;
                // （可选）打印每个分区的详情，方便调试
                ESP_LOGD(TAG, "分区: %s (类型: %d)，大小: %u 字节", 
                         part->label, part->type, part->size);
            }
        }
        // 销毁迭代器，避免内存泄漏
        esp_partition_iterator_release(iter);
    }

    // 3. 计算剩余容量（总容量 - 已使用容量）
    uint32_t flash_free_size = (flash_total_size >= flash_used_size) ? (flash_total_size - flash_used_size) : 0;

    // 4. 打印所有Flash容量信息

    ESP_LOGI(TAG, "实际Flash总物理容量: %u 字节 (%.2f MB)", 
             flash_total_size, (float)flash_total_size / (1024 * 1024));
    ESP_LOGI(TAG, "已使用容量（分区表已分配）: %u 字节 (%.2f MB)", 
             flash_used_size, (float)flash_used_size / (1024 * 1024));
    ESP_LOGI(TAG, "剩余容量（未分配）: %u 字节 (%.2f MB)", 
             flash_free_size, (float)flash_free_size / (1024 * 1024));

    // 后续原有逻辑（WiFi、摄像头、服务器初始化）
    wifi_init();

    esp_err_t cam_err = camera_init_custom();
    if (cam_err != ESP_OK) {
        ESP_LOGE(TAG, "摄像头初始化失败，无法提供视频流");
        return;
    }

    http_server_start();
    mqtt_app_start();
    
    ESP_LOGI(TAG, "ESP32-CAM项目启动完成");
}
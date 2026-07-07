#include "camera.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "camera";

#define CAMERA_PWDN_PIN GPIO_NUM_32

// 互斥信号量：防止 camera_capture() 和 camera_reinit() 并发访问
// 用信号量取代普通的 volatile bool，因为 bool 存在 TOCTOU 竞态
static SemaphoreHandle_t s_camera_mutex = NULL;

esp_err_t camera_init_custom(void) {
    camera_config_t config = {
        .pin_pwdn = 32,
        .pin_reset = -1,
        .pin_xclk = 0,
        .pin_sccb_sda = 26,
        .pin_sccb_scl = 27,
        .pin_d7 = 35,
        .pin_d6 = 34,
        .pin_d5 = 39,
        .pin_d4 = 36,
        .pin_d3 = 21,
        .pin_d2 = 19,
        .pin_d1 = 18,
        .pin_d0 = 5,
        .pin_vsync = 25,
        .pin_href = 23,
        .pin_pclk = 22,
        .xclk_freq_hz = 10000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_SVGA,
        .jpeg_quality = 30,
        .fb_count = 2,
        .grab_mode = CAMERA_GRAB_LATEST,
    };

    // 首次初始化时创建互斥信号量
    if (s_camera_mutex == NULL) {
        s_camera_mutex = xSemaphoreCreateBinary();
        if (s_camera_mutex == NULL) {
            ESP_LOGE(TAG, "创建摄像头互斥信号量失败");
            return ESP_FAIL;
        }
        // 初始状态：可用（give 一次，后续 camera_capture 可以 take）
        xSemaphoreGive(s_camera_mutex);
    }

    ESP_LOGI(TAG, "开始初始化摄像头...");
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "摄像头初始化失败: %s", esp_err_to_name(err));
        return err;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        ESP_LOGI(TAG, "摄像头型号: 0x%02X", s->id.PID);
        s->set_framesize(s, FRAMESIZE_VGA);
        s->set_quality(s, 30);
    }
    ESP_LOGI(TAG, "摄像头初始化成功");
    return ESP_OK;
}

camera_fb_t *camera_capture(void) {
    // 尝试拿信号量（0 tick 等待 = 拿不到立即返回）
    // 比 volatile bool 强在 原子性：拿信号量和后续 fb_get 之间不会被 deinit 打断
    if (xSemaphoreTake(s_camera_mutex, 0) != pdTRUE) {
        return NULL;
    }
    camera_fb_t *fb = esp_camera_fb_get();
    xSemaphoreGive(s_camera_mutex);
    return fb;
}

void camera_power_off(void)
{
    gpio_set_direction(CAMERA_PWDN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(CAMERA_PWDN_PIN, 1);
    ESP_LOGI(TAG, "摄像头已断电（PWDN 拉高）");
    vTaskDelay(pdMS_TO_TICKS(200));
}

esp_err_t camera_reinit(void)
{
    ESP_LOGI(TAG, "摄像头重新初始化（恢复 OTA 失败后的状态）");

    // 拿互斥信号量：阻塞等待所有正在执行的 camera_capture() 完成
    // 之后 camera_capture() 再调就会因信号量枯竭而返回 NULL
    xSemaphoreTake(s_camera_mutex, portMAX_DELAY);

    // 1. 销毁全部摄像头 FreeRTOS 资源（cam_task、队列、帧缓冲）
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_camera_deinit 失败: %s", esp_err_to_name(err));
    }
    vTaskDelay(pdMS_TO_TICKS(500));

    // 2. 全新初始化
    err = camera_init_custom();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "摄像头重新初始化失败");
        xSemaphoreGive(s_camera_mutex);
        return err;
    }

    xSemaphoreGive(s_camera_mutex);
    ESP_LOGI(TAG, "摄像头重新初始化成功");
    return ESP_OK;
}

#include "camera.h"
#include "esp_log.h"

static const char *TAG = "camera";

esp_err_t camera_init_custom(void) {
    // 使用AI-Thinker ESP32-CAM模块的引脚配置
    camera_config_t config = {
        .pin_pwdn = 32,
        .pin_reset = -1,  // 多数ESP32-CAM模块复位引脚为-1
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
        .pin_d0 = 5,      // 注意：ESP32-CAM模块的D0引脚是5而不是15
        .pin_vsync = 25,
        .pin_href = 23,
        .pin_pclk = 22,

        .xclk_freq_hz = 10000000,  // 降低XCLK频率至10MHz，提高稳定性
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_SVGA,
        /*FRAMESIZE_QVGA (320x240)，FRAMESIZE_VGA (640x480) ，FRAMESIZE_SVGA (800x600)，FRAMESIZE_XGA (1024x768)- 当前设置*/
        .jpeg_quality = 30,  // JPEG质量30-50，平衡质量与性能
        .fb_count = 2,       // 双缓冲减少卡顿
        .grab_mode = CAMERA_GRAB_LATEST,
    };
    
    ESP_LOGI(TAG, "开始初始化摄像头...");
    esp_err_t err = esp_camera_init(&config);  // 调用原生初始化函数
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "摄像头初始化失败: %s", esp_err_to_name(err));
        return err;
    }

    // 简化传感器配置，仅保留必要参数
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        ESP_LOGI(TAG, "摄像头型号: 0x%02X", s->id.PID);
        s->set_framesize(s, FRAMESIZE_VGA);  // 确保分辨率正确
        s->set_quality(s, 30);               // 同步JPEG质量
    }
    ESP_LOGI(TAG, "摄像头初始化成功");
    return ESP_OK;
}

camera_fb_t *camera_capture(void) {
    return esp_camera_fb_get();
}
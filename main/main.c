#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "wifi.h"
#include "camera.h"
#include "http_server.h"
#include "mqtt.h"

static const char *TAG = "cam_http_server";

// -------------------------- 5. 主函数（串联所有模块） --------------------------
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

    // 1. 初始化WiFi
    wifi_init();

    // 2. 初始化摄像头（依赖WiFi之后，避免资源竞争）
    esp_err_t cam_err = camera_init_custom();  // 调用重命名后的函数
    if (cam_err != ESP_OK) {
        ESP_LOGE(TAG, "摄像头初始化失败，无法提供视频流");
        return;  // 摄像头失败则无需启动服务器
    }

    // 3. 启动HTTP服务器
    http_server_start();
    
    // 4. 启动MQTT客户端
    mqtt_app_start();
    
    ESP_LOGI(TAG, "ESP32-CAM项目启动完成");
}
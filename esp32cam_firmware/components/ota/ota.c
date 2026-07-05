#include "ota.h"
#include "esp_log.h"

static const char *TAG = "ota";

esp_err_t ota_init(void)
{
    ESP_LOGI(TAG, "OTA 组件初始化完成");
    return ESP_OK;
}

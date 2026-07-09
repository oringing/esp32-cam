#include "esp_log.h"
#include "esp_wifi.h"
#include "camera.h"
#include "nvs_flash.h"
#include <string.h>
#include "mqtt.h"
#include "ota.h"

static const char *TAG = "mqtt_client";
static esp_mqtt_client_handle_t client;
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, (int)event_id);
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t) event_data;
    client = event->client;
    int msg_id;
    switch ((int)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, MQTT_CONTROL_TOPIC, 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        
        // 处理控制命令
        if (strncmp(event->data, "take_picture", event->data_len) == 0) {
            ESP_LOGI(TAG, "MQTT 触发拍照，请通过 HTTP /api/capture 获取图片");
        } else if (strncmp(event->data, "ota_update ", 11) == 0) {
            // 命令格式: ota_update http://192.168.5.100:8000/firmware.bin
            // 跳过 "ota_update " 前缀获取 URL
            const char *url = event->data + 11;
            int url_len = event->data_len - 11;
            if (url_len <= 0) {
                ESP_LOGE(TAG, "OTA 升级地址为空");
                return;
            }
            // 拷贝 URL 到临时缓冲区（确保 \0 结尾）
            char url_buf[256];
            int copy_len = (url_len < 255) ? url_len : 255;
            strncpy(url_buf, url, copy_len);
            url_buf[copy_len] = '\0';
            ESP_LOGI(TAG, "收到 OTA 升级指令，地址: %s", url_buf);
            camera_power_off();
            vTaskDelay(pdMS_TO_TICKS(50));
            esp_err_t ret = ota_start_download_async(url_buf);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "OTA 升级请求提交失败: %s", esp_err_to_name(ret));
            }
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = MQTT_BROKER_URI,
            },
        },
        .credentials = {
            .username = MQTT_USERNAME,
            .authentication = {
                .password = MQTT_PASSWORD,
            },
            .client_id = MQTT_CLIENT_ID,
        },
    };
    
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

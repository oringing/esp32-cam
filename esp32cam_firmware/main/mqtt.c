#include "esp_log.h"
#include "esp_wifi.h"
#include "camera.h"
#include "nvs_flash.h"
#include <string.h>
#include "mqtt.h"

static const char *TAG = "mqtt_client";
static esp_mqtt_client_handle_t client;
static bool streaming = false;
static TaskHandle_t stream_task_handle = NULL;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

// 视频流任务函数
static void video_stream_task(void *pvParameters)
{
    ESP_LOGI(TAG, "视频流任务已启动");
    const int frame_delay = 100; // 10 FPS

    while (streaming) {
        camera_fb_t *fb = camera_capture();
        if (fb) {
            // 发布图片数据到流主题
            esp_mqtt_client_publish(client, MQTT_STREAM_TOPIC, (const char*)fb->buf, fb->len, 0, 0);
            esp_camera_fb_return(fb);
        } else {
            ESP_LOGW(TAG, "获取摄像头画面失败");
        }
        vTaskDelay(pdMS_TO_TICKS(frame_delay));
    }

    ESP_LOGI(TAG, "视频流任务已停止");
    stream_task_handle = NULL;
    vTaskDelete(NULL);
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
        streaming = false;
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
            ESP_LOGI(TAG, "Taking picture...");
            camera_fb_t *fb = camera_capture();
            if (fb) {
                // 发布图片数据
                esp_mqtt_client_publish(client, MQTT_IMAGE_TOPIC, (const char*)fb->buf, fb->len, 0, 0);
                esp_camera_fb_return(fb);
            }
        } else if (strncmp(event->data, "start_stream", event->data_len) == 0) {
            ESP_LOGI(TAG, "Starting video stream...");
            start_video_stream();
        } else if (strncmp(event->data, "stop_stream", event->data_len) == 0) {
            ESP_LOGI(TAG, "Stopping video stream...");
            stop_video_stream();
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

void start_video_stream(void)
{
    if (!streaming) {
        streaming = true;
        BaseType_t result = xTaskCreate(video_stream_task, "video_stream", 4096, NULL, 5, &stream_task_handle);
        if (result != pdTRUE) {
            ESP_LOGE(TAG, "创建视频流任务失败");
            streaming = false;
        }
    }
}

void stop_video_stream(void)
{
    streaming = false;
    if (stream_task_handle) {
        vTaskDelete(stream_task_handle);
        stream_task_handle = NULL;
    }
}

void mqtt_app_start(void)
{
    // 每次启动时清除NVS并重新初始化
    ESP_LOGI(TAG, "Erasing and reinitializing NVS for MQTT");
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to erase NVS: %s", esp_err_to_name(err));
    }
    
    err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to init NVS: %s", esp_err_to_name(err));
    }

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

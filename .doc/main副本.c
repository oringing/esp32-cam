#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_camera.h"
#include "esp_http_server.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"

// -------------------------- 1. 配置参数（根据你的WiFi修改） --------------------------
#define WIFI_SSID "ChinaNet-Fbcn"
#define WIFI_PWD "afxefvhf"
static const char *TAG = "cam_http_server";
// 添加事件处理相关变量
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
static int s_retry_num = 0;
#define WIFI_MAXIMUM_RETRY 5

// -------------------------- 2. WiFi连接模块（带重连机制） --------------------------
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init(void) {
    s_wifi_event_group = xEventGroupCreate();
    
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // 注册事件处理函数
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handler,
                                        NULL,
                                        &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler,
                                        NULL,
                                        &instance_got_ip);

    // 连接WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PWD,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
    
    // 等待连接成功或失败
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi连接成功");
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "WiFi连接失败");
    } else {
        ESP_LOGE(TAG, "WiFi连接超时");
    }
}

// -------------------------- 3. 摄像头初始化模块 --------------------------
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
        .frame_size = FRAMESIZE_VGA,  // 640x480符合需求
        /*FRAMESIZE_QVGA (320x240)，FRAMESIZE_VGA (640x480) - 当前设置，FRAMESIZE_SVGA (800x600)，FRAMESIZE_XGA (1024x768)*/
        .jpeg_quality = 30,  // JPEG质量30-50，平衡质量与性能
        .fb_count = 2,       // 双缓冲减少卡顿
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
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


// -------------------------- 4. HTTP服务器模块（推送视频流） --------------------------
static esp_err_t video_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
    camera_fb_t *fb = NULL;
    esp_err_t err = ESP_OK;
    const int max_fps = 10;  // 目标10fps
    const int frame_delay = 1000 / max_fps;  // 100ms/帧

    ESP_LOGI(TAG, "视频流请求开始处理");

    int retry_count = 0;
    const int max_retries = 15;

    while (retry_count < max_retries) {
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGW(TAG, "获取帧数据失败，重试 %d/%d", retry_count+1, max_retries);
            retry_count++;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // 检查是否是有效的JPEG数据
        if (fb->len > 0 && fb->buf[0] == 0xFF && fb->buf[1] == 0xD8 && fb->buf[2] == 0xFF) {
            // 发送多部分数据头
            err = httpd_resp_send_chunk(req, "--frame\r\n", strlen("--frame\r\n"));
            if (err != ESP_OK) break;

            // 发送JPEG数据头（含长度）
            char header[128];
            snprintf(header, sizeof(header), 
                     "Content-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n", 
                     fb->len);
            err = httpd_resp_send_chunk(req, header, strlen(header));
            if (err != ESP_OK) break;

            // 发送帧数据
            err = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
            if (err != ESP_OK) break;

            // 发送帧尾
            err = httpd_resp_send_chunk(req, "\r\n", 2);
            if (err != ESP_OK) break;
            
            retry_count = 0; // 成功获取帧，重置重试计数
        } else {
            ESP_LOGW(TAG, "获取到无效的JPEG数据，长度: %zu", fb->len);
        }

        esp_camera_fb_return(fb);  // 及时释放帧缓冲
        vTaskDelay(pdMS_TO_TICKS(frame_delay));  // 控制帧率
    }

    if (fb) esp_camera_fb_return(fb);
    httpd_resp_send_chunk(req, NULL, 0);  // 结束响应
    ESP_LOGI(TAG, "视频流请求处理结束");
    return err;
}

// 创建HTTP服务器
void http_server_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 5;  // 限制并发连接，减少内存占用
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t video_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = video_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &video_uri);

        // v5.5兼容的IP地址获取方式
        esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (sta_netif != NULL) {
            esp_netif_ip_info_t ip_info;
            esp_netif_get_ip_info(sta_netif, &ip_info);
            ESP_LOGI(TAG, "局域网访问地址: http://" IPSTR, IP2STR(&ip_info.ip));
        } else {
            ESP_LOGE(TAG, "无法获取WiFi接口信息");
        }
    } else {
        ESP_LOGE(TAG, "HTTP服务器启动失败");
    }
}


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
    ESP_LOGI(TAG, "ESP32-CAM项目启动完成");
}
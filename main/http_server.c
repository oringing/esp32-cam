#include "http_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_netif.h"
#include <string.h>

static const char *TAG = "http_server";

static esp_err_t video_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
    camera_fb_t *fb = NULL;
    esp_err_t err = ESP_OK;
    const int max_fps = VIDEO_FPS;
    const int frame_delay = 1000 / max_fps;

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

void http_server_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_SERVER_PORT;
    config.max_open_sockets = HTTP_MAX_OPEN_SOCKETS;
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
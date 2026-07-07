#include "http_server.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "esp_netif.h"
#include <string.h>
#include "camera.h"
#include "mqtt.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "driver/ledc.h"
#include <math.h>

static const char *TAG = "http_server";
static volatile bool g_streaming_enabled = true;

/* ================================================================
 * 控制面板 HTML 页面
 * ================================================================ */
static const char INDEX_HTML[] = R"rawliteral(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32-CAM</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font:14px/1.5 system-ui,sans-serif;background:#1a1a2e;color:#eee;min-height:100vh}
header{background:#16213e;padding:12px 16px;display:flex;align-items:center;justify-content:space-between}
header h1{font-size:18px;color:#0ff}
.badge{font-size:11px;background:#0f3460;padding:4px 10px;border-radius:12px}
main{max-width:640px;margin:0 auto;padding:12px}
.video-box{position:relative;background:#000;border-radius:8px;overflow:hidden;margin-bottom:12px}
.video-box img{width:100%;display:block}
.video-box .overlay{position:absolute;top:8px;left:8px;font-size:11px;background:rgba(0,0,0,.6);padding:2px 8px;border-radius:4px}
/* --- 35%+10%+55% 单行控制栏 --- */
.control-row{display:flex;align-items:center;background:#16213e;border-radius:8px;padding:10px 14px;margin-bottom:12px}
.stream-section{display:flex;align-items:center;gap:6px;width:35%;flex-shrink:0}
.ctrl-gap{width:10%;flex-shrink:0}
.led-section{display:flex;align-items:center;gap:8px;width:55%;flex-shrink:0}
.ctrl-label{font-size:13px;color:#ccc;white-space:nowrap}
.switch{position:relative;display:inline-block;width:46px;height:26px}
.switch input{opacity:0;width:0;height:0}
.slider-knob{position:absolute;cursor:pointer;top:0;left:0;right:0;bottom:0;background:#333;border-radius:26px;transition:.3s}
.slider-knob:before{position:absolute;content:"";height:20px;width:20px;left:3px;bottom:3px;background:#ddd;border-radius:50%;transition:.3s}
input:checked+.slider-knob{background:#0ff}
input:checked+.slider-knob:before{transform:translateX(20px)}
.toggle-state{font-size:12px;color:#888;min-width:18px;text-align:center}
/* --- 滑块: 青色填充轨道 --- */
input[type=range]{-webkit-appearance:none;appearance:none;width:100%;height:6px;border-radius:3px;outline:none;cursor:pointer;background:linear-gradient(to right,#0ff 0%,#333 0%)}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;background:#0ff;border-radius:50%;cursor:pointer;border:none;box-shadow:0 0 4px rgba(0,255,255,.4)}
input[type=range]::-moz-range-thumb{width:18px;height:18px;background:#0ff;border-radius:50%;cursor:pointer;border:none}
.brightness-value{font-size:13px;color:#0ff;min-width:36px;text-align:right}
/* --- 其余 --- */
button{border:none;border-radius:6px;padding:10px;font-size:13px;cursor:pointer;font-weight:600;transition:opacity .2s}
button:active{opacity:.7}
.btn-action{background:#1a1a2e;color:#ccc;border:1px solid #333}
.section{background:#16213e;border-radius:8px;padding:14px;margin-bottom:12px}
.section h2{font-size:15px;margin-bottom:10px;color:#0ff}
input[type=file]{font-size:12px;color:#aaa;margin-bottom:8px;width:100%}
.progress-bar{height:6px;background:#0f3460;border-radius:3px;margin-top:8px;overflow:hidden;display:none}
.progress-bar .fill{height:100%;background:#0ff;width:0;transition:width .3s}
#status{font-size:11px;color:#888;text-align:center;margin-top:8px}
footer{text-align:center;font-size:11px;color:#555;padding:16px}
</style>
</head>
<body>
<header>
<h1>ESP32-CAM</h1>
<span class="badge" id="connStatus">● 在线</span>
</header>
<main>
<div class="video-box">
<img id="stream" src="/stream" onerror="this.style.display='none';document.getElementById('streamStatus').textContent='(等待...)'">
<span class="overlay" id="streamStatus">LIVE</span>
</div>
<div class="control-row">
<div class="stream-section">
<span class="ctrl-label">视频流</span>
<span class="toggle-state" id="streamState">开</span>
<label class="switch">
<input type="checkbox" id="streamSwitch" checked onchange="toggleStream()">
<span class="slider-knob"></span>
</label>
</div>
<div class="ctrl-gap"></div>
<div class="led-section">
<span class="ctrl-label">补光灯</span>
<input type="range" id="ledSlider" min="0" max="100" value="0" oninput="onLedInput(this.value)">
<span class="brightness-value" id="ledValue">0%</span>
</div>
</div>
<div class="section">
<h2>固件升级 (OTA)</h2>
<p style="font-size:11px;color:#888;margin-bottom:8px">选择 .bin 文件，点击升级</p>
<form id="otaForm" onsubmit="uploadFirmware(event)">
<input type="file" id="fwFile" accept=".bin">
<button type="submit" class="btn-action" style="width:100%">⬆ 上传并升级</button>
</form>
<div class="progress-bar" id="progressBar"><div class="fill" id="progressFill"></div></div>
<div id="progressText" style="font-size:11px;color:#0ff;margin-top:4px;display:none"></div>
</div>
<div class="section">
<h2>快捷操作</h2>
<button class="btn-action" style="width:100%" onclick="apiCapture()">📷 拍照 (下载 JPEG)</button>
</div>
<div id="status">就绪</div>
</main>
<footer>esp32cam.local:8080</footer>
<script>
var streamOn=true;
function setStatus(msg,isOk){var s=document.getElementById("status");s.textContent=msg;s.style.color=isOk?"#0ff":"#f88";if(isOk)setTimeout(function(){s.textContent="就绪";s.style.color="#888"},3000)}
function toggleStream(){var el=document.getElementById("streamSwitch");var st=document.getElementById("streamState");var act=el.checked?"start":"stop";fetch("/api/stream?action="+act).then(function(r){return r.text()}).then(function(t){streamOn=el.checked;st.textContent=el.checked?"开":"关";st.style.color=el.checked?"#0ff":"#888";setStatus(t,true)}).catch(function(e){el.checked=!el.checked;setStatus("视频流切换失败",false)})}
/* ---- 补光灯: 实时输入 + 80ms 节流 ---- */
function updateSliderTrack(v){var s=document.getElementById("ledSlider");s.style.background="linear-gradient(to right, #0ff "+v+"%, #333 "+v+"%)"}
var ledThrottling=false,ledQueued=null;
function onLedInput(v){document.getElementById("ledValue").textContent=v+"%";updateSliderTrack(v);if(!ledThrottling){ledThrottling=true;sendLed(v);setTimeout(function(){ledThrottling=false;if(ledQueued!==null){var qv=ledQueued;ledQueued=null;onLedInput(qv)}},80)}else{ledQueued=v}}
function sendLed(v){fetch("/api/led?brightness="+v).then(function(r){return r.text()}).then(function(t){setStatus(t,true)}).catch(function(e){setStatus("LED请求失败",false)})}
function apiCapture(){fetch("/api/capture").then(function(r){if(!r.ok)throw new Error("HTTP "+r.status);return r.blob()}).then(function(b){var a=document.createElement("a");a.href=URL.createObjectURL(b);a.download="capture.jpg";a.click();setStatus("拍照成功",true)}).catch(function(e){setStatus("拍照失败: "+e.message,false)})}
var isUploading=false;
function uploadFirmware(e){
  e.preventDefault();
  if(isUploading)return;
  var f=document.getElementById("fwFile").files[0];
  if(!f){document.getElementById("status").textContent="请先选择固件文件";return}
  if(!confirm("确定要升级到 "+f.name+" ?"))return;
  isUploading=true;
  var bar=document.getElementById("progressBar");
  var fill=document.getElementById("progressFill");
  var txt=document.getElementById("progressText");
  bar.style.display="block";txt.style.display="block";
  var fd=new FormData();fd.append("firmware",f);
  var xhr=new XMLHttpRequest();
  xhr.open("POST","/upload");
  xhr.upload.onprogress=function(ev){
    if(ev.lengthComputable){
      var pct=Math.round(ev.loaded/ev.total*100);
      fill.style.width=pct+"%";txt.textContent="上传: "+pct+"%"
    }
  };
  xhr.onload=function(){
    if(xhr.status===200){
      fill.style.width="100%";txt.textContent="上传完成, 正在升级...";
      var evt=new EventSource("/progress");
      evt.onmessage=function(me){
        var d=JSON.parse(me.data);
        if(d.percent!==undefined){
          fill.style.width=d.percent+"%";txt.textContent="升级: "+d.percent+"%"
        }
        if(d.done){evt.close();txt.textContent="升级完成! 即将重启...";isUploading=false}
      };
      evt.onerror=function(){evt.close();txt.textContent="SSE断开, 设备可能已重启";isUploading=false}
    }else{
      txt.textContent="上传失败: HTTP "+xhr.status;isUploading=false
    }
  };
  xhr.onerror=function(){txt.textContent="上传失败: 网络错误";isUploading=false};
  xhr.send(fd)
}
</script>
</body>
</html>)rawliteral";

/* ================================================================
 * 路由处理器
 * ================================================================ */

static esp_err_t page_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
    return ESP_OK;
}

static esp_err_t stream_handler(httpd_req_t *req);
static void stream_task(void *arg);

// === 异步 MJPEG 推流任务 (在独立 FreeRTOS 任务中运行, 不阻塞 HTTP Server) ===
static void stream_task(void *arg) {
    httpd_req_t *req = (httpd_req_t *)arg;

    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
    camera_fb_t *fb = NULL;
    const int frame_delay = 1000 / VIDEO_FPS;
    int retry_count = 0;
    const int max_retries = 15;

    ESP_LOGI(TAG, "MJPEG 推流任务启动");

    while (retry_count < max_retries) {
        if (!g_streaming_enabled) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        fb = camera_capture();
        if (!fb) {
            ESP_LOGW(TAG, "获取帧失败, 重试 %d/%d", retry_count + 1, max_retries);
            retry_count++;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (fb->len > 0 && fb->buf[0] == 0xFF && fb->buf[1] == 0xD8 && fb->buf[2] == 0xFF) {
            if (httpd_resp_send_chunk(req, "--frame\r\n", strlen("--frame\r\n")) != ESP_OK) break;

            char header[128];
            snprintf(header, sizeof(header),
                     "Content-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n", fb->len);
            if (httpd_resp_send_chunk(req, header, strlen(header)) != ESP_OK) break;
            if (httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len) != ESP_OK) break;
            if (httpd_resp_send_chunk(req, "\r\n", 2) != ESP_OK) break;
            retry_count = 0;
        } else {
            ESP_LOGW(TAG, "无效 JPEG, len=%zu", fb->len);
        }

        esp_camera_fb_return(fb);
        fb = NULL;
        vTaskDelay(pdMS_TO_TICKS(frame_delay));
    }

    if (fb) esp_camera_fb_return(fb);
    httpd_resp_send_chunk(req, NULL, 0);
    httpd_req_async_handler_complete(req);
    ESP_LOGI(TAG, "MJPEG 推流任务结束");
    vTaskDelete(NULL);
}

// === 入口: 异步启动 MJPEG 推流 (创建独立任务后立即返回) ===
static esp_err_t stream_handler(httpd_req_t *req) {
    httpd_req_t *async_req = NULL;
    esp_err_t err = httpd_req_async_handler_begin(req, &async_req);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "async_handler_begin 失败: %s", esp_err_to_name(err));
        return err;
    }

    if (xTaskCreate(stream_task, "mjpeg_stream", 4096, async_req, 5, NULL) != pdTRUE) {
        ESP_LOGE(TAG, "创建 MJPEG 推流任务失败");
        httpd_req_async_handler_complete(async_req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "MJPEG 推流任务已创建, HTTP Server 任务已释放");
    return ESP_OK;
}
// GET /api/stream?action=start|stop
static esp_err_t api_stream_handler(httpd_req_t *req) {
    char action[16] = {0};
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        char *buf = malloc(buf_len);
        if (buf) {
            httpd_req_get_url_query_str(req, buf, buf_len);
            httpd_query_key_value(buf, "action", action, sizeof(action));
            free(buf);
        }
    }

    ESP_LOGI(TAG, "[API] 视频流控制: action='%s'", action);

    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    if (strcmp(action, "start") == 0) {
        g_streaming_enabled = true;
        httpd_resp_send(req, "视频流已开启", -1);
    } else if (strcmp(action, "stop") == 0) {
        g_streaming_enabled = false;
        httpd_resp_send(req, "视频流已暂停", -1);
    } else {
        httpd_resp_send(req, "无效操作, 请用 ?action=start 或 ?action=stop", -1);
    }
    return ESP_OK;
}

// GET /api/led?brightness=0~100 — PWM 亮度 (GPIO 4, 低电平驱动, Gamma 2.2)
static esp_err_t api_led_handler(httpd_req_t *req) {
    char val_str[8] = {0};
    int brightness = 0;
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        char *buf = malloc(buf_len);
        if (buf) {
            httpd_req_get_url_query_str(req, buf, buf_len);
            if (httpd_query_key_value(buf, "brightness", val_str, sizeof(val_str)) == ESP_OK) {
                brightness = atoi(val_str);
                if (brightness < 0) brightness = 0;
                if (brightness > 100) brightness = 100;
            }
            free(buf);
        }
    }

    // Gamma 2.2 + 反转 (GPIO 低电平=亮)
    float normalized = brightness / 100.0f;
    float gamma = powf(normalized, 2.2f);
    // 0%=1023(HIGH=灭), 100%=0(LOW=最亮)
    uint32_t duty = (uint32_t)(gamma * 1023.0f);  // 硬件已反相, 无需软件反转

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);

    ESP_LOGI(TAG, "[API] 亮度: %d%% -> gamma=%.3f -> duty=%lu", brightness, gamma, duty);

    char resp[32];
    snprintf(resp, sizeof(resp), "补光灯: %d%%", brightness);
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    httpd_resp_send(req, resp, -1);
    return ESP_OK;
}

// GET /api/capture
static esp_err_t api_capture_handler(httpd_req_t *req) {
    camera_fb_t *fb = camera_capture();
    if (!fb) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=capture.jpg");
    httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return ESP_OK;
}

/* ================================================================
 * HTTP 服务器启动
 * ================================================================ */
void http_server_start(void) {
    // --- LEDC PWM 初始化 (GPIO 4, 低电平驱动, 默认灭) ---
    // 摄像头占用 LEDC_TIMER_0/CHANNEL_0(XCLK) → 使用 TIMER_1/CHANNEL_1
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_1,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .gpio_num = 4,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .timer_sel = LEDC_TIMER_1,
        .duty = 0,    // 1023全高电平LED 亮 ， 0为低电平LED 熄灭 (安全默认)
        .hpoint = 0,
        .flags = { .output_invert = 0 },
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_LOGI(TAG, "LEDC PWM 就绪: GPIO4, 5kHz, 默认灭 (duty=0)");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = HTTP_SERVER_PORT;
    config.max_open_sockets = HTTP_MAX_OPEN_SOCKETS;
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "HTTP 服务器启动失败");
        return;
    }

    httpd_uri_t uri_page = {
        .uri = "/", .method = HTTP_GET,
        .handler = page_handler, .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_page);

    httpd_uri_t uri_stream = {
        .uri = "/stream", .method = HTTP_GET,
        .handler = stream_handler, .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_stream);

    httpd_uri_t uri_api_stream = {
        .uri = "/api/stream", .method = HTTP_GET,
        .handler = api_stream_handler, .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_api_stream);

    httpd_uri_t uri_api_led = {
        .uri = "/api/led", .method = HTTP_GET,
        .handler = api_led_handler, .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_api_led);

    httpd_uri_t uri_api_capture = {
        .uri = "/api/capture", .method = HTTP_GET,
        .handler = api_capture_handler, .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_api_capture);

    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif != NULL) {
        esp_netif_ip_info_t ip_info;
        esp_netif_get_ip_info(sta_netif, &ip_info);
        ESP_LOGI(TAG, "面板: http://" IPSTR ":%d  /  http://esp32cam.local:%d",
                 IP2STR(&ip_info.ip), HTTP_SERVER_PORT, HTTP_SERVER_PORT);
    }
}
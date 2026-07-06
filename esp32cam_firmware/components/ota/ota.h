#ifndef OTA_H
#define OTA_H

#include "esp_err.h"

/**
 * @brief 初始化 OTA 组件，创建专用 OTA 任务和信号量
 * @return ESP_OK 成功
 */
esp_err_t ota_init(void);

/**
 * @brief 异步触发 OTA 升级（非阻塞）
 *        解析 URL 后立即返回，下载在专用 OTA 任务中执行
 * @param url 固件下载地址，如 "http://192.168.5.100:8000/firmware.bin"
 * @return ESP_OK 表示已接收升级请求
 */
esp_err_t ota_start_download_async(const char *url);

#endif

#ifndef OTA_H
#define OTA_H

#include "esp_err.h"

/**
 * @brief OTA 完成回调函数类型
 * @param result ESP_OK 表示升级成功（设备即将重启），其他值表示失败
 * @param user_ctx 用户上下文指针（由 ota_set_complete_callback 传入）
 */
typedef void (*ota_complete_cb_t)(esp_err_t result, void *user_ctx);

/**
 * @brief 初始化 OTA 组件，创建专用 OTA 任务和信号量
 * @return ESP_OK 成功
 */
esp_err_t ota_init(void);

/**
 * @brief 注册 OTA 完成回调
 *        无论 OTA 成功还是失败，完成任务后都会调用此回调
 * @param cb 回调函数指针，传 NULL 取消注册
 * @param user_ctx 用户上下文，回调时原样传回
 */
void ota_set_complete_callback(ota_complete_cb_t cb, void *user_ctx);

/**
 * @brief 异步触发 OTA 升级（非阻塞）
 *        解析 URL 后立即返回，下载在专用 OTA 任务中执行
 * @param url 固件下载地址，如 "http://192.168.5.100:8000/firmware.bin"
 * @return ESP_OK 表示已接收升级请求
 */
esp_err_t ota_start_download_async(const char *url);

#endif

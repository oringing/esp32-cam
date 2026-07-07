#ifndef CAMERA_H
#define CAMERA_H

#include "esp_camera.h"

esp_err_t camera_init_custom(void);
camera_fb_t *camera_capture(void);

/**
 * @brief 断电摄像头传感器
 */
void camera_power_off(void);

/**
 * @brief 重新初始化摄像头（OTA 失败后恢复用）
 */
esp_err_t camera_reinit(void);

#endif

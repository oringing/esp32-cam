#ifndef CAMERA_H
#define CAMERA_H

#include "esp_camera.h"

esp_err_t camera_init_custom(void);
camera_fb_t *camera_capture(void);

/**
 * @brief 断电摄像头传感器
 *        通过 PWDN 引脚（GPIO32）拉高切断供电，I2S DMA 停止，释放 PSRAM 总线。
 *        系统重启后 app_main() 中的 camera_init_custom() 会恢复供电并重新初始化。
 */
void camera_power_off(void);

#endif

#ifndef CAMERA_H
#define CAMERA_H

#include "esp_camera.h"

esp_err_t camera_init_custom(void);
camera_fb_t *camera_capture(void);

#endif
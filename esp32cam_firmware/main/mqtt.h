#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

#include "esp_err.h"
#include <mqtt_client.h>

// MQTT客户端配置宏定义
#define MQTT_BROKER_URI "mqtt://192.168.94.176:1883"
#define MQTT_USERNAME "esp32cam"
#define MQTT_PASSWORD "123456"
#define MQTT_CLIENT_ID "esp32cam_client"
#define MQTT_CONTROL_TOPIC "camera/control"
#define MQTT_IMAGE_TOPIC "camera/image"

void mqtt_app_start(void);

#endif
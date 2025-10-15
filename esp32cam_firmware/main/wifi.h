#ifndef WIFI_H
#define WIFI_H

// // WiFi配置
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PWD "YOUR_WIFI_PASSWORD" //若 WiFi 无密码，将WIFI_PWD ""引号内留空即可

// WiFi连接配置
#define WIFI_MAXIMUM_RETRY 5

// WiFi事件位定义
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

void wifi_init(void);

#endif
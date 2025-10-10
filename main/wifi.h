#ifndef WIFI_H
#define WIFI_H

// WiFi配置
#define WIFI_SSID "Redmi Note 12 Turbo"
#define WIFI_PWD "88888888"

// WiFi连接配置
#define WIFI_MAXIMUM_RETRY 5

// WiFi事件位定义
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

void wifi_init(void);

#endif
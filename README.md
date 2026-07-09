# ESP32-CAM 嵌入式网络摄像头

> 基于 ESP32-CAM（OV2640 + PSRAM）的嵌入式网络摄像头系统。提供 Web 控制面板实现实时 MJPEG 视频流、LED 补光灯调节、一键拍照，支持 A/B 分区 OTA 固件升级与 Bootloader 自动回滚，同时具备 MQTT 远程指令通道。

---

**Web 控制面板**

![网页控制面板](docs/images/网页控制面板.png)

浏览器访问 `http://esp32cam.local:8080`，即开即用。实时 MJPEG 画面（640×480 @ 30fps）、视频流启停开关、Gamma 校正补光灯滑块（5kHz PWM / 10-bit / Gamma 2.2）、固件升级（上传进度条 + SSE 状态推送），预留一键拍照下载功能。

---

## 核心功能

### 1. 双分区 OTA 固件升级与自动回滚
- 支持Web端可视化升级，实时展示上传与进度，通过SSE推送状态
- 提供本地Web上传并预留MQTT远程URL触发，覆盖不同运维场景
- 采用ota_0/ota_1双分区设计，Bootloader自动校验固件，异常自动回滚
- 升级异常可自动恢复摄像头运行，无需手动重启，保障设备持续可用

### 2. 异步 MJPEG 视频流架构
- 解决HTTP Server单任务导致的长连接阻塞控制请求的问题
- 基于FreeRTOS创建独立推流任务，与主HTTP服务完全解耦并行运行
- 视频流与控制指令互不干扰，确保控制操作保持毫秒级响应速度
- 遵循标准MJPEG流协议，主流浏览器无需额外插件即可直接播放

### 3. Web 可视化控制面板
- 前端资源完整内嵌固件，无需单独部署前端工程，开箱即可使用
- 提供实时MJPEG视频预览，支持一键启停流，画面稳定传输
- 补光灯采用PWM调光，加入节流防抖与Gamma 2.2校正，调光均匀自然

### 4. 远程控制与硬件安全机制
- 提供轻量级MQTT控制通道用于后续扩展，仅传输控制指令，网络带宽占用极低
- 引入二值信号量保护摄像头资源，避免并发访问引发内存越界异常
- 关键路径异常容错机制，覆盖 WiFi断连、OTA 升级异常等核心场景

### 扩展方向
- Web 配网与参数持久化：新增AP模式Web配网，替换硬编码的WiFi/MQTT凭证，配置存入NVS，设备重启自动保留。
- OTA 流式写入优化：将Web OTA改为流式边收边写模式，省去固件全量PSRAM缓存，降低内存峰值占用。
- MQTT 远程指令扩展：扩展MQTT指令集，新增补光灯调光、设备状态上报，全程仅传输控制指令字符串。

---

## 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│                    浏览器 (esp32cam.local:8080)             │
│    MJPEG 视频流   │  API 控制 (LED调光/视频开关)  │ OTA 上传  │
└──────────────────┼──────────────────────────────┼───────────┘
                   ▼                              ▼
┌──────────────────────────────────────────────────────────────┐
│                    HTTP Server (端口 8080)                    │
│  ┌───────────────────┐  ┌──────────────────────────────────┐  │
│  │   同步路由 /api/*  │  │  异步 MJPEG 推流 /stream          │ │
│  │   POST /upload     │  │  FreeRTOS 独立任务               │  │
│  │   GET /progress    │  │  (不阻塞 HTTP Server)            │  │
│  └─────────┬─────────┘  └────────────────┬─────────────────┘  │
└────────────┼─────────────────────────────┼────────────────────┘
             │                             │
    ┌────────┼──────────┐                  │
    ▼        ▼          ▼                  │ 
┌───────┐ ┌──────┐ ┌──────────┐            │
│Camera │ │LEDC  │ │ OTA 分区 │            │
│OV2640 │ │PWM   │ │ ota_0    │            │
│Semaph.│ │GPIO4 │ │ ota_1    │            │
└───┬───┘ └──────┘ └────┬─────┘            │
    │                   │                  │
    └────────┬──────────┘                  │
             │                             │
             ▼                             │
┌──────────────────────────┐   ┌───────────┴────────────┐
│   WiFi STA + mDNS        │   │  MQTT Client           │
│   esp32cam.local         │   │  (纯控制通道, 不传数据)  │
│   自动重连 (5次)          │   │  take_picture /        │
│                          │   │  ota_update <url>      │
└──────────────────────────┘   └────────────────────────┘
```

**协议分工：**

| 协议 | 传输内容 | 设计原则 |
|------|----------|----------|
| **HTTP** (端口 8080) | MJPEG 视频流、控制 API、OTA 上传、SSE 进度 | 大数据通道，浏览器直连 |
| **MQTT** (1883) | 控制命令字符串 | 轻量指令下发，不传图像数据 |

---

## 技术栈

| 层级 | 技术 | 说明 |
|------|------|------|
| 实时系统 | FreeRTOS（ESP-IDF v5.5） | 任务调度、信号量、事件组 |
| 硬件驱动 | OV2640 + LEDC PWM + GPIO | XCLK 10MHz、PWM 5kHz / 10-bit / Gamma 2.2 |
| 网络通信 | WiFi STA + LwIP + mDNS + HTTP Server | 异步 handler、mDNS 主机名 `esp32cam` |
| 应用协议 | HTTP/1.1 + MQTT | multipart/form-data 解析、SSE 推送 |
| 存储 | 4MB SPI Flash + 8MB PSRAM | OTA A/B 双分区、NVS |


---

## 快速上手

```bash
# 1. 进入固件工程目录
cd esp32cam_firmware

# 2. 配置 WiFi 和 MQTT 参数
# 编辑 main/wifi.h: WIFI_SSID / WIFI_PWD
# 编辑 main/mqtt.h: MQTT_BROKER_URI / MQTT_USERNAME / MQTT_PASSWORD

# 3. 编译、烧录、监视串口
idf.py set-target esp32
idf.py build
idf.py -p COM7 flash monitor
```

> **注意**：仅支持 2.4GHz WiFi。ESP32-CAM 没有 USB 转串口芯片，需接下载扩展版或者 USB-TTL（3.3V）。

### 使用

1. 设备启动后，串口打印 IP 地址
2. 浏览器打开 `http://esp32cam.local:8080` 或打印的 IP
3. 首次使用建议测试 OTA 升级流程验证功能完整性

### 固件工程结构

```
esp32cam_firmware/
├── main/                      # 应用源码
│   ├── main.c                 # 启动流程、Flash 容量诊断、健康自检
│   ├── camera.c / .h          # OV2640 驱动：Semaphore 防竞态、断电/重初始化
│   ├── http_server.c / .h     # HTTP Server：异步 MJPEG、API、OTA 上传+SSE
│   ├── wifi.c / .h            # WiFi STA + mDNS：EventGroup 同步、自动重连
│   ├── mqtt.c / .h            # MQTT Client：远程指令接收（控制通道）
│   └── CMakeLists.txt
├── components/
│   ├── ota/                   # OTA 组件：HTTP 下载 + Flash 写入 + 分区切换
│   └── esp32-camera/          # 摄像头驱动（espressif 官方组件）
├── managed_components/
│   └── espressif__mdns/       # mDNS 服务（espressif 官方组件）
├── partitions.csv             # 自定义分区表：ota_0 + ota_1 各约 1.31MB
└── docs/
    ├── 网页控制面板调试记录.md   # HTTP 阻塞问题排查全过程
    └── 坏固件回滚.md           # OTA 回滚验证日志与分析
```

---

## 其他信息

- **硬件**：AI-Thinker ESP32-CAM（ESP32 + OV2640 + 8MB PSRAM + 4MB Flash）
- **开发环境**：window11、VS Code + ESP-IDF v5.5 插件
- **参考**：ESP-IDF 官方示例（`camera_web_server`、`native_ota`）、espressif/esp32-camera 组件

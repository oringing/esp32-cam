idf_component_register(SRCS "mqtt.c" "http_server.c" "camera.c" "wifi.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi nvs_flash mqtt esp_http_server esp32-camera spi_flash)
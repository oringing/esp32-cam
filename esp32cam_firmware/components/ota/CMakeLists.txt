idf_component_register(SRCS "ota.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_wifi nvs_flash mqtt esp_http_server)

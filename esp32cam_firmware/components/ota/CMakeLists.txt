idf_component_register(SRCS "ota.c"
                    INCLUDE_DIRS "."
                    REQUIRES app_update esp_http_client)

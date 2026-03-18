idf_component_register(
                    SRCS "sagent.c" "sagent_tool.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_http_client mbedtls nvs_flash)
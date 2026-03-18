idf_component_register(SRCS "esp32_s3_cd.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES nvs_flash esp_wifi esp_driver_gpio driver
                    REQUIRES sagent ssd1306_driver
)

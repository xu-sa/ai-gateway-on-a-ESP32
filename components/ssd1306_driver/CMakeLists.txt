idf_component_register(
                    SRCS "ssd1306_driver.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES driver esp_driver_gpio 
                    REQUIRES font)
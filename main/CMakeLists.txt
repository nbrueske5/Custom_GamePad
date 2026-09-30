idf_component_register(SRCS "Communication.c" "main.c"
                            "LCD_Display.c"
                            "Pixel_Array_Presets.c"
                            "temp_sensor.c"
                       PRIV_REQUIRES spi_flash
                       REQUIRES driver
                       REQUIRES esp_lcd
                       REQUIRES esp_adc
                       REQUIRES esp_wifi
                       REQUIRES nvs_flash
                       INCLUDE_DIRS "")

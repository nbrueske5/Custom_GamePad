idf_component_register(SRCS "tiny_usb.c" "Communication.c" "main.c"
                            "LCD_Display.c"
                            "Pixel_Array_Presets.c"
                            "temp_sensor.c"
                            "rtos_tasks.c"
                       PRIV_REQUIRES spi_flash
                       REQUIRES driver
                       REQUIRES esp_lcd
                       REQUIRES esp_adc
                       REQUIRES esp_wifi
                       REQUIRES nvs_flash
                       REQUIRES espressif__esp_tinyusb
                       INCLUDE_DIRS "")

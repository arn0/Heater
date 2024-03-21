idf_component_register(SRCS "monitor.c" "clock.c" "real_time_stats_example_main.c" "spi_lcd.c" "lvgl_ui.c" "control_task.c" "webmdns.c" "rgb_led.c" "heater_task.c" "webserver.c" "wifi_station.c" "main.c"
                       INCLUDE_DIRS ".")

spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)

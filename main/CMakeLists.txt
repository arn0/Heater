idf_component_register(SRCS "json.cpp" "heater.c" "control.c" "real_time_stats.c" "logger.c" "mount.c" "sntp.c" "monitor.c" "clock.c" "spi_lcd.c" "lvgl_ui.c" "webmdns.c" "rgb_led.c" "webserver.c" "wifi_station.c" "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES nvs_flash spiffs esp_wifi esp_http_server pzem004tv3 ArduinoJson
                       )

spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)

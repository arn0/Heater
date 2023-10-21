idf_component_register(SRCS "rgb_led.c" "heater_task.c" "webserver.c" "wifi_station.c" "main.c"
                       INCLUDE_DIRS ".")

spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)
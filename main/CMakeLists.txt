idf_component_register(SRCS "real_time_stats_example_main.c" "spi_lcd.c" "lvgl_ui.c" "control_task.c" "webmdns.c" "t_monitor_task.c" "rgb_led.c" "heater_task.c" "webserver.c" "wifi_station.c" "main.c"
                       INCLUDE_DIRS ".")

spiffs_create_partition_image(storage ../data FLASH_IN_PROJECT)

idf_build_set_property(COMPILE_OPTIONS "-DLV_CONF_INCLUDE_SIMPLE=1" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-DLV_CONF_PATH= ${CMAKE_SOURCE_DIR}/main/lv_conf.h" APPEND)
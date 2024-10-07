#ifndef LVGL_UI_H
#define LVGL_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_EXAMPLE_ENABLE_LCD

extern bool wifi_connected;
extern bool stats_read_postponed;
void lvgl_ui_update(void);
esp_err_t stats_save();
esp_err_t stats_read();

#endif // CONFIG_EXAMPLE_ENABLE_LCD

#ifdef __cplusplus
}
#endif

#endif // LVGL_UI_H
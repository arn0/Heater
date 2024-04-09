#ifndef LVGL_UI_H
#define LVGL_UI_H
#ifdef __cplusplus

extern "C" {
#endif

extern bool wifi_connected;
extern bool stats_read_postponed;
void lvgl_ui_update(void);
esp_err_t stats_save();
esp_err_t stats_read();

#ifdef __cplusplus
}
#endif

#endif // LVGL_UI_H
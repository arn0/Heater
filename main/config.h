#ifndef HEATER_CONFIG_H
#define HEATER_CONFIG_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int day_start_minutes;
    int night_start_minutes;
    float day_temperature;
    float night_temperature;
    float floor_temperature;
    bool night_enabled;
    int preheat_min_minutes;
    int preheat_max_minutes;
    float warmup_rate_c_per_min;
    float stage_full_delta;
    float stage_single_delta;
    float stage_hold_delta;
    int override_duration_minutes;
} heater_config_t;

const heater_config_t *heater_config_get(void);
const heater_config_t *heater_config_defaults(void);
heater_config_t *heater_config_mutable(void);
esp_err_t heater_config_load(void);
esp_err_t heater_config_save(void);
esp_err_t heater_config_apply(const heater_config_t *config, bool persist);
esp_err_t heater_config_apply_json(const char *json_payload);
int heater_config_minutes_from_string(const char *hhmm);
void heater_config_string_from_minutes(int minutes, char *buffer, size_t buffer_len);

#ifdef __cplusplus
}
#endif

#endif // HEATER_CONFIG_H

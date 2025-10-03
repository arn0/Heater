#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "control.h"
#include "heater.h"
#include "task_priorities.h"
#include "config.h"

static const char *TAG = "control_task";

static int minutes_since_midnight(const struct tm *timeinfo) {
    return timeinfo->tm_hour * 60 + timeinfo->tm_min;
}

static int minutes_until(int now_minutes, int target_minutes) {
    int diff = target_minutes - now_minutes;
    if (diff < 0) {
        diff += 24 * 60;
    }
    return diff;
}

static float clamp_floor(float value, float floor_value) {
    return value < floor_value ? floor_value : value;
}

static void update_schedule_state(const heater_config_t *cfg, time_t now) {
    struct tm local_time = {0};
    localtime_r(&now, &local_time);
    const int current_minutes = minutes_since_midnight(&local_time);

    bool is_day = true;
    int minutes_to_transition = 0;

    if (cfg->night_enabled) {
        if (cfg->day_start_minutes == cfg->night_start_minutes) {
            is_day = true;
            minutes_to_transition = 24 * 60;
        } else if (cfg->day_start_minutes < cfg->night_start_minutes) {
            is_day = current_minutes >= cfg->day_start_minutes && current_minutes < cfg->night_start_minutes;
            minutes_to_transition = minutes_until(current_minutes, is_day ? cfg->night_start_minutes : cfg->day_start_minutes);
        } else {
            // Day period wraps around midnight
            is_day = current_minutes >= cfg->day_start_minutes || current_minutes < cfg->night_start_minutes;
            minutes_to_transition = minutes_until(current_minutes, is_day ? cfg->night_start_minutes : cfg->day_start_minutes);
        }
    } else {
        is_day = true;
        minutes_to_transition = 24 * 60;
    }

    float scheduled_target = is_day ? cfg->day_temperature : cfg->night_temperature;
    heater_status.scheduled_base_target = scheduled_target;

    bool preheat = false;
    if (!is_day && cfg->night_enabled && heater_status.blue) {
        const int minutes_until_day = minutes_until(current_minutes, cfg->day_start_minutes);
        float delta = cfg->day_temperature - heater_status.rem;
        if (delta < 0.0f) {
            delta = 0.0f;
        }
        float warmup_needed = cfg->preheat_max_minutes;
        if (cfg->warmup_rate_c_per_min > 0.01f) {
            warmup_needed = delta / cfg->warmup_rate_c_per_min;
        }
        if (warmup_needed < (float)cfg->preheat_min_minutes) {
            warmup_needed = (float)cfg->preheat_min_minutes;
        }
        if (warmup_needed > (float)cfg->preheat_max_minutes) {
            warmup_needed = (float)cfg->preheat_max_minutes;
        }
        if (minutes_until_day <= (int)warmup_needed) {
            preheat = true;
            scheduled_target = cfg->day_temperature;
            minutes_to_transition = minutes_until_day;
        }
    }

    scheduled_target = clamp_floor(scheduled_target, cfg->floor_temperature);

    heater_status.schedule_target = scheduled_target;
    heater_status.preheat_active = preheat;
    heater_status.schedule_is_day = is_day || preheat;
    heater_status.minutes_to_next_transition = minutes_to_transition;
}

static void update_override_state(const heater_config_t *cfg, time_t now) {
    if (!heater_status.override_active) {
        return;
    }
    if (heater_status.override_expires > 0 && now >= heater_status.override_expires) {
        ESP_LOGI(TAG, "Manual override expired");
        heater_status.override_active = false;
        heater_status.override_target = 0.0f;
        heater_status.override_expires = 0;
    } else {
        heater_status.override_target = clamp_floor(heater_status.override_target, cfg->floor_temperature);
    }
}

void control_task() {
    TickType_t previous_wake_time = xTaskGetTickCount();
    const TickType_t time_increment = pdMS_TO_TICKS(CONTROL_TASK_DELAY_MS);

    do {
        BaseType_t was_delayed = xTaskDelayUntil(&previous_wake_time, time_increment);
        if (was_delayed == pdFALSE) {
            ESP_LOGW(TAG, "Task was not delayed");
        }

        const heater_config_t *cfg = heater_config_get();
        if (!cfg) {
            continue;
        }

        heater_status.safe = true;
        heater_status.update = false;

        time_t now;
        time(&now);

        update_schedule_state(cfg, now);
        update_override_state(cfg, now);

        float effective_target = heater_status.schedule_target;
        if (heater_status.override_active) {
            effective_target = heater_status.override_target;
        }
        effective_target = clamp_floor(effective_target, cfg->floor_temperature);
        heater_status.target = effective_target;

        float delta = 0.0f;
        if (heater_status.blue) {
            delta = heater_status.target - heater_status.rem;
        }

        if (heater_status.fnt >= INTERNAL_MAX_TEMP || heater_status.bck >= INTERNAL_MAX_TEMP ||
            heater_status.top >= MAX_TEMP || heater_status.bot >= MAX_TEMP ||
            heater_status.chip >= INTERNAL_MAX_TEMP) {
            delta = 0.0f;
        }

        if (delta > 0.0f && heater_status.bot > MAX_TEMP - 2.0f) {
            delta = (MAX_TEMP - heater_status.bot) / 5.0f;
        }

        if (delta <= cfg->stage_hold_delta) {
            heater_status.one_on = false;
            heater_status.two_on = false;
        } else if (delta >= cfg->stage_full_delta) {
            heater_status.one_on = true;
            heater_status.two_on = true;
        } else if (delta >= cfg->stage_single_delta) {
            heater_status.one_on = false;
            heater_status.two_on = true;
        } else {
            heater_status.one_on = true;
            heater_status.two_on = false;
        }
    } while (true);
}

bool control_task_start() {
    const heater_config_t *cfg = heater_config_get();
    if (cfg) {
        heater_status.target = cfg->day_temperature;
        heater_status.schedule_target = cfg->day_temperature;
        heater_status.scheduled_base_target = cfg->day_temperature;
    } else {
        heater_status.target = TARGET_DEFAULT;
    }
    heater_status.safe = true;
    heater_status.override_active = false;
    heater_status.override_target = 0.0f;
    heater_status.override_expires = 0;

    xTaskCreate(control_task, "control_task", 4096, NULL, CONTROL_TASK_PRIORITY, NULL);
    return true;
}

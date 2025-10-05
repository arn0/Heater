#include "config.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "ArduinoJson.h"

static const char *TAG = "heater_config";
static const char *CONFIG_PATH = "/data/config.json";

static const heater_config_t kDefaultConfig = {
    .day_start_minutes = 6 * 60 + 30,
    .night_start_minutes = 22 * 60 + 30,
    .day_temperature = 20.0f,
    .night_temperature = 17.0f,
    .floor_temperature = 12.0f,
    .night_enabled = true,
    .preheat_min_minutes = 30,
    .preheat_max_minutes = 90,
    .warmup_rate_c_per_min = 0.12f,
    .stage_full_delta = 0.5f,
    .stage_single_delta = 0.25f,
    .stage_hold_delta = 0.05f,
    .override_duration_minutes = 120,
};

static heater_config_t s_config = kDefaultConfig;

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static float clamp_float(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static void normalize_config(heater_config_t *cfg) {
    cfg->day_start_minutes = clamp_int(cfg->day_start_minutes, 0, 23 * 60 + 59);
    cfg->night_start_minutes = clamp_int(cfg->night_start_minutes, 0, 23 * 60 + 59);

    cfg->floor_temperature = clamp_float(cfg->floor_temperature, 5.0f, 25.0f);
    cfg->day_temperature = clamp_float(cfg->day_temperature, cfg->floor_temperature, 26.0f);

    if (cfg->night_enabled) {
        cfg->night_temperature = clamp_float(cfg->night_temperature, cfg->floor_temperature, cfg->day_temperature);
    } else {
        cfg->night_temperature = cfg->day_temperature;
    }

    cfg->preheat_min_minutes = clamp_int(cfg->preheat_min_minutes, 0, 6 * 60);
    cfg->preheat_max_minutes = clamp_int(cfg->preheat_max_minutes, cfg->preheat_min_minutes, 6 * 60);

    if (cfg->warmup_rate_c_per_min <= 0.01f) {
        cfg->warmup_rate_c_per_min = kDefaultConfig.warmup_rate_c_per_min;
    }

    if (cfg->stage_hold_delta < 0.0f) {
        cfg->stage_hold_delta = 0.0f;
    }
    if (cfg->stage_single_delta < cfg->stage_hold_delta + 0.01f) {
        cfg->stage_single_delta = cfg->stage_hold_delta + 0.01f;
    }
    if (cfg->stage_full_delta < cfg->stage_single_delta + 0.01f) {
        cfg->stage_full_delta = cfg->stage_single_delta + 0.01f;
    }

    cfg->override_duration_minutes = clamp_int(cfg->override_duration_minutes, 15, 8 * 60);
}

const heater_config_t *heater_config_get(void) {
    return &s_config;
}

const heater_config_t *heater_config_defaults(void) {
    return &kDefaultConfig;
}

heater_config_t *heater_config_mutable(void) {
    return &s_config;
}

int heater_config_minutes_from_string(const char *hhmm) {
    if (!hhmm) {
        return -1;
    }
    int hour = 0;
    int minute = 0;
    if (sscanf(hhmm, "%d:%d", &hour, &minute) != 2) {
        return -1;
    }
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return -1;
    }
    return hour * 60 + minute;
}

void heater_config_string_from_minutes(int minutes, char *buffer, size_t buffer_len) {
    if (!buffer || buffer_len == 0) {
        return;
    }
    const int total_minutes = (minutes % (24 * 60) + (24 * 60)) % (24 * 60);
    snprintf(buffer, buffer_len, "%02d:%02d", total_minutes / 60, total_minutes % 60);
}

esp_err_t heater_config_apply(const heater_config_t *config, bool persist) {
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }
    s_config = *config;
    normalize_config(&s_config);

    if (persist) {
        return heater_config_save();
    }
    return ESP_OK;
}

esp_err_t heater_config_load(void) {
    FILE *fp = fopen(CONFIG_PATH, "r");
    if (!fp) {
        ESP_LOGW(TAG, "Config file not found, using defaults");
        s_config = kDefaultConfig;
        return heater_config_save();
    }

    char buffer[512];
    size_t read = fread(buffer, 1, sizeof(buffer) - 1, fp);
    fclose(fp);
    buffer[read] = '\0';

    DynamicJsonDocument doc(1024);
    auto err = deserializeJson(doc, buffer);
    if (err) {
        ESP_LOGE(TAG, "Failed to parse config file: %s", err.c_str());
        s_config = kDefaultConfig;
        return heater_config_save();
    }

    heater_config_t cfg = kDefaultConfig;

    if (doc.containsKey("day_start")) {
        cfg.day_start_minutes = heater_config_minutes_from_string(doc["day_start"].as<const char *>());
    } else if (doc.containsKey("day_start_minutes")) {
        cfg.day_start_minutes = doc["day_start_minutes"].as<int>();
    }

    if (doc.containsKey("night_start")) {
        cfg.night_start_minutes = heater_config_minutes_from_string(doc["night_start"].as<const char *>());
    } else if (doc.containsKey("night_start_minutes")) {
        cfg.night_start_minutes = doc["night_start_minutes"].as<int>();
    }

    if (doc.containsKey("day_temp")) {
        cfg.day_temperature = doc["day_temp"].as<float>();
    }
    if (doc.containsKey("night_temp")) {
        cfg.night_temperature = doc["night_temp"].as<float>();
    }
    if (doc.containsKey("floor_temp")) {
        cfg.floor_temperature = doc["floor_temp"].as<float>();
    }
    if (doc.containsKey("night_enabled")) {
        cfg.night_enabled = doc["night_enabled"].as<bool>();
    }
    if (doc.containsKey("preheat_min")) {
        cfg.preheat_min_minutes = doc["preheat_min"].as<int>();
    }
    if (doc.containsKey("preheat_max")) {
        cfg.preheat_max_minutes = doc["preheat_max"].as<int>();
    }
    if (doc.containsKey("warmup_rate")) {
        cfg.warmup_rate_c_per_min = doc["warmup_rate"].as<float>();
    }
    if (doc.containsKey("stage_full")) {
        cfg.stage_full_delta = doc["stage_full"].as<float>();
    }
    if (doc.containsKey("stage_single")) {
        cfg.stage_single_delta = doc["stage_single"].as<float>();
    }
    if (doc.containsKey("stage_hold")) {
        cfg.stage_hold_delta = doc["stage_hold"].as<float>();
    }
    if (doc.containsKey("override_minutes")) {
        cfg.override_duration_minutes = doc["override_minutes"].as<int>();
    }

    normalize_config(&cfg);
    s_config = cfg;
    ESP_LOGI(TAG, "Loaded config: day %02d:%02d night %02d:%02d temp %.1f/%.1f floor %.1f",
             cfg.day_start_minutes / 60, cfg.day_start_minutes % 60,
             cfg.night_start_minutes / 60, cfg.night_start_minutes % 60,
             cfg.day_temperature, cfg.night_temperature, cfg.floor_temperature);
    return ESP_OK;
}

esp_err_t heater_config_save(void) {
    StaticJsonDocument<512> doc;
    char time_buffer[6];
    heater_config_string_from_minutes(s_config.day_start_minutes, time_buffer, sizeof(time_buffer));
    doc["day_start"] = time_buffer;
    heater_config_string_from_minutes(s_config.night_start_minutes, time_buffer, sizeof(time_buffer));
    doc["night_start"] = time_buffer;
    doc["day_temp"] = s_config.day_temperature;
    doc["night_temp"] = s_config.night_temperature;
    doc["floor_temp"] = s_config.floor_temperature;
    doc["night_enabled"] = s_config.night_enabled;
    doc["preheat_min"] = s_config.preheat_min_minutes;
    doc["preheat_max"] = s_config.preheat_max_minutes;
    doc["warmup_rate"] = s_config.warmup_rate_c_per_min;
    doc["stage_full"] = s_config.stage_full_delta;
    doc["stage_single"] = s_config.stage_single_delta;
    doc["stage_hold"] = s_config.stage_hold_delta;
    doc["override_minutes"] = s_config.override_duration_minutes;

    char buffer[512];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));

    FILE *fp = fopen(CONFIG_PATH, "w");
    if (!fp) {
        ESP_LOGE(TAG, "Failed to open config file for writing: %s", CONFIG_PATH);
        return ESP_FAIL;
    }
    size_t written = fwrite(buffer, 1, len, fp);
    fclose(fp);
    if (written != len) {
        ESP_LOGE(TAG, "Failed to write full config file (%u vs %u)", (unsigned)written, (unsigned)len);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Saved config (%u bytes)", (unsigned)len);
    return ESP_OK;
}

static void update_from_object(const JsonVariantConst &node, heater_config_t *cfg) {
    if (node.isNull() || !cfg) {
        return;
    }
    if (node.containsKey("day_start")) {
        int mins = heater_config_minutes_from_string(node["day_start"].as<const char *>());
        if (mins >= 0) {
            cfg->day_start_minutes = mins;
        }
    }
    if (node.containsKey("night_start")) {
        int mins = heater_config_minutes_from_string(node["night_start"].as<const char *>());
        if (mins >= 0) {
            cfg->night_start_minutes = mins;
        }
    }
    if (node.containsKey("day_temp")) {
        cfg->day_temperature = node["day_temp"].as<float>();
    }
    if (node.containsKey("night_temp")) {
        cfg->night_temperature = node["night_temp"].as<float>();
    }
    if (node.containsKey("floor_temp")) {
        cfg->floor_temperature = node["floor_temp"].as<float>();
    }
    if (node.containsKey("night_enabled")) {
        cfg->night_enabled = node["night_enabled"].as<bool>();
    }
    if (node.containsKey("preheat_min")) {
        cfg->preheat_min_minutes = node["preheat_min"].as<int>();
    }
    if (node.containsKey("preheat_max")) {
        cfg->preheat_max_minutes = node["preheat_max"].as<int>();
    }
    if (node.containsKey("warmup_rate")) {
        cfg->warmup_rate_c_per_min = node["warmup_rate"].as<float>();
    }
    if (node.containsKey("stage_full")) {
        cfg->stage_full_delta = node["stage_full"].as<float>();
    }
    if (node.containsKey("stage_single")) {
        cfg->stage_single_delta = node["stage_single"].as<float>();
    }
    if (node.containsKey("stage_hold")) {
        cfg->stage_hold_delta = node["stage_hold"].as<float>();
    }
    if (node.containsKey("override_minutes")) {
        cfg->override_duration_minutes = node["override_minutes"].as<int>();
    }
}

esp_err_t heater_config_apply_json(const char *json_payload) {
    if (!json_payload) {
        return ESP_ERR_INVALID_ARG;
    }

    DynamicJsonDocument doc(1024);
    auto err = deserializeJson(doc, json_payload);
    if (err) {
        ESP_LOGE(TAG, "Failed to parse websocket payload: %s", err.c_str());
        return ESP_ERR_INVALID_ARG;
    }

    const char *type = doc["type"].as<const char *>();
    if (type && strcmp(type, "schedule") != 0) {
        ESP_LOGW(TAG, "Unhandled config message type: %s", type);
        return ESP_OK;
    }

    heater_config_t cfg = s_config;
    update_from_object(doc.as<JsonVariantConst>(), &cfg);

    if (doc.containsKey("temps")) {
        update_from_object(doc["temps"], &cfg);
    }
    if (doc.containsKey("preheat")) {
        update_from_object(doc["preheat"], &cfg);
    }

    normalize_config(&cfg);
    s_config = cfg;
    ESP_LOGI(TAG, "Applied config from websocket");
    return heater_config_save();
}

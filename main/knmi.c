#include "knmi.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdint.h>

#include "esp_http_client.h"
#include "esp_log.h"

#include "../../secret.h"

#define KNMI_BASE_URL "https://api.dataplatform.knmi.nl/edr/v1/collections/10-minute-in-situ-meteorological-observations/locations/0-20000-0-06375"
#define KNMI_HTTP_TIMEOUT_MS 6000
#define KNMI_SINGLE_OFFSET_SECONDS (3 * 60)
#define KNMI_STEP_SECONDS 600

static const char *TAG = "knmi";

typedef struct {
    char *buffer;
    size_t capacity;
    size_t length;
} knmi_buffer_t;

static uint32_t s_window_secs = KNMI_WINDOW_DEFAULT_SECONDS;

static const char KNMI_ROOT_CA_PEM[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIEXjCCA0agAwIBAgITB3MSSkvL1E7HtTvq8ZSELToPoTANBgkqhkiG9w0BAQsF\n"
"ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n"
"b24gUm9vdCBDQSAxMB4XDTIyMDgyMzIyMjUzMFoXDTMwMDgyMzIyMjUzMFowPDEL\n"
"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEcMBoGA1UEAxMTQW1hem9uIFJT\n"
"QSAyMDQ4IE0wMjCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALtDGMZa\n"
"qHneKei1by6+pUPPLljTB143Si6VpEWPc6mSkFhZb/6qrkZyoHlQLbDYnI2D7hD0\n"
"sdzEqfnuAjIsuXQLG3A8TvX6V3oFNBFVe8NlLJHvBseKY88saLwufxkZVwk74g4n\n"
"WlNMXzla9Y5F3wwRHwMVH443xGz6UtGSZSqQ94eFx5X7Tlqt8whi8qCaKdZ5rNak\n"
"+r9nUThOeClqFd4oXych//Rc7Y0eX1KNWHYSI1Nk31mYgiK3JvH063g+K9tHA63Z\n"
"eTgKgndlh+WI+zv7i44HepRZjA1FYwYZ9Vv/9UkC5Yz8/yU65fgjaE+wVHM4e/Yy\n"
"C2osrPWE7gJ+dXMCAwEAAaOCAVowggFWMBIGA1UdEwEB/wQIMAYBAf8CAQAwDgYD\n"
"VR0PAQH/BAQDAgGGMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjAdBgNV\n"
"HQ4EFgQUwDFSzVpQw4J8dHHOy+mc+XrrguIwHwYDVR0jBBgwFoAUhBjMhTTsvAyU\n"
"lC4IWZzHshBOCggwewYIKwYBBQUHAQEEbzBtMC8GCCsGAQUFBzABhiNodHRwOi8v\n"
"b2NzcC5yb290Y2ExLmFtYXpvbnRydXN0LmNvbTA6BggrBgEFBQcwAoYuaHR0cDov\n"
"L2NydC5yb290Y2ExLmFtYXpvbnRydXN0LmNvbS9yb290Y2ExLmNlcjA/BgNVHR8E\n"
"ODA2MDSgMqAwhi5odHRwOi8vY3JsLnJvb3RjYTEuYW1hem9udHJ1c3QuY29tL3Jv\n"
"b3RjYTEuY3JsMBMGA1UdIAQMMAowCAYGZ4EMAQIBMA0GCSqGSIb3DQEBCwUAA4IB\n"
"AQAtTi6Fs0Azfi+iwm7jrz+CSxHH+uHl7Law3MQSXVtR8RV53PtR6r/6gNpqlzdo\n"
"Zq4FKbADi1v9Bun8RY8D51uedRfjsbeodizeBB8nXmeyD33Ep7VATj4ozcd31YFV\n"
"fgRhvTSxNrrTlNpWkUk0m3BMPv8sg381HhA6uEYokE5q9uws/3YkKqRiEz3TsaWm\n"
"JqIRZhMbgAfp7O7FUwFIb7UIspogZSKxPIWJpxiPo3TcBambbVtQOcNRWz5qCQdD\n"
"slI2yayq0n2TXoHyNCLEH8rpsJRVILFsg0jc7BaFrMnF462+ajSehgj12IidNeRN\n"
"4zl+EoNaWdpnWndvSpAEkq2P\n"
"-----END CERTIFICATE-----\n"
"-----BEGIN CERTIFICATE-----\n"
"MIIEkjCCA3qgAwIBAgITBn+USionzfP6wq4rAfkI7rnExjANBgkqhkiG9w0BAQsF\n"
"ADCBmDELMAkGA1UEBhMCVVMxEDAOBgNVBAgTB0FyaXpvbmExEzARBgNVBAcTClNj\n"
"b3R0c2RhbGUxJTAjBgNVBAoTHFN0YXJmaWVsZCBUZWNobm9sb2dpZXMsIEluYy4x\n"
"OzA5BgNVBAMTMlN0YXJmaWVsZCBTZXJ2aWNlcyBSb290IENlcnRpZmljYXRlIEF1\n"
"dGhvcml0eSAtIEcyMB4XDTE1MDUyNTEyMDAwMFoXDTM3MTIzMTAxMDAwMFowOTEL\n"
"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n"
"b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n"
"ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n"
"9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n"
"IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n"
"VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n"
"93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n"
"jgSubJrIqg0CAwEAAaOCATEwggEtMA8GA1UdEwEB/wQFMAMBAf8wDgYDVR0PAQH/\n"
"BAQDAgGGMB0GA1UdDgQWBBSEGMyFNOy8DJSULghZnMeyEE4KCDAfBgNVHSMEGDAW\n"
"gBScXwDfqgHXMCs4iKK4bUqc8hGRgzB4BggrBgEFBQcBAQRsMGowLgYIKwYBBQUH\n"
"MAGGImh0dHA6Ly9vY3NwLnJvb3RnMi5hbWF6b250cnVzdC5jb20wOAYIKwYBBQUH\n"
"MAKGLGh0dHA6Ly9jcnQucm9vdGcyLmFtYXpvbnRydXN0LmNvbS9yb290ZzIuY2Vy\n"
"MD0GA1UdHwQ2MDQwMqAwoC6GLGh0dHA6Ly9jcmwucm9vdGcyLmFtYXpvbnRydXN0\n"
"LmNvbS9yb290ZzIuY3JsMBEGA1UdIAQKMAgwBgYEVR0gADANBgkqhkiG9w0BAQsF\n"
"AAOCAQEAYjdCXLwQtT6LLOkMm2xF4gcAevnFWAu5CIw+7bMlPLVvUOTNNWqnkzSW\n"
"MiGpSESrnO09tKpzbeR/FoCJbM8oAxiDR3mjEH4wW6w7sGDgd9QIpuEdfF7Au/ma\n"
"eyKdpwAJfqxGF4PcnCZXmTA5YpaP7dreqsXMGz7KQ2hsVxa81Q4gLv7/wmpdLqBK\n"
"bRRYh5TmOTFffHPLkIhqhBGWJ6bt2YFGpn6jcgAKUj6DiAdjd4lpFw85hdKrCEVN\n"
"0FE6/V1dN2RMfjCyVSRCnTawXZwXgWHxyvkQAiSr6w10kY17RSlQOYiypok1JR4U\n"
"akcjMS9cmvqtmg5iUaQqqcT5NJ0hGA==\n"
"-----END CERTIFICATE-----\n";

static void knmi_series_init(knmi_series_t *series) {
    if (!series) {
        return;
    }
    series->samples = NULL;
    series->count = 0;
}

void knmi_series_free(knmi_series_t *series) {
    if (!series) {
        return;
    }
    free(series->samples);
    series->samples = NULL;
    series->count = 0;
}

static void encode_datetime(const char *input, char *output, size_t output_size) {
    size_t j = 0;
    for (size_t i = 0; input[i] != '\0' && j + 1 < output_size; ++i) {
        if (input[i] == ':') {
            if (j + 3 >= output_size) {
                break;
            }
            output[j++] = '%';
            output[j++] = '3';
            output[j++] = 'A';
        } else {
            output[j++] = input[i];
        }
    }
    output[j] = '\0';
}

static esp_err_t knmi_http_event_handler(esp_http_client_event_t *evt) {
    knmi_buffer_t *ctx = (knmi_buffer_t *)evt->user_data;
    if (!ctx) {
        return ESP_OK;
    }

    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (evt->data && evt->data_len > 0) {
            size_t remaining = (ctx->capacity > ctx->length) ? (ctx->capacity - ctx->length - 1) : 0;
            size_t copy_len = evt->data_len;
            if (copy_len > remaining) {
                copy_len = remaining;
            }
            if (copy_len > 0) {
                memcpy(ctx->buffer + ctx->length, evt->data, copy_len);
                ctx->length += copy_len;
                ctx->buffer[ctx->length] = '\0';
            }
        }
        break;
    case HTTP_EVENT_ERROR:
        ESP_LOGW(TAG, "HTTP event error");
        break;
    default:
        break;
    }

    return ESP_OK;
}

extern time_t __tm_to_time_t(const struct tm *);

static int64_t knmi_days_from_civil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);                     // [0, 399]
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1; // [0, 365]
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;         // [0, 146096]
    return era * 146097LL + (int64_t)doe - 719468LL;
}

static time_t knmi_timegm(const struct tm *tm) {
    if (!tm) {
        return (time_t)-1;
    }
    int year = tm->tm_year + 1900;
    unsigned month = tm->tm_mon + 1;
    unsigned day = tm->tm_mday;
    int64_t days = knmi_days_from_civil(year, month, day);
    int64_t seconds = ((days * 24 + tm->tm_hour) * 60 + tm->tm_min) * 60 + tm->tm_sec;
    return (time_t)seconds;
}

static bool iso8601_to_time_t(const char *str, size_t len, time_t *out_time) {
    if (!str || len < 19 || !out_time) {
        return false;
    }
    char buffer[32];
    if (len >= sizeof(buffer)) {
        len = sizeof(buffer) - 1;
    }
    memcpy(buffer, str, len);
    buffer[len] = '\0';

    int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
    if (sscanf(buffer, "%4d-%2d-%2dT%2d:%2d:%2d", &year, &month, &day, &hour, &minute, &second) != 6) {
        return false;
    }

    struct tm tm_val = {0};
    tm_val.tm_year = year - 1900;
    tm_val.tm_mon = month - 1;
    tm_val.tm_mday = day;
    tm_val.tm_hour = hour;
    tm_val.tm_min = minute;
    tm_val.tm_sec = second;
    tm_val.tm_isdst = 0;

    time_t t = knmi_timegm(&tm_val);
    if (t == (time_t)-1) {
        return false;
    }
    *out_time = t;
    return true;
}

static bool parse_float_array(const char *start, const char *end, float **out_values, size_t *out_count) {
    if (!start || !end || end <= start || !out_values || !out_count) {
        return false;
    }
    size_t count = 0;
    const char *ptr = start;
    while (ptr < end) {
        while (ptr < end && !(isdigit((unsigned char)*ptr) || *ptr == '-' || *ptr == '+')) {
            if (*ptr == ']') {
                break;
            }
            ++ptr;
        }
        if (ptr >= end || *ptr == ']') {
            break;
        }
        char *next = NULL;
        strtof(ptr, &next);
        if (next == ptr) {
            break;
        }
        ++count;
        ptr = next;
    }

    if (count == 0) {
        *out_values = NULL;
        *out_count = 0;
        return true;
    }

    float *values = (float *)malloc(count * sizeof(float));
    if (!values) {
        return false;
    }

    ptr = start;
    size_t idx = 0;
    while (ptr < end && idx < count) {
        while (ptr < end && !(isdigit((unsigned char)*ptr) || *ptr == '-' || *ptr == '+')) {
            if (*ptr == ']') {
                break;
            }
            ++ptr;
        }
        if (ptr >= end || *ptr == ']') {
            break;
        }
        char *next = NULL;
        float value = strtof(ptr, &next);
        if (next == ptr) {
            break;
        }
        values[idx++] = value;
        ptr = next;
    }

    if (idx != count) {
        free(values);
        return false;
    }

    *out_values = values;
    *out_count = count;
    return true;
}

static bool parse_time_array(const char *payload, size_t expected_count, time_t **out_times, size_t *out_count) {
    if (!payload || !out_times || !out_count) {
        return false;
    }
    const char *t_axis = strstr(payload, "\"axes\"");
    if (!t_axis) {
        return false;
    }
    const char *t_block = strstr(t_axis, "\"t\"");
    if (!t_block) {
        return false;
    }
    const char *values_key = strstr(t_block, "\"values\"");
    if (!values_key) {
        return false;
    }
    const char *array_start = strchr(values_key, '[');
    const char *array_end = array_start ? strchr(array_start, ']') : NULL;
    if (!array_start || !array_end || array_end <= array_start + 1) {
        return false;
    }

    time_t *timestamps = NULL;
    size_t allocated = 0;
    if (expected_count > 0) {
        timestamps = (time_t *)malloc(expected_count * sizeof(time_t));
        allocated = expected_count;
    }

    size_t idx = 0;
    const char *scan = array_start;
    while (scan < array_end) {
        const char *q1 = strchr(scan, '"');
        if (!q1 || q1 >= array_end) {
            break;
        }
        const char *q2 = strchr(q1 + 1, '"');
        if (!q2 || q2 > array_end) {
            break;
        }
        time_t ts = 0;
        if (!iso8601_to_time_t(q1 + 1, (size_t)(q2 - (q1 + 1)), &ts)) {
            free(timestamps);
            return false;
        }
        if (idx >= allocated) {
            size_t new_cap = allocated ? allocated * 2 : 4;
            time_t *tmp = (time_t *)realloc(timestamps, new_cap * sizeof(time_t));
            if (!tmp) {
                free(timestamps);
                return false;
            }
            timestamps = tmp;
            allocated = new_cap;
        }
        timestamps[idx++] = ts;
        scan = q2 + 1;
    }

    *out_times = timestamps;
    *out_count = idx;
    return true;
}

static bool knmi_parse_series_json(const char *payload, knmi_series_t *series) {
    if (!payload || !series) {
        return false;
    }

    knmi_series_init(series);

    const char *ta_section = strstr(payload, "\"ta\"");
    if (!ta_section) {
        ESP_LOGW(TAG, "Missing 'ta' section in payload");
        return false;
    }
    const char *values_key = strstr(ta_section, "\"values\"");
    if (!values_key) {
        ESP_LOGW(TAG, "Missing ta values array in payload");
        return false;
    }
    const char *array_start = strchr(values_key, '[');
    const char *array_end = array_start ? strchr(array_start, ']') : NULL;
    if (!array_start || !array_end || array_end <= array_start + 1) {
        ESP_LOGW(TAG, "Malformed ta values array");
        return false;
    }

    float *values = NULL;
    size_t value_count = 0;
    if (!parse_float_array(array_start, array_end, &values, &value_count)) {
        ESP_LOGW(TAG, "Failed to parse ta values array");
        return false;
    }

    time_t *timestamps = NULL;
    size_t ts_count = 0;
    if (!parse_time_array(payload, value_count, &timestamps, &ts_count)) {
        free(values);
        ESP_LOGW(TAG, "Failed to parse time axis");
        return false;
    }

    size_t count = value_count < ts_count ? value_count : ts_count;
    if (count == 0) {
        free(values);
        free(timestamps);
        series->samples = NULL;
        series->count = 0;
        return true;
    }

    knmi_sample_t *samples = (knmi_sample_t *)calloc(count, sizeof(knmi_sample_t));
    if (!samples) {
        free(values);
        free(timestamps);
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        samples[i].timestamp = timestamps[i];
        samples[i].temperature = values[i];
    }

    free(values);
    free(timestamps);
    series->samples = samples;
    series->count = count;
    return true;
}

static bool time_synchronised(void) {
    time_t now = 0;
    time(&now);
    return now >= 1600000000; // ~2020-09-13
}

static void time_to_iso8601(time_t t, char *buffer, size_t len) {
    struct tm tm_utc;
    gmtime_r(&t, &tm_utc);
    strftime(buffer, len, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}

static esp_err_t knmi_http_fetch(const char *url, knmi_buffer_t *buffer) {
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = KNMI_HTTP_TIMEOUT_MS,
        .event_handler = knmi_http_event_handler,
        .user_data = buffer,
        .cert_pem = KNMI_ROOT_CA_PEM,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to create KNMI HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_set_header(client, "Accept", "application/prs.coverage+json");
    if (err == ESP_OK) {
        err = esp_http_client_set_header(client, "Authorization", KNMI_EDR_API_KEY);
    }
    if (err == ESP_OK) {
        err = esp_http_client_set_header(client, "Accept-Encoding", "identity");
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set KNMI request headers: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "KNMI request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "KNMI request returned HTTP %d", status);
        return (status == 404) ? ESP_ERR_NOT_FOUND : ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t knmi_fetch_series(time_t start_time, time_t end_time, knmi_series_t *out_series) {
    if (!out_series) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!time_synchronised()) {
        ESP_LOGI(TAG, "Skipping KNMI request until system time is synchronised");
        return ESP_ERR_INVALID_STATE;
    }

    if (end_time < start_time) {
        time_t tmp = start_time;
        start_time = end_time;
        end_time = tmp;
    }

    knmi_series_init(out_series);

    char start_iso[32];
    char end_iso[32];
    time_to_iso8601(start_time, start_iso, sizeof(start_iso));
    time_to_iso8601(end_time, end_iso, sizeof(end_iso));

    char start_encoded[48];
    char end_encoded[48];
    encode_datetime(start_iso, start_encoded, sizeof(start_encoded));
    encode_datetime(end_iso, end_encoded, sizeof(end_encoded));

    char url[256];
    if (start_time == end_time) {
        int written = snprintf(url, sizeof(url),
                               "%s?f=CoverageJSON&datetime=%s&parameter-name=ta",
                               KNMI_BASE_URL, start_encoded);
        if (written < 0 || written >= (int)sizeof(url)) {
            ESP_LOGW(TAG, "KNMI request URL truncated");
            return ESP_FAIL;
        }
    } else {
        int written = snprintf(url, sizeof(url),
                               "%s?f=CoverageJSON&datetime=%s/%s&parameter-name=ta",
                               KNMI_BASE_URL, start_encoded, end_encoded);
        if (written < 0 || written >= (int)sizeof(url)) {
            ESP_LOGW(TAG, "KNMI request URL truncated");
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "KNMI request URL: %s", url);

    char payload[4096] = {0};
    knmi_buffer_t buffer = {
        .buffer = payload,
        .capacity = sizeof(payload),
        .length = 0,
    };

    esp_err_t err = knmi_http_fetch(url, &buffer);
    if (err != ESP_OK) {
        return err;
    }

    if (!knmi_parse_series_json(payload, out_series)) {
        ESP_LOGW(TAG, "Failed to parse KNMI response");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "KNMI returned %u samples", (unsigned)out_series->count);
    return ESP_OK;
}

esp_err_t knmi_fetch_point(time_t target_time, knmi_sample_t *out_sample) {
    if (!time_synchronised()) {
        ESP_LOGI(TAG, "Skipping KNMI request until system time is synchronised");
        return ESP_ERR_INVALID_STATE;
    }

    if (target_time < KNMI_SINGLE_OFFSET_SECONDS) {
        target_time = KNMI_SINGLE_OFFSET_SECONDS;
    }

    time_t query_time = target_time - KNMI_SINGLE_OFFSET_SECONDS;
    query_time = (query_time / KNMI_STEP_SECONDS) * KNMI_STEP_SECONDS;

    knmi_series_t series;
    knmi_series_init(&series);
    esp_err_t err = knmi_fetch_series(query_time, query_time, &series);
    if (err != ESP_OK) {
        knmi_series_free(&series);
        return err;
    }
    if (series.count == 0) {
        knmi_series_free(&series);
        return ESP_ERR_NOT_FOUND;
    }

    if (out_sample) {
        *out_sample = series.samples[series.count - 1];
    }

    knmi_series_free(&series);
    return ESP_OK;
}

void knmi_set_window_seconds(uint32_t window_secs) {
    uint32_t clamped = window_secs;
    if (clamped < KNMI_WINDOW_MIN_SECONDS) {
        clamped = KNMI_WINDOW_MIN_SECONDS;
    }
    if (clamped > KNMI_WINDOW_MAX_SECONDS) {
        clamped = KNMI_WINDOW_MAX_SECONDS;
    }
    if (clamped != s_window_secs) {
        ESP_LOGI(TAG, "Setting KNMI window to %u seconds", (unsigned)clamped);
        s_window_secs = clamped;
    }
}

uint32_t knmi_get_window_seconds(void) {
    return s_window_secs;
}

esp_err_t knmi_handle_ws_command(const char *json_payload) {
    if (!json_payload) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *type = strstr(json_payload, "\"type\"");
    if (!type) {
        return ESP_OK;
    }
    const char *colon = strchr(type, ':');
    if (!colon) {
        return ESP_OK;
    }
    colon++;
    while (*colon && isspace((unsigned char)*colon)) {
        ++colon;
    }
    if (*colon != '"') {
        return ESP_OK;
    }
    const char *end = strchr(colon + 1, '"');
    if (!end) {
        return ESP_OK;
    }
    size_t len = (size_t)(end - (colon + 1));
    if (len != strlen("knmi")) {
        return ESP_OK;
    }
    if (strncmp(colon + 1, "knmi", len) != 0) {
        return ESP_OK;
    }

    const char *window_key = strstr(json_payload, "\"window_secs\"");
    if (!window_key) {
        window_key = strstr(json_payload, "\"window\"");
    }
    if (!window_key) {
        ESP_LOGW(TAG, "KNMI command missing window field");
        return ESP_ERR_INVALID_ARG;
    }
    const char *value = strchr(window_key, ':');
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }
    value++;
    while (*value && isspace((unsigned char)*value)) {
        ++value;
    }
    long parsed = strtol(value, NULL, 10);
    if (parsed <= 0) {
        ESP_LOGW(TAG, "Invalid KNMI window value");
        return ESP_ERR_INVALID_ARG;
    }
    knmi_set_window_seconds((uint32_t)parsed);
    return ESP_OK;
}

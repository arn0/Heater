#ifndef KNMI_H
#define KNMI_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KNMI_WINDOW_DEFAULT_SECONDS (10 * 60)
#define KNMI_WINDOW_MIN_SECONDS (5 * 60)
#define KNMI_WINDOW_MAX_SECONDS (6 * 3600)

typedef struct {
    time_t timestamp;
    float temperature;
} knmi_sample_t;

typedef struct {
    knmi_sample_t *samples;
    size_t count;
} knmi_series_t;

/**
 * @brief Fetch KNMI temperature at the specified timestamp (UTC).
 *
 * The API expects timestamps aligned with the KNMI observation cadence. The helper applies the
 * “+3 minute” latency rule before issuing the request.
 *
 * @param target_time  Desired observation time (UNIX epoch, UTC).
 * @param out_sample   Receives the timestamp and temperature (may be NULL).
 */
esp_err_t knmi_fetch_point(time_t target_time, knmi_sample_t *out_sample);

/**
 * @brief Fetch a KNMI time window and return all available samples.
 *
 * @param start_time  Window start (inclusive, UNIX epoch UTC).
 * @param end_time    Window end   (inclusive, UNIX epoch UTC).
 * @param out_series  Receives allocated samples; caller must call knmi_series_free().
 */
esp_err_t knmi_fetch_series(time_t start_time, time_t end_time, knmi_series_t *out_series);

/**
 * @brief Release memory held by a knmi_series_t.
 */
void knmi_series_free(knmi_series_t *series);

/**
 * @brief Update the lookback window (in seconds) used for KNMI data requests.
 *
 * Values are clamped to [KNMI_WINDOW_MIN_SECONDS, KNMI_WINDOW_MAX_SECONDS].
 */
void knmi_set_window_seconds(uint32_t window_secs);

/**
 * @brief Get the current KNMI lookback window in seconds.
 */
uint32_t knmi_get_window_seconds(void);

/**
 * @brief Handle WebSocket JSON command for KNMI settings.
 */
esp_err_t knmi_handle_ws_command(const char *json_payload);

#ifdef __cplusplus
}
#endif

#endif // KNMI_H

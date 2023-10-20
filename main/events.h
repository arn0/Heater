#include "freertos/event_groups.h"
#include "esp_event.h"

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_DISCONNECTED_BIT BIT1
#define SLEEP_WAKEUP_BIT BIT2
#define TCP_CONNECTED_BIT BIT3
#define TCP_FAILED_BIT BIT4

extern EventGroupHandle_t s_wifi_event_group;
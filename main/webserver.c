#include <stdio.h>
#include <stdlib.h>
#include <string.h> //Requires by memset
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "spi_flash_mmap.h"
#include <sys/stat.h>
#include <esp_http_server.h>
#include "nvs_flash.h"
#include "esp_spiffs.h"

/*
 * Command codes for websocket comtrol and monitor.
 *
 */

enum uint_8 { HEAT_1_IS_OFF = 1, HEAT_1_IS_ON, HEAT_2_IS_OFF = 1, HEAT_2_IS_ON, HEAT_1_OFF = 1, HEAT_1_ON, HEAT_2_OFF = 1, HEAT_2_ON, 
    TARGET_SET, TARGET_UPDATE, WIFI_SET, WIFI_UPDATE, BLUETOOTH_SET, BLUETOOTH_UPDATE, MATTER_SET, MATTER_UPDATE, 
    T_REM, T_FRT, T_TOP, T_BOT, T_CHP, M_VOLT, M_CURR };




static const char *TAG = "webserver"; // TAG for debug
int led_state = 0;

#define INDEX_HTML_PATH "/spiffs/index.html"
char index_html[4096];
char response_data[4096];

// We can spend effort making code memory-efficient, or we can just blow
// a chunk of RAM to make code trivial.
#define readBufSize 1024*16
static char readBuf[readBufSize];

#define sendBufSize 256
static char sendBuf[sendBufSize];


/*
 * Initialize and mount SPIFFS partition for future access
 */

esp_err_t spiffs_init()
{
	ESP_LOGI(TAG, "Initializing SPIFFS");

	esp_vfs_spiffs_conf_t conf = {
		.base_path = "/spiffs",
		.partition_label = NULL,
		.max_files = 5,
		.format_if_mount_failed = false
	};

	// Use settings defined above to initialize and mount SPIFFS filesystem.
	// Note: esp_vfs_spiffs_register is an all-in-one convenience function.

	esp_err_t ret = esp_vfs_spiffs_register(&conf);

	if (ret != ESP_OK) {
		if (ret == ESP_FAIL) {
			ESP_LOGE(TAG, "Failed to mount or format filesystem");
		} else if (ret == ESP_ERR_NOT_FOUND) {
			ESP_LOGE(TAG, "Failed to find SPIFFS partition");
		} else {
			ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
		}
		return ret;
	}

	size_t total = 0, used = 0;
	ret = esp_spiffs_info(conf.partition_label, &total, &used);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
	} else {
		ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
	}
	return ret;
}

/*
 * Reads file into memory buffer
 */

static size_t read_spiff_buffer(const char *file_name)
{
	size_t readSize = 0;
	FILE* f = fopen(file_name, "r");
	if (f == NULL) {
		ESP_LOGE(TAG, "Failed to open file for reading");
		return 0;
	}

	readSize = fread(readBuf, sizeof(char), readBufSize, f);
	fclose(f);

	// Fail if file is larger than memory buffer, as we don't
	// have code to split across multiple read operations.
	if (readSize >= readBufSize) {
		ESP_LOGE(TAG, "File size exceeds memory buffer");
		ESP_ERROR_CHECK(ESP_FAIL);
	} else {
		// Technically unnecessary but an extra null termination never hurt anyone
		readBuf[readSize]=0;
	}
return readSize;
}


/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

/*
 * async send function, which we put into the httpd work queue
 */
static void ws_async_send(void *arg)
{
    static const char * data = "Async data";
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = strlen(data);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    if (resp_arg == NULL) {
        return ESP_ERR_NO_MEM;
    }
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    esp_err_t ret = httpd_queue_work(handle, ws_async_send, resp_arg);
    if (ret != ESP_OK) {
        free(resp_arg);
    }
    return ret;
}




void ws_task(void){

    // need to know if websocket is still connected, else we exit

    // test if values are updated

    // prepare command

    //send command
}


















/*
 * Main server page
 */

esp_err_t get_index_handler(httpd_req_t *req)
{
  size_t readSize = 0;
  ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "cache-control", "max-age=1")); // For development
  ESP_ERROR_CHECK(httpd_resp_set_type(req, "text/html"));
  readSize = read_spiff_buffer("/spiffs/index.html");
  ESP_ERROR_CHECK(httpd_resp_send(req, readBuf, readSize));

  return ESP_OK;
}

httpd_uri_t get_root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_index_handler,
    .user_ctx = NULL};

httpd_uri_t get_index = {
    .uri = "/index.html",
    .method = HTTP_GET,
    .handler = get_index_handler,
    .user_ctx = NULL};


/*
 * Websocket
 */

static esp_err_t get_ws_handler(httpd_req_t *req)
{
	if (req->method == HTTP_GET) {
		ESP_LOGI(TAG, "Handshake done, the new connection was opened");
	    xTaskCreate( ws_task, "ws task", 4096, NULL, 5, NULL );
		return ESP_OK;
	}
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    }
    ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT &&
        strcmp((char*)ws_pkt.payload,"Trigger async") == 0) {
        free(buf);
        return trigger_async_send(req->handle, req);
    }

    ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
    }
    free(buf);
    return ret;
}



static const httpd_uri_t get_ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = get_ws_handler,
        .user_ctx   = NULL,
        .is_websocket = true
};
















httpd_handle_t start_webserver(void)
{
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	httpd_handle_t server = NULL;

	// Initialize SPIFFS which holds the HTML/CSS/JS files we serve to client browser
	ESP_ERROR_CHECK(spiffs_init());

	// Start the httpd server
	ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) == ESP_OK) {
		ESP_LOGI(TAG, "Registering URI handlers");
		httpd_register_uri_handler(server, &get_index);
		httpd_register_uri_handler(server, &get_root);
		httpd_register_uri_handler(server, &get_ws);    // Registering the ws handler
		return server;
	}
	ESP_LOGI(TAG, "Error starting server!");
	return NULL;
}
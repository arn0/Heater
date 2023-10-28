#include <stdio.h>
#include <stdlib.h>
#include <string.h> //Requires by memset
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_system.h"
#include "spi_flash_mmap.h"
#include <sys/stat.h>
#include <esp_http_server.h>
#include "nvs_flash.h"
#include "esp_spiffs.h"

#include "heater_task.h"


#define HEATER_WEB_STEP 0.5

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




void ws_task(){

	do{
    // need to know if websocket is still connected, else we exit

    // test if values are updated

    // prepare command

    //send command
	}while (true);
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

	heater_status.web = TAR_W_FL | ONE_W_FL | TWO_W_FL;		// force update once

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
 * Struct with information to uniquely identify a websocket
 */

struct websock_instance {
  httpd_handle_t handle;
  int descriptor;
};

// Websocket instance for browser joystick client
static struct websock_instance websock_client;

/*
 * @brief WebSocket has been closed, clean up associated data structures
 * @param context Unused here, but had to be nonzero for this callback to be called upon socket close
 */
void socket_close_cleanup(void* context){
  ESP_LOGI(TAG, "Lost our client handler.");
  websock_client.handle = NULL;
  websock_client.descriptor = 0;
}

/*
 * URI handler for WebSocket
 */

static esp_err_t get_ws_handler(httpd_req_t *req)
{
	if (req->method == HTTP_GET) {
		ESP_LOGI(TAG, "Handshake done, the new connection was opened");
		//xTaskCreate( ws_task, "ws task", 4096, NULL, 5, NULL );

		if (websock_client.handle == NULL) {
			// We didn't have a clien before, now we do. Set up notification so
			// we know when it goes away.
			ESP_LOGI(TAG, "Have a new client");
			websock_client.handle = req->handle;
			websock_client.descriptor = httpd_req_to_sockfd(req);
			req->sess_ctx = (void*)1; // Set to nonzero otherwise free_ctx won't get called.
			req->free_ctx = socket_close_cleanup;
		} else if (websock_client.handle != req->handle || websock_client.descriptor != httpd_req_to_sockfd(req)) {
			ESP_LOGI(TAG, "Already have ja client, reject connection attempt.");
			return ESP_FAIL;
		}
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
    //ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
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
        //ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);

			switch(ws_pkt.payload[0]){
				case 'D':
					heater_status.target -= HEATER_WEB_STEP;
					heater_status.web |= TAR_W_FL;
					break;

				case 'U':
					heater_status.target += HEATER_WEB_STEP;
					heater_status.web |= TAR_W_FL;
					break;

				case 'I':
					heater_status.one_d = !heater_status.one_d;
					heater_status.web |= ONE_W_FL;
					break;

				case 'H':
					heater_status.two_d = !heater_status.two_d;
					heater_status.web |= TWO_W_FL;
					break;
			}

    }
    //ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);
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


esp_err_t send_sensor_update_frame(){
	httpd_ws_frame_t ws_frame = {
		.final = true,
		.fragmented = false,
		.type = HTTPD_WS_TYPE_TEXT,
		.payload = (uint8_t*)sendBuf,
		.len = strlen(sendBuf)+1
	};

	ESP_ERROR_CHECK(httpd_ws_send_frame_async( websock_client.handle, websock_client.descriptor, &ws_frame ));
  return ESP_OK;
}


/* --- PRINTF_BYTE_TO_BINARY macro's --- */
#define PRINTF_BINARY_PATTERN_INT8 "%c%c%c%c%c%c%c%c"
#define PRINTF_BYTE_TO_BINARY_INT8(i)    \
    (((i) & 0x80ll) ? '1' : '0'), \
    (((i) & 0x40ll) ? '1' : '0'), \
    (((i) & 0x20ll) ? '1' : '0'), \
    (((i) & 0x10ll) ? '1' : '0'), \
    (((i) & 0x08ll) ? '1' : '0'), \
    (((i) & 0x04ll) ? '1' : '0'), \
    (((i) & 0x02ll) ? '1' : '0'), \
    (((i) & 0x01ll) ? '1' : '0')

#define PRINTF_BINARY_PATTERN_INT16 \
    PRINTF_BINARY_PATTERN_INT8              PRINTF_BINARY_PATTERN_INT8
#define PRINTF_BYTE_TO_BINARY_INT16(i) \
    PRINTF_BYTE_TO_BINARY_INT8((i) >> 8),   PRINTF_BYTE_TO_BINARY_INT8(i)
#define PRINTF_BINARY_PATTERN_INT32 \
    PRINTF_BINARY_PATTERN_INT16             PRINTF_BINARY_PATTERN_INT16
#define PRINTF_BYTE_TO_BINARY_INT32(i) \
    PRINTF_BYTE_TO_BINARY_INT16((i) >> 16), PRINTF_BYTE_TO_BINARY_INT16(i)
#define PRINTF_BINARY_PATTERN_INT64    \
    PRINTF_BINARY_PATTERN_INT32             PRINTF_BINARY_PATTERN_INT32
#define PRINTF_BYTE_TO_BINARY_INT64(i) \
    PRINTF_BYTE_TO_BINARY_INT32((i) >> 32), PRINTF_BYTE_TO_BINARY_INT32(i)
/* --- end macros --- */


/*
 * Send updates to client
 */

void send_sensor_update(){
	TickType_t xLastWakeTime;
	const TickType_t xFrequency = pdMS_TO_TICKS(333);
	BaseType_t xWasDelayed;

	// Initialise the xLastWakeTime variable with the current time.
	xLastWakeTime = xTaskGetTickCount ();

	heater_status.web |= TAR_W_FL;			// force update

	do{
		// Wait for the next cycle.
		xWasDelayed = xTaskDelayUntil( &xLastWakeTime, xFrequency );

		if( xWasDelayed == pdFALSE ){
			ESP_LOGE( TAG, "Task ran out of time" );
		}
	
		// Send only if we have both (1) an update, and (2) a client to send to.

		if( heater_status.web && websock_client.handle != NULL ){
			//ESP_LOGI(TAG, "heater_status.web: " PRINTF_BINARY_PATTERN_INT16 "\n", PRINTF_BYTE_TO_BINARY_INT16(heater_status.web));
			if( heater_status.web & REM_W_FL ){
				heater_status.web &= ~REM_W_FL;
				sprintf( sendBuf, "r%2.01f", heater_status.rem );
				send_sensor_update_frame();
			}else if( heater_status.web & ENV_W_FL ){
				heater_status.web &= ~ENV_W_FL;
				sprintf( sendBuf, "e%2.01f", heater_status.env );
				send_sensor_update_frame();
			}else if( heater_status.web & TOP_W_FL ){
				heater_status.web &= ~TOP_W_FL;
				sprintf( sendBuf, "t%2.01f", heater_status.top );
				send_sensor_update_frame();
			}else if( heater_status.web & BOT_W_FL ){
				heater_status.web &= ~BOT_W_FL;
				sprintf( sendBuf, "b%2.01f", heater_status.bot );
				send_sensor_update_frame();
			}else if( heater_status.web & CHP_W_FL ){
				heater_status.web &= ~CHP_W_FL;
				sprintf( sendBuf, "c%2.01f", heater_status.chip );
				send_sensor_update_frame();
			}else if( heater_status.web & TAR_W_FL ){
				heater_status.web &= ~TAR_W_FL;
				sprintf( sendBuf, "a%2.01f", heater_status.target );
				send_sensor_update_frame();
			}else if( heater_status.web & ONE_W_FL ){
				heater_status.web &= ~ONE_W_FL;
				sprintf( sendBuf, "i%d", heater_status.one_s );
				send_sensor_update_frame();
			}else if( heater_status.web & TWO_W_FL ){
				heater_status.web &= ~TWO_W_FL;
				sprintf( sendBuf, "h%d", heater_status.two_s );
				send_sensor_update_frame();
			}


		}
  }while (true);
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

	// Initialize structure tracking websocket to client
	websock_client.handle = NULL;
	websock_client.descriptor = 0;

	// Initialize SPIFFS which holds the HTML/CSS/JS files we serve to client browser
	ESP_ERROR_CHECK(spiffs_init());

	// Start the httpd server
	ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) == ESP_OK) {
		ESP_LOGI(TAG, "Registering URI handlers");
		httpd_register_uri_handler(server, &get_index);
		httpd_register_uri_handler(server, &get_root);
		httpd_register_uri_handler(server, &get_ws);    // Registering the ws handler

		xTaskCreate( send_sensor_update, "sensor update task", 4096, NULL, 5, NULL );
		return server;
	}
	ESP_LOGI(TAG, "Error starting server!");
	return NULL;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>					//Requires by memset
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_system.h"
#include "spi_flash_mmap.h"
#include <sys/stat.h>
#include <esp_http_server.h>
#include "nvs_flash.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "cJSON.h"

#include "mount.h"
#include "task_priorities"
#include "heater.h"

#define HEATER_WEB_STEP 0.5

static const char *TAG = "webserver";
int led_state = 0;

/* Max length a file path can have on storage */	// FIXME: remove CONFIG_, get acurate file name length
#define CONFIG_SPIFFS_OBJ_NAME_LEN 32
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
#define MAX_FILE_SIZE   (200*1024) // 200 KB
#define MAX_FILE_SIZE_STR "200KB"

/* Scratch buffer size */
#define SCRATCH_BUFSIZE  8192

struct file_server_data {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};

const char *base_path = "/data";

#define INDEX_HTML_PATH "/data/index.html"
char index_html[4096];
char response_data[4096];

// We can spend effort making code memory-efficient, or we can just blow
// a chunk of RAM to make code trivial.

#define readBufSize 1024*16
static char readBuf[readBufSize];

#define sendBufSize 256*4
static char sendBuf[sendBufSize];


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
 * Structure holding server handle and internal socket fd
 * in order to use out of request send
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

/*
 * Struct with information to uniquely identify a websocket
 */

struct websock_instance {
  httpd_handle_t handle;
  int descriptor;
};

// Websocket instance for browser client
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

esp_err_t send_sensor_update_frame(char *buffer){
	httpd_ws_frame_t ws_frame = {
		.final = true,
		.fragmented = false,
		.type = HTTPD_WS_TYPE_TEXT,
		.payload = (uint8_t*)buffer,
		.len = strlen(buffer)
	};

	ESP_ERROR_CHECK(httpd_ws_send_frame_async( websock_client.handle, websock_client.descriptor, &ws_frame ));
  return ESP_OK;
}

int16_t do_checksum(int8_t *buffer, size_t size){
	int16_t sum = 0;

	for (size_t i = 0; i < size; i++)
	{
		sum += buffer[i];
	}
	return(sum);
}

time_t time_last_log = 0;
int16_t checksum_last = 0;

/*
 * Send updates to client
 */

void send_sensor_update(){
	TickType_t xPreviousWakeTime;
	const TickType_t xTimeIncrement = pdMS_TO_TICKS(WS_UPDATE_TASK_DELAY_MS);
	BaseType_t xWasDelayed;
	time_t now;
	int16_t checksum;
	cJSON *root;
	char *json_string;

	root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "time", now);
	cJSON_AddNumberToObject(root, "target", heater_status.target);
	cJSON_AddNumberToObject(root, "env", heater_status.env);
	cJSON_AddNumberToObject(root, "top", heater_status.top);
	cJSON_AddNumberToObject(root, "bot", heater_status.bot);
	cJSON_AddNumberToObject(root, "chip", heater_status.chip);
	cJSON_AddNumberToObject(root, "rem", heater_status.rem);
	cJSON_AddNumberToObject(root, "voltage", heater_status.voltage);
	cJSON_AddNumberToObject(root, "current", heater_status.current);
	cJSON_AddNumberToObject(root, "power", heater_status.power);
	cJSON_AddNumberToObject(root, "energy", heater_status.energy);
	cJSON_AddNumberToObject(root, "pf", heater_status.pf);
	cJSON_AddNumberToObject(root, "web", heater_status.web);
	cJSON_AddBoolToObject(root, "one_set", heater_status.one_set);
	cJSON_AddBoolToObject(root, "one_pwr", heater_status.one_pwr);
	cJSON_AddBoolToObject(root, "two_set", heater_status.two_set);
	cJSON_AddBoolToObject(root, "two_pwr", heater_status.two_pwr);
	cJSON_AddBoolToObject(root, "safe", heater_status.safe);

	// Initialise the xLastWakeTime variable with the current time.
	xPreviousWakeTime = xTaskGetTickCount ();

	do{
		// Wait for the next cycle.
		xWasDelayed = xTaskDelayUntil( &xPreviousWakeTime, xTimeIncrement );

		if( xWasDelayed == pdFALSE ){
			ESP_LOGE( TAG, "Task ran out of time" );
		}

		if(websock_client.handle != NULL){
		time(&now);
		if(now > (time_last_log + 1)){				// Max 1 packet per second
			time_last_log = now;
			checksum = do_checksum((int8_t *) &heater_status, sizeof(heater_status));		// Check if status has changed
			//ESP_LOGI( TAG, "Checksum: %d, last checksum: %d stack %d", checksum, checksum_last, uxTaskGetStackHighWaterMark(NULL) );
			
			if(checksum_last != checksum){
				heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);


				checksum_last = checksum;
				cJSON_SetNumberValue(cJSON_GetObjectItem(root, "time"), now);
				cJSON_SetNumberValue(cJSON_GetObjectItem(root, "target"), heater_status.target);
				cJSON_SetNumberValue(cJSON_GetObjectItem(root, "env"), heater_status.env);
				cJSON_SetNumberValue(cJSON_GetObjectItem(root, "top"), heater_status.top);
				cJSON_SetNumberValue(cJSON_GetObjectItem(root, "bot"), heater_status.bot);
				cJSON_SetNumberValue(cJSON_GetObjectItem(root, "chip"), heater_status.chip);
				cJSON_SetNumberValue(cJSON_GetObjectItem(root, "rem"), heater_status.rem);
				cJSON_SetNumberValue(cJSON_GetObjectItem(root, "voltage"), heater_status.voltage);
				cJSON_SetNumberValue(cJSON_GetObjectItem(root, "current"), heater_status.current);
				cJSON_SetNumberValue(cJSON_GetObjectItem(root, "power"), heater_status.power);
				cJSON_SetNumberValue(cJSON_GetObjectItem(root, "energy"), heater_status.energy);
				cJSON_SetNumberValue(cJSON_GetObjectItem(root, "pf"), heater_status.pf);
				cJSON_SetNumberValue(cJSON_GetObjectItem(root, "web"), heater_status.web);
				cJSON_SetBoolValue(cJSON_GetObjectItem(root, "one_set"), heater_status.one_set);
				cJSON_SetBoolValue(cJSON_GetObjectItem(root, "one_pwr"), heater_status.one_pwr);
				cJSON_SetBoolValue(cJSON_GetObjectItem(root, "two_set"), heater_status.two_set);
				cJSON_SetBoolValue(cJSON_GetObjectItem(root, "two_pwr"), heater_status.two_pwr);
				cJSON_SetBoolValue(cJSON_GetObjectItem(root, "safe"), heater_status.safe);
				json_string = cJSON_Print(root);
				//ESP_LOGI( TAG, "cJSON string %s length: %d", json_string, strlen(json_string) );
				//strcpy(sendBuf, json_string);
				//ESP_LOGI( TAG, "sendBuf      %s", json_string );
				send_sensor_update_frame(json_string);
				ESP_LOGI( TAG, "Send." );
			}
		}
		}

		// Send only if we have both (1) an update, and (2) a client to send to.

		if( heater_status.web && websock_client.handle != NULL ){
			if( heater_status.web & REM_W_FL ){
				heater_status.web &= ~REM_W_FL;
				sprintf( sendBuf, "r%2.01f", heater_status.rem );
				send_sensor_update_frame(sendBuf);
			}else if( heater_status.web & ENV_W_FL ){
				heater_status.web &= ~ENV_W_FL;
				sprintf( sendBuf, "e%2.01f", heater_status.env );
				send_sensor_update_frame(sendBuf);
			}else if( heater_status.web & TOP_W_FL ){
				heater_status.web &= ~TOP_W_FL;
				sprintf( sendBuf, "t%2.01f", heater_status.top );
				send_sensor_update_frame(sendBuf);
			}else if( heater_status.web & BOT_W_FL ){
				heater_status.web &= ~BOT_W_FL;
				sprintf( sendBuf, "b%2.01f", heater_status.bot );
				send_sensor_update_frame(sendBuf);
			}else if( heater_status.web & CHP_W_FL ){
				heater_status.web &= ~CHP_W_FL;
				sprintf( sendBuf, "c%2.01f", heater_status.chip );
				send_sensor_update_frame(sendBuf);
			}else if( heater_status.web & TAR_W_FL ){
				heater_status.web &= ~TAR_W_FL;
				sprintf( sendBuf, "a%2.01f", heater_status.target );
				send_sensor_update_frame(sendBuf);
			}else if( heater_status.web & ONE_W_FL ){
				heater_status.web &= ~ONE_W_FL;
				sprintf( sendBuf, "i%d", heater_status.one_pwr );
				send_sensor_update_frame(sendBuf);
			}else if( heater_status.web & TWO_W_FL ){
				heater_status.web &= ~TWO_W_FL;
				sprintf( sendBuf, "h%d", heater_status.two_pwr );
				send_sensor_update_frame(sendBuf);
			}
		}
  }while (true);
}

/*
 * URI handler for WebSocket
 */

static esp_err_t get_ws_handler( httpd_req_t *req )
{
	if ( req->method == HTTP_GET ) {
		ESP_LOGI( TAG, "Handshake done, the new connection was opened" );

		if ( websock_client.handle == NULL ) {
			// We didn't have a client before, now we do. Set up notification so
			// we know when it goes away.
			ESP_LOGI( TAG, "Have a new client" );
			websock_client.handle = req->handle;
			websock_client.descriptor = httpd_req_to_sockfd( req );
			req->sess_ctx = (void*)1; // Set to nonzero otherwise free_ctx won't get called.
			req->free_ctx = socket_close_cleanup;
		} else if ( websock_client.handle != req->handle || websock_client.descriptor != httpd_req_to_sockfd( req ) ) {
			ESP_LOGI( TAG, "Already have ja client, reject connection attempt." );
			return ESP_FAIL;
		}
		return ESP_OK;
	}

	httpd_ws_frame_t ws_pkt;
	uint8_t *buf = NULL;
	memset( &ws_pkt, 0, sizeof( httpd_ws_frame_t ));
	ws_pkt.type = HTTPD_WS_TYPE_TEXT;
	/* Set max_len = 0 to get the frame len */
	esp_err_t ret = httpd_ws_recv_frame( req, &ws_pkt, 0 );
	if ( ret != ESP_OK ) {
		ESP_LOGE( TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret );
		return ret;
	}
	//ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
	if ( ws_pkt.len ) {
		/* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
		buf = calloc( 1, ws_pkt.len + 1 );
		if ( buf == NULL ) {
			ESP_LOGE( TAG, "Failed to calloc memory for buf" );
			return ESP_ERR_NO_MEM;
		}
		ws_pkt.payload = buf;
		/* Set max_len = ws_pkt.len to get the frame payload */
		ret = httpd_ws_recv_frame( req, &ws_pkt, ws_pkt.len );
		if ( ret != ESP_OK ) {
			ESP_LOGE( TAG, "httpd_ws_recv_frame failed with %d", ret );
			free( buf );
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
					heater_status.one_set = !heater_status.one_set;
					heater_status.web |= ONE_W_FL;
					break;

				case 'H':
					heater_status.two_set = !heater_status.two_set;
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


/*
 * URI handler for main server page
 */

esp_err_t get_index_handler(httpd_req_t *req)
{
	size_t readSize = 0;
	//ESP_ERROR_CHECK(httpd_resp_set_hdr(req, "cache-control", "max-age=1")); // For development
	ESP_ERROR_CHECK(httpd_resp_set_type(req, "text/html"));
	readSize = read_spiff_buffer("/data/index.html");
	ESP_ERROR_CHECK(httpd_resp_send(req, readBuf, readSize));

	heater_status.web = TAR_W_FL | ONE_W_FL | TWO_W_FL;		// force update once

	return ESP_OK;
}

#define IS_FILE_EXT(filename, ext) \
    (strcasecmp(&filename[strlen(filename) - sizeof(ext) + 1], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filename)
{
    if (IS_FILE_EXT(filename, ".html")) {
        return httpd_resp_set_type(req, "text/html");
    } else if (IS_FILE_EXT(filename, ".css")) {
        return httpd_resp_set_type(req, "text/css");
    } else if (IS_FILE_EXT(filename, ".js")) {
        return httpd_resp_set_type(req, "application/x-javascript");
    } else if (IS_FILE_EXT(filename, ".jpeg")) {
        return httpd_resp_set_type(req, "image/jpeg");
    } else if (IS_FILE_EXT(filename, ".svg")) {
        return httpd_resp_set_type(req, "image/svg+xml");
    } else if (IS_FILE_EXT(filename, ".ico")) {
        return httpd_resp_set_type(req, "image/x-icon");
    }
    /* This is a limited set only */
    /* For any other type always set as plain text */
    return httpd_resp_set_type(req, "text/plain");
}

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

/* Handler to download a file kept on the server */
static esp_err_t download_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    FILE *fd = NULL;
    struct stat file_stat;

    const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri, sizeof(filepath));
    if (!filename) {
        ESP_LOGE(TAG, "Filename is too long");
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
        return ESP_FAIL;
    }

    if (stat(filepath, &file_stat) == -1) {
        /* If file not present on SPIFFS check if URI
         * corresponds to one of the hardcoded paths */
        if (strcmp(filename, "/index.html") == 0) {
            return get_index_handler(req);
        }
        ESP_LOGE(TAG, "Failed to stat file : %s", filepath);
        /* Respond with 404 Not Found */
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    set_content_type_from_file(req, filename);

    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
    size_t chunksize;
    do {
        /* Read file in chunks into the scratch buffer */
        chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

        if (chunksize > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
               return ESP_FAIL;
           }
        }

        /* Keep looping till the whole file is sent */
    } while (chunksize != 0);

    /* Close file after sending complete */
    fclose(fd);
    ESP_LOGI(TAG, "File sending complete");

    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

/*
 * Webserver
 */

httpd_handle_t start_webserver(void)
{
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	httpd_handle_t server = NULL;

    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;

	// Initialize structure tracking websocket to client
	websock_client.handle = NULL;
	websock_client.descriptor = 0;

    static struct file_server_data *server_data = NULL;

    if (server_data) {
        ESP_LOGE(TAG, "File server already started");		// ESP_ERR_INVALID_STATE
        return NULL;
    }

    /* Allocate memory for server data */
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for server data");		// ESP_ERR_NO_MEM
        return NULL;
    }
    strlcpy(server_data->base_path, base_path,
            sizeof(server_data->base_path));
	
	ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
	if (httpd_start(&server, &config) == ESP_OK) {			// Start the httpd server
		ESP_LOGI(TAG, "Registering URI handlers");

		static httpd_uri_t get_index = {                    // URI handler for index
   		    .uri = "/index.html",
   		    .method = HTTP_GET,
   		    .handler = get_index_handler,
   		    .user_ctx = NULL};
		httpd_register_uri_handler(server, &get_index);

		static httpd_uri_t get_root = {                    	// URI handler for root
   	        .uri = "/",
           	.method = HTTP_GET,
           	.handler = get_index_handler,
           	.user_ctx = NULL};
		httpd_register_uri_handler(server, &get_root);

		static const httpd_uri_t get_ws = {                 // URI handler for websocket
            .uri        = "/ws",
            .method     = HTTP_GET,
            .handler    = get_ws_handler,
            .user_ctx   = NULL,
            .is_websocket = true};
		httpd_register_uri_handler(server, &get_ws);

		
        httpd_uri_t file_download = {                       // URI handler for getting uploaded files
			.uri       = "/*",                              // Match all URIs of type /path/to/file
			.method    = HTTP_GET,
			.handler   = download_get_handler,
			.user_ctx  = server_data                        // Pass server data as context
			};
		httpd_register_uri_handler(server, &file_download);

		xTaskCreate( send_sensor_update, "sensor update", 4096+1024, NULL, WS_UPDATE_TASK_PRIORITY, NULL );
		return server;
	}
	ESP_LOGI(TAG, "Error starting server!");
	return NULL;
}
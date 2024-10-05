#include <stdio.h>
#include <stdlib.h>
#include <string.h>					//Requires by memset
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "spi_flash_mmap.h"
#include <sys/stat.h>
#include <esp_http_server.h>
#include "nvs_flash.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"

#include "mount.h"
#include "task_priorities"
#include "heater.h"
#include "lvgl_ui.h"
#include "json.h"

#define HEATER_WEB_STEP 0.1

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
//char index_html[4096];
//char response_data[4096];

// We can spend effort making code memory-efficient, or we can just blow
// a chunk of RAM to make code trivial.

#define readBufSize 1024*16
static char readBuf[readBufSize];

//#define sendBufSize 256
//static char sendBuf[sendBufSize];


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
 * Struct with information to uniquely identify a websocket
 */

struct websock_instance {
  httpd_handle_t handle;
  int descriptor;
};

#define MAX_WEBSOCK_CLIENTS 4

// Websocket instance for multiple browser clients
static struct websock_instance websock_clients[MAX_WEBSOCK_CLIENTS];

/*
 * @brief WebSocket has been closed, clean up associated data structures
 * @param context Unused here, but had to be nonzero for this callback to be called upon socket close
 */

void socket_close_cleanup(struct websock_instance* context){
	ESP_LOGI(TAG, "Lost our client handler.");
	context->handle = NULL;
	context->descriptor = 0;
}

esp_err_t send_sensor_update_frame(char *buffer, size_t client){
	httpd_ws_frame_t ws_frame = {
		.final = true,
		.fragmented = false,
		.type = HTTPD_WS_TYPE_TEXT,
		.payload = (uint8_t*)buffer,
		.len = strlen(buffer)
	};

	ESP_ERROR_CHECK(httpd_ws_send_frame_async( websock_clients[client].handle, websock_clients[client].descriptor, &ws_frame ));
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

static time_t next_log_time = 0;
static int16_t checksum_last = 0;
static char* json_string;

/*
 * Send updates to client
 */

void send_sensor_update(){
	TickType_t xPreviousWakeTime;
	const TickType_t xTimeIncrement = pdMS_TO_TICKS(WS_UPDATE_TASK_DELAY_MS);
	BaseType_t xWasDelayed;
	time_t now;
	int16_t checksum;

	// Initialise the xLastWakeTime variable with the current time.
	xPreviousWakeTime = xTaskGetTickCount ();

	do{
		// Wait for the next cycle.
		xWasDelayed = xTaskDelayUntil( &xPreviousWakeTime, xTimeIncrement );

		if( xWasDelayed == pdFALSE ){
			ESP_LOGE( TAG, "Task ran out of time" );
		}
		if(websock_clients[0].handle != NULL
		|| websock_clients[1].handle != NULL
		|| websock_clients[2].handle != NULL
		|| websock_clients[3].handle != NULL){

			time(&now);
			if(now > next_log_time){				// Max 1 packet per second
				next_log_time = now + 1;
				checksum = do_checksum((int8_t *) &heater_status, sizeof(heater_status));		// Check if status has changed
				
				if(checksum_last != checksum){
					checksum_last = checksum;

					for (size_t i = 0; i < MAX_WEBSOCK_CLIENTS; i++)
					{
						if (websock_clients[i].handle != NULL)
						{
							json_string = json_update();
							ESP_LOGD( TAG, "json_str: %s, length: %d", json_string, strlen(json_string) );
							send_sensor_update_frame(json_string, i);
						}
					}
				}
			}
		}
  }while (true);
}

/*
 * URI handler for WebSocket
 */

static esp_err_t get_ws_handler( httpd_req_t *req )
{
	size_t i;

	if ( req->method == HTTP_GET ) {
		ESP_LOGI( TAG, "Handshake done, the new connection was opened" );

		for (i = 0; i < MAX_WEBSOCK_CLIENTS; i++)
		{
			if( websock_clients[i].handle == req->handle && websock_clients[i].descriptor == httpd_req_to_sockfd( req ) ){		// client already connected
				return ESP_OK;
			}
		}
		for (i = 0; i < MAX_WEBSOCK_CLIENTS; i++)
		{
			if( websock_clients[i].handle == NULL ){
				ESP_LOGI( TAG, "Have a new client, %d now", i + 1 );
				websock_clients[i].handle = req->handle;
				websock_clients[i].descriptor = httpd_req_to_sockfd( req );
				req->sess_ctx = (void*) &websock_clients[i];
				req->free_ctx = (void *) socket_close_cleanup;
				next_log_time = 0;		// send update immediately
				return ESP_OK;
			}
		}
		ESP_LOGI( TAG, "Already have 4 clients, reject connection attempt." );
		return ESP_FAIL;
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
				heater_status.update = true;
				next_log_time = 0;		// send update immediately
				break;

			case 'U':
				heater_status.target += HEATER_WEB_STEP;
				heater_status.update = true;
				next_log_time = 0;		// send update immediately
				break;
			
			case 'S':
#ifdef ENABLE_LOG
				if(strcmp((char *) ws_pkt.payload, "SavePoints") == 0){
					stats_save();
				}
#endif
				break;
		}
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

/* Handler to upload a file onto the server */
static esp_err_t upload_post_handler(httpd_req_t *req)
{
	 char filepath[FILE_PATH_MAX];
	 FILE *fd = NULL;
	 struct stat file_stat;

	 /* Skip leading "/upload" from URI to get filename */
	 /* Note sizeof() counts NULL termination hence the -1 */
	 const char *filename = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
															req->uri + sizeof("/upload") - 1, sizeof(filepath));
	 if (!filename) {
		  /* Respond with 500 Internal Server Error */
		  httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
		  return ESP_FAIL;
	 }

	 /* Filename cannot have a trailing '/' */
	 if (filename[strlen(filename) - 1] == '/') {
		  ESP_LOGE(TAG, "Invalid filename : %s", filename);
		  httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Invalid filename");
		  return ESP_FAIL;
	 }

	 if (stat(filepath, &file_stat) == 0) {
		  ESP_LOGE(TAG, "File already exists : %s", filepath);
		  /* Respond with 400 Bad Request */
		  httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "File already exists");
		  return ESP_FAIL;
	 }

	 /* File cannot be larger than a limit */
	 if (req->content_len > MAX_FILE_SIZE) {
		  ESP_LOGE(TAG, "File too large : %d bytes", req->content_len);
		  /* Respond with 400 Bad Request */
		  httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST,
									 "File size must be less than "
									 MAX_FILE_SIZE_STR "!");
		  /* Return failure to close underlying connection else the
			* incoming file content will keep the socket busy */
		  return ESP_FAIL;
	 }

	 fd = fopen(filepath, "w");
	 if (!fd) {
		  ESP_LOGE(TAG, "Failed to create file : %s", filepath);
		  /* Respond with 500 Internal Server Error */
		  httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to create file");
		  return ESP_FAIL;
	 }

	 ESP_LOGI(TAG, "Receiving file : %s...", filename);

	 /* Retrieve the pointer to scratch buffer for temporary storage */
	 char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
	 int received;

	 /* Content length of the request gives
	  * the size of the file being uploaded */
	 int remaining = req->content_len;

	 while (remaining > 0) {

		  ESP_LOGI(TAG, "Remaining size : %d", remaining);
		  /* Receive the file part by part into a buffer */
		  if ((received = httpd_req_recv(req, buf, MIN(remaining, SCRATCH_BUFSIZE))) <= 0) {
				if (received == HTTPD_SOCK_ERR_TIMEOUT) {
					 /* Retry if timeout occurred */
					 continue;
				}

				/* In case of unrecoverable error,
				 * close and delete the unfinished file*/
				fclose(fd);
				unlink(filepath);

				ESP_LOGE(TAG, "File reception failed!");
				/* Respond with 500 Internal Server Error */
				httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
				return ESP_FAIL;
		  }

		  /* Write buffer content to file on storage */
		  if (received && (received != fwrite(buf, 1, received, fd))) {
				/* Couldn't write everything to file!
				 * Storage may be full? */
				fclose(fd);
				unlink(filepath);

				ESP_LOGE(TAG, "File write failed!");
				/* Respond with 500 Internal Server Error */
				httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to write file to storage");
				return ESP_FAIL;
		  }

		  /* Keep track of remaining size of
			* the file left to be uploaded */
		  remaining -= received;
	 }

	 /* Close file upon upload completion */
	 fclose(fd);
	 ESP_LOGI(TAG, "File reception complete");

	 /* Redirect onto root to see the updated file list */
//    httpd_resp_set_status(req, "303 See Other");
//    httpd_resp_set_hdr(req, "Location", "/");
//    httpd_resp_sendstr(req, "File uploaded successfully");
	 return ESP_OK;
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

	// Initialize structure tracking websockets to clients
	memset( &websock_clients, 0, sizeof(websock_clients) );

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

		
		  httpd_uri_t file_download = {                    // URI handler for getting uploaded files
			.uri       = "/*",                              // Match all URIs of type /path/to/file
			.method    = HTTP_GET,
			.handler   = download_get_handler,
			.user_ctx  = server_data                        // Pass server data as context
			};
		httpd_register_uri_handler(server, &file_download);

		httpd_uri_t file_upload = {								// URI handler for uploading files to server

		  .uri       = "/upload/*",								// Match all URIs of type /upload/path/to/file
		  .method    = HTTP_POST,
		  .handler   = upload_post_handler,
		  .user_ctx  = server_data									// Pass server data as context
		};
		httpd_register_uri_handler(server, &file_upload);

		xTaskCreate( send_sensor_update, "sensor update", 4096+1024, NULL, WS_UPDATE_TASK_PRIORITY, NULL );
		return server;
	}
	ESP_LOGI(TAG, "Error starting server!");
	return NULL;
}
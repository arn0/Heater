/*
 * Log sersor values and system status to memory
 */

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include "esp_system.h"
#include "esp_spiffs.h"
#include "esp_log.h"

#include "heater.h"
#include "logger.h"

#ifdef ENABLE_LOG

static const char *TAG = "logger";

static bool log_full = false;
static int16_t log_counter = 0;
static struct snapshot log_base[SNAPSHOT_BUFFER_SIZE];
static const char filepath[] = "/data/log.bin";

esp_err_t log_add(){
	time(&(log_base[log_counter].time));
	log_base[log_counter].target = heater_status.target;
	log_base[log_counter].fnt = heater_status.fnt;
	log_base[log_counter].bck = heater_status.bck;
	log_base[log_counter].bot = heater_status.bot;
	log_base[log_counter].top = heater_status.top;
	log_base[log_counter].chip = heater_status.chip;
	log_base[log_counter].rem = heater_status.rem;
	log_base[log_counter].one_set = heater_status.one_set;
	log_base[log_counter].one_pwr = heater_status.one_pwr;
	log_base[log_counter].two_set = heater_status.two_set;
	log_base[log_counter].two_pwr = heater_status.two_pwr;
	log_base[log_counter].safe = heater_status.safe;

	if (log_counter < SNAPSHOT_BUFFER_SIZE)
	{
		log_counter++;
	}else{
		log_counter = 0;
		log_full = true;
	}
	return(ESP_OK);
}

esp_err_t log_save(){
   struct stat file_stat;
	FILE *fd = NULL;
	size_t filesize;
	void *split;

	if(log_counter == 0){
		ESP_LOGW(TAG, "Log buffer empty!");
      return ESP_FAIL;
	}

	if(stat(filepath, &file_stat) == 0){										// Is there a previous log file?
		ESP_LOGI(TAG, "Deleting file : %s", filepath);
		unlink(filepath);																// Delete it
	}
   fd = fopen(filepath, "w");
   if(!fd) {
      ESP_LOGE(TAG, "Failed to create file : %s", filepath);
      return ESP_FAIL;
   }
	if(!log_full){																		// Write all in sequence
		filesize = sizeof(log_base[0]) * log_counter;
		ESP_LOGI(TAG, "Writing file : %s %d bytes", filepath, filesize);
		if(fwrite(log_base, 1, filesize, fd) != filesize){					// Write buffer content to file on storage
			fclose(fd);																	// Couldn't write everything to file! Storage may be full?
			unlink(filepath);	
			ESP_LOGE(TAG, "File write failed!");
			return ESP_FAIL;
		}
	}else{																				// Split write
		split = &log_base[log_counter];											// First part from log_last + 1 to end off buffer
		filesize = sizeof(log_base) - sizeof(log_base[0]) * (SNAPSHOT_BUFFER_SIZE - log_counter);
      ESP_LOGI(TAG, "Split write to file : %s, %d bytes", filepath, filesize);
		if(fwrite(split, 1, filesize, fd) != filesize){
			fclose(fd);
			unlink(filepath);	
			ESP_LOGE(TAG, "File write failed!");
			return ESP_FAIL;
		}
		filesize = sizeof(log_base[0]) * log_counter;
      ESP_LOGI(TAG, "Split write to file : %s, %d bytes", filepath, filesize);
		if(fwrite(log_base, 1, filesize, fd) != filesize){
			fclose(fd);
			unlink(filepath);	
			ESP_LOGE(TAG, "File write failed!");
			return ESP_FAIL;
		}
	}
	fclose(fd);
	return(ESP_OK);
}

int16_t count;
int16_t start;
int16_t gaps;

esp_err_t log_read(){
   struct stat file_stat;
	FILE *fd = NULL;
	size_t filesize = 0;
	time_t now;

	if(stat(filepath, &file_stat) == 0){						// Is there a previous log file?
		fd = fopen(filepath, "r");
		if (!fd) {
			ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
			return ESP_FAIL;
		}
		filesize = fread(log_base, 1, sizeof(log_base), fd);
		if(filesize < sizeof(log_base[0])){
			ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
			return ESP_FAIL;
		}
		ESP_LOGI(TAG, "Read existing file : %s, %d bytes", filepath, filesize);
		if(filesize == sizeof(log_base)){
			log_full = true;
			log_counter = SNAPSHOT_BUFFER_SIZE / sizeof(log_base[0]);
		}else{
			log_full = false;
			log_counter = sizeof(log_base) / sizeof(log_base[0]);
		}
		time(&now);
		if(now - 24*60*60 > log_base[log_counter].time){
			log_full = false;
			log_counter = 0;
			return ESP_FAIL;
		}else{
			gaps = (now - log_base[log_counter].time) / 60;
			start = log_counter - gaps;
		}
		return(ESP_OK);
	}
	return ESP_FAIL;
}

int16_t log_fill(){
	while(gaps--)
	{
		return((int16_t) (heater_status.fnt * 100));
	}
		return((int16_t) (heater_status.fnt * 100));
	
}
#endif
#include <sys/unistd.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spiffs.h"
#include "esp_log.h"

#include "task_priorities.h"
#include "control.h"
#include "heater.h"
#include "lvgl_ui.h"

static const char *TAG = "control_task";
static const char filepath[] = "/data/target.bin";

esp_err_t read_target( float *target ) {
   struct stat file_stat;
   FILE *fd = NULL;
   uint8_t buffer[4];

   if ( stat( filepath, &file_stat ) == 0 ) { // Is there a file?
      fd = fopen( filepath, "r" );
      if ( !fd ) {
         ESP_LOGE( TAG, "Failed to open existing file : %s", filepath );
         return ESP_FAIL;
      }
      if ( fread( buffer, 1, 4, fd ) < 4 ) {
         ESP_LOGE( TAG, "Failed to read existing file : %s", filepath );
         return ESP_FAIL;
      }
      ESP_LOGD( TAG, "Read existing file : %s, %d bytes", filepath, 4 );
      fclose( fd );
      ESP_LOGV( TAG, "Buffer : %02hhX%02hhX%02hhX%02hhX", buffer[3], buffer[2], buffer[1], buffer[0] );
      *target = *(float *)buffer;
      return ( ESP_OK );
   }
   ESP_LOGE( TAG, "File not found : %s", filepath );
   return ESP_FAIL;
}

esp_err_t write_target( float target ) {
   struct stat file_stat;
   FILE *fd = NULL;
   uint8_t buffer[4];

   *(float *)buffer = target;
   ESP_LOGV( TAG, "Buffer : %02hhX%02hhX%02hhX%02hhX", buffer[3], buffer[2], buffer[1], buffer[0] );

   if ( stat( filepath, &file_stat ) == 0 ) { // Is there a file?
      ESP_LOGD( TAG, "Deleting file : %s", filepath );
      unlink( filepath ); // Delete it
   }
   fd = fopen( filepath, "w" );
   if ( !fd ) {
      ESP_LOGE( TAG, "Failed to create file : %s", filepath );
      return ESP_FAIL;
   }
   ESP_LOGD( TAG, "Writing file : %s %d bytes", filepath, 4 );
   if ( fwrite( buffer, 1, 4, fd ) != 4 ) { // Write buffer content to file on storage
      fclose( fd );                         // Couldn't write everything to file! Storage may be full?
      unlink( filepath );
      ESP_LOGE( TAG, "File write failed!" );
      return ESP_FAIL;
   }
   fclose( fd );
   return ( ESP_OK );
}

/*
 * Control loop
 */

void control_task() {
   TickType_t xPreviousWakeTime;
   const TickType_t xTimeIncrement = pdMS_TO_TICKS( CONTROL_TASK_DELAY_MS );
   BaseType_t xWasDelayed;
   float delta;

   // Initialise the xLastWakeTime variable with the current time.
   xPreviousWakeTime = xTaskGetTickCount();

   do {
      // Wait for the next cycle.
      xWasDelayed = xTaskDelayUntil( &xPreviousWakeTime, xTimeIncrement );

      if ( xWasDelayed == pdFALSE ) {
         ESP_LOGW( TAG, "Task was not delayed" );
      }

      heater_status.safe = true;
      if ( heater_status.update ) {
         if ( write_target( heater_status.target ) == ESP_OK ) {
            heater_status.update = false;
         }
      }

      // here we need to implement a feedback control loop
      // now just for testing

      if ( heater_status.blue ) {
         delta = heater_status.target - heater_status.rem;
      } else {
         delta = 0;
      }
      // Check for maximum temperature
      if ( heater_status.fnt >= INTERNAL_MAX_TEMP || heater_status.bck >= MAX_TEMP || heater_status.top >= MAX_TEMP || heater_status.bot >= MAX_TEMP || heater_status.chip >= MAX_TEMP ) {
         delta = 0.0;
      }
      if ( delta <= 0.0 ) {
         heater_status.one_on = false;
         heater_status.two_on = false;
      } else if ( delta > 0.4 ) {
         heater_status.one_on = true;
         heater_status.two_on = true;
      } else if ( delta > 0.2 ) {
         heater_status.one_on = false;
         heater_status.two_on = true;
      } else {
         heater_status.one_on = true;
         heater_status.two_on = false;
      }
   } while ( true );
}

bool control_task_start() {

   if ( read_target( &heater_status.target ) != ESP_OK ) {
      heater_status.target = TARGET_DEFAULT;
   }
   heater_status.safe = true;

   // Now we start the contol loop

   xTaskCreate( control_task, "control_task", 4096, NULL, CONTROL_TASK_PRIORITY, NULL );

   return ( true );
}
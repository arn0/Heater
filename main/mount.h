#ifndef MOUNT_H
#define MOUNT_H

#define MAX_OPEN_FILES 5
#ifdef __cplusplus
extern "C" {
#endif

esp_err_t spiffs_init( const char *base_path );

#ifdef __cplusplus
}
#endif

#endif // MOUNT_H
#ifndef CONTROL_H
#define CONTROL_H

#define TARGET_DEFAULT 20.5
#define MAX_TEMP 100.0
#define INTERNAL_MAX_TEMP 70     // As original was 0 - 70 Celsius

#ifdef __cplusplus
extern "C" {
#endif

bool control_task_start();

#ifdef __cplusplus
}
#endif

#endif //CONTROL_H
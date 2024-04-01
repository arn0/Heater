#ifndef CLOCK_H
#define CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

extern char strftime_buf[64];

void clock_start(void);

#ifdef __cplusplus
}
#endif

#endif //CLOCK_H
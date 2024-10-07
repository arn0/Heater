#ifndef SNTP_H
#define SNTP_H

#ifdef __cplusplus
extern "C" {
#endif

void timezone_set();
void sntp_client_start();
void sntp_client_stop();

#ifdef __cplusplus
}
#endif

#endif // SNTP_H
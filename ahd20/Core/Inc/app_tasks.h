#ifndef __APP_TASKS_H__
#define __APP_TASKS_H__

#include <stdint.h>

typedef struct {
    uint8_t  mode;
    uint8_t  aht20_ok;
    int      wifi_ok;
    int      cloud_ok;
    float    temp;
    float    humi;
    uint32_t aht20_last_ms;
    uint32_t post_last_ms;
    uint32_t reconnect_last_ms;
    int      msg_id;
} AppState;

void App_Run(uint8_t aht20_ok, int wifi_ok, int cloud_ok);

#endif /* __APP_TASKS_H__ */

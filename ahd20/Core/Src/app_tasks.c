#include "app_tasks.h"
#include "stm32f1xx_hal.h"
#include "pins.h"
#include "bsp_LCD_ILI9341.h"
#include "onenet_mqtt.h"
#include "esp8266.h"
#include "aht20_soft.h"
#include <stdio.h>

/* ---- 用户配置宏 ---- */
#define ONENET_FIELD_TEMP   "SHT30_T"
#define ONENET_FIELD_HUMI   "SHT30_H"

/* ---- 内部辅助 ---- */
extern UART_HandleTypeDef huart1;

/* ---- 任务函数 ---- */

static void Console_Task(AppState *s)
{
    uint8_t c;
    if (HAL_UART_Receive(&huart1, &c, 1, 0) == HAL_OK) {
        if (c != '\r' && c != '\n') {
            uint8_t new_mode = 0xFF;
            if      (c == '1') new_mode = 1;
            else if (c == '2') new_mode = 2;
            else if (c == '3') new_mode = 3;
            else if (c == '0') new_mode = 0;
            if (new_mode != 0xFF) {
                s->mode = new_mode;
                LED_BLUE_OFF(); LED_RED_OFF();
                if (s->mode == 1 || s->mode == 3) LED_BLUE_ON();
                if (s->mode == 2 || s->mode == 3) LED_RED_ON();
            }
        }
    }
}

static void Sensor_Task(AppState *s)
{
    if (HAL_GetTick() - s->aht20_last_ms < 2000) return;
    s->aht20_last_ms = HAL_GetTick();

    if (!s->aht20_ok) return;

    if (AHT20_SoftRead(&s->temp, &s->humi) == 0) {
        char b[32];
        int ti = (int)(s->temp * 10.0f);
        int hi = (int)(s->humi * 10.0f);
        int ta = (ti > 0) ? ti : -ti;
        int ha = (hi > 0) ? hi : -hi;

        LCD_Fill(0, 60, 239, 125, BLACK);
        snprintf(b, sizeof(b), "%s%d.%d C", (ti < 0) ? "-" : "", ta / 10, ta % 10);
        LCD_String(10, 80, b, 24, YELLOW, BLACK);

        LCD_Fill(0, 180, 239, 245, BLACK);
        snprintf(b, sizeof(b), "%d.%d %%RH", ha / 10, ha % 10);
        LCD_String(10, 200, b, 24, CYAN, BLACK);

        printf("[AHT20] T=%.1f C  H=%.1f %%RH\r\n", s->temp, s->humi);
    } else {
        printf("[AHT20] read failed, skip\r\n");
    }
}

static void Cloud_Task(AppState *s)
{
    if (!s->cloud_ok || !s->aht20_ok) return;
    if (HAL_GetTick() - s->post_last_ms < 5000) return;
    s->post_last_ms = HAL_GetTick();

    int rc = OneNET_PublishDataPoint(s->msg_id,
                                      ONENET_FIELD_TEMP, (double)s->temp,
                                      ONENET_FIELD_HUMI, (double)s->humi);
    if (rc == 0) {
        printf("[SYS] publish OK, id=%d\r\n", s->msg_id);
        s->msg_id++;
    } else {
        printf("[SYS] publish FAIL\r\n");
        s->cloud_ok = 0;
        LCD_String(50, 252, "LOST", 12, RED, BROWN);
    }
}

static void Reconnect_Task(AppState *s)
{
    if (s->cloud_ok || !s->wifi_ok) return;
    if (HAL_GetTick() - s->reconnect_last_ms < 10000) return;
    s->reconnect_last_ms = HAL_GetTick();

    printf("[SYS] Reconnecting OneNET...\r\n");
    LCD_String(50, 252, "....", 12, YELLOW, BROWN);

    printf("[SYS] Testing ESP8266 with Reset...\r\n");
    ESP8266_Status test_s = ESP8266_Reset();
    printf("[SYS] ESP8266 Reset result: %d\r\n", (int)test_s);

    OneNET_Disconnect();
    HAL_Delay(500);

    if (OneNET_Connect() == 0) {
        s->cloud_ok = 1;
        LCD_String(50, 252, "OK   ", 12, GREEN, BROWN);
        printf("[SYS] OneNET reconnected!\r\n");
    } else {
        LCD_String(50, 252, "FAIL ", 12, RED, BROWN);
        printf("[SYS] Reconnect failed\r\n");
    }
}

/* ---- 主循环入口 ---- */
void App_Run(uint8_t aht20_ok, int wifi_ok, int cloud_ok)
{
    AppState s = {0};
    s.aht20_ok = aht20_ok;
    s.wifi_ok  = wifi_ok;
    s.cloud_ok = cloud_ok;
    s.msg_id   = 1;

    printf("\r\n=== Running ===\r\n");

    while (1) {
        OneNET_Process();
        Console_Task(&s);
        Sensor_Task(&s);
        Cloud_Task(&s);
        Reconnect_Task(&s);
        HAL_Delay(10);
    }
}

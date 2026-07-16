/**
  ******************************************************************************
  * @file    esp8266.h
  * @brief   ESP8266 WiFi 模块 AT 指令驱动头文件
  *
  *   USART2 (PA2=TX / PA3=RX) @115200 8N1
  *   RST = PC4（低电平复位）
  ******************************************************************************
  */

#ifndef __ESP8266_H__
#define __ESP8266_H__

#include "stm32f1xx_hal.h"
#include <stdint.h>

/* ============================== 配置 ===================================== */
#define ESP8266_RX_BUF_SIZE    1024
#define ESP8266_CMD_TIMEOUT    10000   /* AT 指令通用超时 ms */
#define ESP8266_CONN_TIMEOUT   15000   /* 连接 WiFi/TCP 超时 ms */
#define ESP8266_RESET_TIMEOUT  5000    /* 复位超时 ms */

/* ============================== 状态 ===================================== */
typedef enum {
    ESP8266_OK = 0,
    ESP8266_ERROR,
    ESP8266_TIMEOUT,
    ESP8266_BUSY,
} ESP8266_Status;

/* ============================== API ====================================== */

/* 初始化：绑定 UART、配置 RST 引脚，使能中断接收 */
void ESP8266_Init(UART_HandleTypeDef *huart);

/* 串口中断回调：ISR 中每收到一个字节就调用 */
void ESP8266_UART_IRQHandler(uint8_t byte);

/* 硬件复位：拉低 RST 100ms → 拉高 → 等待 "ready" */
ESP8266_Status ESP8266_Reset(void);

/* 连接 WiFi（AT+CWJAP） */
ESP8266_Status ESP8266_ConnectWiFi(const char *ssid, const char *password);

/* 建立 TCP 连接（AT+CIPSTART） */
ESP8266_Status ESP8266_ConnectTCP(const char *host, uint16_t port);

/* 发送 TCP 数据（AT+CIPSEND + 数据 + 等待 SEND OK） */
ESP8266_Status ESP8266_SendData(const uint8_t *data, uint16_t len);

/* 关闭 TCP 连接 */
ESP8266_Status ESP8266_CloseTCP(void);

/* 断开 WiFi */
ESP8266_Status ESP8266_DisconnectWiFi(void);

/* ---------- 数据接收 ---------- */

/* 主循环轮询：解析 AT 响应、提取 +IPD 数据 */
void ESP8266_Poll(void);

/* 返回 +IPD 缓冲区中可读数据长度 */
int ESP8266_DataAvailable(void);

/* 从 +IPD 缓冲区读取数据（非破坏性） */
int ESP8266_PeekData(uint8_t *buf, uint16_t max_len);

/* 从 +IPD 缓冲区消费（弹出）指定字节 */
void ESP8266_ConsumeData(uint16_t len);

#endif /* __ESP8266_H__ */

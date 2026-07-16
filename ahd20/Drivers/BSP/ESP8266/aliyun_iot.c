/**
  ******************************************************************************
  * @file    aliyun_iot.c
  * @brief   ⚠ 调试版（临时）—— 把数据通过 ESP8266 直推 TCP 到网络调试助手
  *
  *  调试助手在 PC 上当 TCP server (端口 8888)，ESP8266 当 TCP client 连过去，
  *  STM32 把 AHT20 读数 JSON 直接发过去，调试助手就能看到。
  *
  *  切换到真的阿里云时，把 Aliyun_Connect 改回 MQTT_Connect(aliyun broker)
  *  把 Aliyun_PropertyPost 改回 MQTT_Publish 就行，其它 API 不变。
  ******************************************************************************
  */
#include "aliyun_iot.h"
#include "esp8266.h"
#include <stdio.h>
#include <string.h>

/* =========================================================================
 * ★ 必须改成你 PC 的 IP 和端口
 *   - PC 端 IP: cmd -> ipconfig -> IPv4
 *   - 端口: 网络调试助手监听的端口（建议 8888）
 * ========================================================================= */
#define TCP_SERVER_IP   "10.117.237.22"
#define TCP_SERVER_PORT 8080

static Aliyun_DeviceInfo *dev_info = NULL;
static int tcp_ready = 0;

void Aliyun_Init(Aliyun_DeviceInfo *info)
{
    dev_info = info;
    tcp_ready = 0;
    (void)dev_info;  /* suppress -Wunused-but-set */
    printf("[Aliyun DEBUG] init pk=%s dn=%s\r\n",
           info->product_key, info->device_name);
    printf("[Aliyun DEBUG] -> TCP target %s:%d\r\n", TCP_SERVER_IP, TCP_SERVER_PORT);
}

int Aliyun_Connect(void)
{
    printf("[Aliyun DEBUG] TCP connect %s:%d ...\r\n", TCP_SERVER_IP, TCP_SERVER_PORT);
    if (ESP8266_ConnectTCP(TCP_SERVER_IP, TCP_SERVER_PORT) == ESP8266_OK) {
        tcp_ready = 1;
        printf("[Aliyun DEBUG] TCP connected!\r\n");
        return 0;
    }
    printf("[Aliyun DEBUG] TCP connect FAILED\r\n");
    return -1;
}

int Aliyun_PropertyPost(const char *json_props)
{
    if (!tcp_ready || !json_props) return -1;
    uint16_t len = (uint16_t)strlen(json_props);
    if (ESP8266_SendData((const uint8_t *)json_props, len) == ESP8266_OK) {
        return 0;
    }
    tcp_ready = 0;  /* 发送失败 → 标记断开 */
    return -1;
}

int Aliyun_EventPost(const char *event_id, const char *json_params)
{
    /* 调试模式：不支持 */
    (void)event_id; (void)json_params;
    return -1;
}

void Aliyun_Process(void)
{
    ESP8266_Poll();  /* 解析 +IPD 等内部数据（暂不取） */
}

int Aliyun_IsConnected(void)
{
    return tcp_ready;
}

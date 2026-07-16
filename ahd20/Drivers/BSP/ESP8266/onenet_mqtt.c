/**
  ******************************************************************************
  * @file    onenet_mqtt.c
  * @brief   OneNET Studio 客户端（走 TCP + STM32 自实现 MQTT 3.1.1）
  *
  *   走 ESP8266 AT+CIPSTART 建 TCP 到 mqtts.heclouds.com:1883
  *   不用 ESP8266 内置 MQTT（AT+MQTT* 对含 & 的 token 解析有 bug）
  *   STM32 自己拼 MQTT CONNECT / PUBLISH / PINGREQ 报文
  ******************************************************************************
  */
#include "onenet_mqtt.h"
#include "esp8266.h"
#include "mqtt_raw.h"
#include <stdio.h>
#include <string.h>

static const OneNET_Config *g_cfg = NULL;
static int g_connected = 0;

/* 心跳时间戳（ms） */
static uint32_t g_last_ping_ms = 0;
#define PING_INTERVAL_MS   30000   /* 30s（比 60s keep_alive 短，更稳） */

/* ============================ 主题构造 ==================================== */
/* 数据流模式：$sys/{pid}/{dev}/dp/post/json */
static int build_topic(char *out, uint16_t out_max, const char *suffix)
{
    if (!g_cfg || !g_cfg->product_id || !g_cfg->device_name) return -1;
    int n = snprintf(out, out_max, "$sys/%s/%s/%s",
                     g_cfg->product_id, g_cfg->device_name, suffix);
    return (n > 0 && n < out_max) ? 0 : -1;
}

/* 构造 OneJSON 数据流 payload */
static int build_payload(char *out, uint16_t out_max,
                          int msg_id,
                          const char *field1, double val1,
                          const char *field2, double val2)
{
    int n = snprintf(out, out_max,
        "{\"id\":%d,\"dp\":{\"%s\":[{\"v\":%.1f}],\"%s\":[{\"v\":%.1f}]}}",
        msg_id, field1, val1, field2, val2);
    return (n > 0 && n < out_max) ? n : -1;
}

/* ============================ MQTT 报文收发 ================================ */

static int send_mqtt_packet(const uint8_t *data, uint16_t len)
{
    ESP8266_Status s = ESP8266_SendData(data, len);
    if (s != ESP8266_OK) {
        printf("[ON] TCP send fail\r\n");
        g_connected = 0;
        return -1;
    }
    return 0;
}

/* 处理收到的 TCP 响应：找 MQTT 报文，识别类型 */
static void handle_incoming(void)
{
    while (ESP8266_DataAvailable() > 0) {
        uint8_t buf[256];
        int n = ESP8266_PeekData(buf, sizeof(buf));
        if (n < 2) {
            ESP8266_ConsumeData((uint16_t)n);
            continue;
        }

        /* DEBUG: 打印收到的原始数据前几个字节 */
        printf("[ON] RX %d bytes: ", n);
        for (int i = 0; i < n && i < 16; i++) printf("%02X ", buf[i]);
        printf("\r\n");

        int ptype = MQTT_ParsePacketType(buf, (uint16_t)n);
        if (ptype == (MQTT_PKT_CONNACK >> 4)) {
            /* CONNACK 响应 */
            int rc = MQTT_ParseConnAckReturnCode(buf, (uint16_t)n);
            printf("[ON] CONNACK: return_code=%d\r\n", rc);
            if (rc == 0) {
                g_connected = 1;
                g_last_ping_ms = HAL_GetTick();
            } else {
                printf("[ON] CONNACK rejected (rc=%d) - check product_id/device_name/token\r\n", rc);
                g_connected = 0;
            }
            /* CONNACK 4 字节 (type + len + 2) */
            ESP8266_ConsumeData(4);
        } else if (ptype == (MQTT_PKT_PINGRESP >> 4)) {
            /* PINGRESP 2 字节 */
            ESP8266_ConsumeData(2);
        } else if (ptype == (MQTT_PKT_PUBACK >> 4)) {
            /* PUBACK 4 字节（QoS 1 才用，我们目前 QoS 0 不会收到） */
            ESP8266_ConsumeData(4);
        } else if (ptype == (MQTT_PKT_PUBLISH >> 4)) {
            /* 云端下行 PUBLISH（暂不订阅，简单丢）*/
            /* 找 Remaining Length 和 payload，跳过整条 */
            if (n >= 4) {
                uint8_t rl = buf[1] & 0x7F;
                uint16_t total = 2 + rl;
                if (n >= total) {
                    printf("[ON] DL PUBLISH (%u bytes), ignored\r\n", total);
                    ESP8266_ConsumeData(total);
                } else {
                    /* 报文分片，保留等下次 */
                    break;
                }
            } else {
                break;
            }
        } else {
            /* 未知 / 不处理：丢掉 1 字节避免死循环 */
            ESP8266_ConsumeData(1);
        }
    }
}

/* ============================ 接口 ======================================== */

void OneNET_Init(const OneNET_Config *cfg)
{
    g_cfg = cfg;
    g_connected = 0;
    g_last_ping_ms = 0;
    if (cfg) {
        printf("[ON] Init: pid=%s dev=%s server=%s:%u\r\n",
               cfg->product_id, cfg->device_name,
               cfg->server, cfg->port);
    }
}

int OneNET_Connect(void)
{
    if (!g_cfg) {
        printf("[ON] No config\r\n");
        return -1;
    }

    /* 1) 建 TCP 到 mqtts.heclouds.com:1883 */
    ESP8266_Status s = ESP8266_ConnectTCP(g_cfg->server, g_cfg->port);
    if (s != ESP8266_OK) {
        printf("[ON] TCP connect failed\r\n");
        g_connected = 0;
        return -1;
    }

    /* 2) 拼 CONNECT 报文并发出去 */
    uint8_t pkt[512];
    int pkt_len = MQTT_BuildConnect(pkt, sizeof(pkt),
                                     g_cfg->device_name,    /* client_id */
                                     g_cfg->product_id,    /* user_name */
                                     g_cfg->token,         /* password  */
                                     60);                  /* keep_alive */
    if (pkt_len < 0) {
        printf("[ON] CONNECT build fail: %d\r\n", pkt_len);
        g_connected = 0;
        return -1;
    }
    printf("[ON] Send CONNECT (%d bytes) ...\r\n", pkt_len);
    /* DEBUG: hex dump 报文前 50 字节 */
    printf("[ON] HEX: ");
    for (int i = 0; i < pkt_len && i < 50; i++) printf("%02X ", pkt[i]);
    printf("\r\n");
    /* DEBUG: 把 token 完整打印出来确认字面值（不要截断！） */
    printf("[ON] token (len=%u):\r\n", (unsigned)strlen(g_cfg->token));
    printf("[ON] >>>%s<<<\r\n", g_cfg->token);

    if (send_mqtt_packet(pkt, (uint16_t)pkt_len) != 0) {
        return -1;
    }

    /* 发完 CONNECT 后立即检查 TCP 是否还被关闭 */
    ESP8266_Poll();
    if (ESP8266_DataAvailable() == 0) {
        printf("[ON] Waiting for CONNACK...\r\n");
    }

    /* 3) 等 CONNACK（5s 超时） */
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < 5000) {
        ESP8266_Poll();
        handle_incoming();
        if (g_connected) {
            printf("[ON] OneNET connected!\r\n");
            return 0;
        }
        HAL_Delay(20);
    }

    printf("[ON] CONNECT timeout (no CONNACK, %d bytes in buffer)\r\n",
           ESP8266_DataAvailable());
    g_connected = 0;
    return -1;
}

void OneNET_Disconnect(void)
{
    if (g_connected) {
        /* 发 DISCONNECT 报文 */
        uint8_t pkt[2] = { MQTT_PKT_DISCONNECT, 0x00 };
        send_mqtt_packet(pkt, 2);
    }
    ESP8266_CloseTCP();
    g_connected = 0;
}

int OneNET_PublishDataPoint(int msg_id,
                              const char *field1, double val1,
                              const char *field2, double val2)
{
    if (!g_connected || !g_cfg) return -1;

    static char topic[64];
    static char payload[128];
    static uint8_t pkt[384];

    if (build_topic(topic, sizeof(topic), "dp/post/json") != 0) return -1;

    int plen = build_payload(payload, sizeof(payload),
                              msg_id, field1, val1, field2, val2);
    if (plen < 0) return -1;

    int pkt_len = MQTT_BuildPublish(pkt, sizeof(pkt), topic,
                                     (const uint8_t *)payload, (uint16_t)plen);
    if (pkt_len < 0) return -1;

    if (send_mqtt_packet(pkt, (uint16_t)pkt_len) != 0) return -1;

    printf("[ON] pub id=%d: %s\r\n", msg_id, payload);
    return 0;
}

void OneNET_Process(void)
{
    ESP8266_Poll();

    /* 1) 处理收到的 TCP 数据（CONNACK / PINGRESP / 等） */
    handle_incoming();

    /* 2) 心跳 */
    if (g_connected && HAL_GetTick() - g_last_ping_ms >= PING_INTERVAL_MS) {
        uint8_t pkt[2];
        int n = MQTT_BuildPingReq(pkt, sizeof(pkt));
        if (n > 0) {
            send_mqtt_packet(pkt, (uint16_t)n);
            printf("[ON] PINGREQ sent\r\n");
            g_last_ping_ms = HAL_GetTick();
        }
    }
}

int OneNET_IsConnected(void)
{
    return g_connected;
}

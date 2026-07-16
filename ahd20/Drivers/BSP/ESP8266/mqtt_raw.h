/**
  ******************************************************************************
  * @file    mqtt_raw.h
  * @brief   MQTT 3.1.1 报文构造（CONNECT / PUBLISH / PINGREQ）+ 响应解析
  *
  *   我们不走 ESP8266 内置 MQTT 协议栈（AT+MQTT* 指令对含 & 的 token 解析有 bug），
  *   改为：ESP8266 建 TCP 连接 → STM32 自己拼 MQTT 报文发过去 → 自己解析响应。
  *   报文走 ESP8266 的 +IPD 机制（标准 AT+CIPSTART / AT+CIPSEND）。
  ******************************************************************************
  */
#ifndef __MQTT_RAW_H__
#define __MQTT_RAW_H__

#include <stdint.h>

/* MQTT 控制报文类型（固定报头第一个字节的高 4 位） */
#define MQTT_PKT_CONNECT     0x10
#define MQTT_PKT_CONNACK     0x20
#define MQTT_PKT_PUBLISH     0x30
#define MQTT_PKT_PUBACK      0x40
#define MQTT_PKT_PINGREQ     0xC0
#define MQTT_PKT_PINGRESP    0xD0
#define MQTT_PKT_DISCONNECT  0xE0

/* ============================ 报文构造 ==================================== */
/* 所有函数返回写入的字节数；< 0 = 错误 */

/* 构造 CONNECT 报文（Clean Session + UserName + Password）
 * out 至少 256 字节
 * keep_alive 单位 s */
int MQTT_BuildConnect(uint8_t *out, uint16_t out_max,
                       const char *client_id,
                       const char *user_name,
                       const char *password,
                       uint16_t keep_alive);

/* 构造 PUBLISH 报文（QoS 0，不带 Packet ID）
 * out 至少 256 字节
 * topic: 主题字符串
 * payload / payload_len: 应用数据（JSON） */
int MQTT_BuildPublish(uint8_t *out, uint16_t out_max,
                       const char *topic,
                       const uint8_t *payload, uint16_t payload_len);

/* 构造 PINGREQ 报文（心跳）
 * out 至少 2 字节，返回 2 */
int MQTT_BuildPingReq(uint8_t *out, uint16_t out_max);

/* ============================ 响应解析 ==================================== */
/* 解析收到的 MQTT 报文（来自 ESP8266 +IPD 数据）
 * 返回：MQTT_PKT_xxx 报文类型，< 0 = 解析失败
 *
 * 简化：只识别固定报头第一个字节，足够判断 CONNACK / PUBACK / PINGRESP */

int MQTT_ParsePacketType(const uint8_t *data, uint16_t len);

/* 解析 CONNACK 的 return code（0 = Connection Accepted）
 * data 至少 4 字节（type + len + 2 bytes session/return code）
 * 返回 0=OK, 1-5=OneNET 鉴权/网络错误码, < 0=格式错 */
int MQTT_ParseConnAckReturnCode(const uint8_t *data, uint16_t len);

#endif /* __MQTT_RAW_H__ */

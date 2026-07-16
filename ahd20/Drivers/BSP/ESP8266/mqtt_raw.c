/**
  ******************************************************************************
  * @file    mqtt_raw.c
  * @brief   MQTT 3.1.1 报文构造 + 响应解析实现
  ******************************************************************************
  */
#include "mqtt_raw.h"
#include <string.h>

/* ============================ 变长编码 ==================================== */
/* MQTT Remaining Length 编码：1-4 字节
 * 每个字节低 7 位存数据，最高位为 continuation bit
 * 最大 4 字节可表示 268,435,455 (~256MB)，足够任何 IoT 场景 */
static int encode_remaining_length(uint8_t *out, uint32_t length)
{
    int n = 0;
    do {
        uint8_t b = (uint8_t)(length & 0x7F);
        length >>= 7;
        if (length > 0) b |= 0x80;
        out[n++] = b;
    } while (length > 0 && n < 4);
    return (length == 0) ? n : -1;
}

/* 写入 2 字节大端长度前缀 + 字符串（MQTT 字符串字段格式） */
static int write_string(uint8_t *out, uint16_t out_max, uint16_t offset,
                         const char *str)
{
    if (!str) return -1;
    uint16_t slen = (uint16_t)strlen(str);
    if (offset + 2 + slen > out_max) return -1;
    out[offset++] = (uint8_t)(slen >> 8);
    out[offset++] = (uint8_t)(slen & 0xFF);
    memcpy(out + offset, str, slen);
    return offset + (int)slen;
}

/* ============================ 报文构造 ==================================== */

int MQTT_BuildConnect(uint8_t *out, uint16_t out_max,
                       const char *client_id,
                       const char *user_name,
                       const char *password,
                       uint16_t keep_alive)
{
    if (!out || out_max < 256 || !client_id || !user_name || !password) return -1;

    /* 1) 拼变长报头 + Payload 到临时 buffer（避开 fixed header + remaining length 位置） */
    uint8_t body[512];
    uint16_t bp = 0;

    /* Protocol Name: "MQTT" */
    if ((bp = (uint16_t)write_string(body, sizeof(body), bp, "MQTT")) < 0) return -2;
    /* Protocol Level: 4 (MQTT 3.1.1) — MQTTX 通的版本 */
    body[bp++] = 0x04;
    /* Connect Flags: 0xC2 = User+Password+CleanSession */
    body[bp++] = 0xC2;
    /* Keep Alive: 2 bytes big-endian */
    body[bp++] = (uint8_t)(keep_alive >> 8);
    body[bp++] = (uint8_t)(keep_alive & 0xFF);
    /* MQTT 5.0 Properties 字段 - 已不用 3.1.1 协议 */
    /* Client ID */
    if ((bp = (uint16_t)write_string(body, sizeof(body), bp, client_id)) < 0) return -3;
    /* User Name */
    if ((bp = (uint16_t)write_string(body, sizeof(body), bp, user_name)) < 0) return -4;
    /* Password */
    if ((bp = (uint16_t)write_string(body, sizeof(body), bp, password)) < 0) return -5;

    uint32_t body_len = bp;

    /* 2) 写 Fixed Header：0x10 (CONNECT) */
    if (out_max < 1 + 4 + body_len) return -6;
    out[0] = MQTT_PKT_CONNECT;

    /* 3) 写 Remaining Length（变长编码，1-4 字节） */
    int rl_n = encode_remaining_length(out + 1, body_len);
    if (rl_n < 0) return -7;

    /* 4) 紧跟变长报头 + Payload（无 gap 字节） */
    memcpy(out + 1 + rl_n, body, body_len);

    return 1 + rl_n + (int)body_len;
}

int MQTT_BuildPublish(uint8_t *out, uint16_t out_max,
                       const char *topic,
                       const uint8_t *payload, uint16_t payload_len)
{
    if (!out || out_max < 256 || !topic) return -1;
    if (payload_len > 0 && !payload) return -1;

    uint16_t topic_strlen = (uint16_t)strlen(topic);
    if (topic_strlen + 2 + payload_len + 5 > out_max) return -2;

    uint16_t o = 0;
    /* 固定报头：0x30 (PUBLISH, QoS 0, no retain) */
    out[o++] = MQTT_PKT_PUBLISH;

    /* Remaining Length = 2 + topic_strlen + payload_len */
    uint32_t rem_len = (uint32_t)(2 + topic_strlen + payload_len);
    int rl_n = encode_remaining_length(out + o, rem_len);
    if (rl_n < 0) return -3;
    o += rl_n;

    /* 变长报头：Topic Name */
    out[o++] = (uint8_t)(topic_strlen >> 8);
    out[o++] = (uint8_t)(topic_strlen & 0xFF);
    memcpy(out + o, topic, topic_strlen);
    o += topic_strlen;

    /* Payload（QoS 0 无 Packet ID） */
    if (payload_len > 0) {
        memcpy(out + o, payload, payload_len);
        o += payload_len;
    }

    return o;
}

int MQTT_BuildPingReq(uint8_t *out, uint16_t out_max)
{
    if (!out || out_max < 2) return -1;
    out[0] = MQTT_PKT_PINGREQ;
    out[1] = 0x00;  /* Remaining Length = 0 */
    return 2;
}

/* ============================ 响应解析 ==================================== */

int MQTT_ParsePacketType(const uint8_t *data, uint16_t len)
{
    if (!data || len < 2) return -1;
    return (data[0] >> 4) & 0x0F;  /* 高 4 位是 packet type */
}

int MQTT_ParseConnAckReturnCode(const uint8_t *data, uint16_t len)
{
    if (!data || len < 4) return -1;
    /* 字节 0: 0x20 (CONNACK)
     * 字节 1: Remaining Length (正常 2)
     * 字节 2: Connect Acknowledge Flags（一般 0）
     * 字节 3: Return Code (0=Accepted, 1-5=各种错误) */
    if ((data[0] & 0xF0) != MQTT_PKT_CONNACK) return -2;
    return (int)data[3];
}

/**
  ******************************************************************************
  * @file    onenet_mqtt.h
  * @brief   OneNET Studio（新版）MQTT 客户端
  *
  *   用 ESP8266 内置 MQTT 协议栈（AT+MQTTxxx）连接 OneNET 物联网开放平台
  *   服务器：mqtts.heclouds.com:1883 (TCP)
  *
  *   上行（设备 → 云）：
  *     主题：$sys/{产品ID}/{设备名}/dp/post/json
  *     Payload：{"id":123,"dp":{"temperature":[{"v":25.5}],"humidity":[{"v":60.0}]}}
  *
  *   鉴权（产品级）：
  *     username = 产品ID
  *     password = token（version=2018-10-31&res=products%2F{产品ID}&et=...&method=md5&sign=...）
  *     client_id = 设备名
  *
  *   注：此版本暂只支持上行（属性上报）。下行（远程控灯）框架已留好
  *       （esp8266.c 已解析 +MQTTSUBRECV），但本文件未订阅主题。
  ******************************************************************************
  */
#ifndef __ONENET_MQTT_H__
#define __ONENET_MQTT_H__

#include <stdint.h>

/* 用户配置：在 main.c 顶部修改这里 */
typedef struct {
    const char *product_id;     /* 产品ID，如 "7Sp4dA99m3" */
    const char *device_name;    /* 设备名，如 "dev1" */
    const char *token;          /* 算好的 password（含 version=2018-10-31...） */
    const char *server;         /* 一般 "mqtts.heclouds.com" */
    uint16_t    port;           /* 一般 1883 */
} OneNET_Config;

/* 初始化（只保存配置，不连） */
void OneNET_Init(const OneNET_Config *cfg);

/* 连接 MQTT（先确保 WiFi 已连），返回 0=OK / -1=失败 */
int  OneNET_Connect(void);

/* 主动断开 */
void OneNET_Disconnect(void);

/* 发布 OneJSON 上行（payload 用 snprintf 构造，函数内不解析 JSON） */
/* 上行 JSON 模板：{"id":%d,"dp":{"%s":[{"v":%.1f}],"%s":[{"v":%.1f}]}} */
int  OneNET_PublishDataPoint(int msg_id,
                              const char *field1, double val1,
                              const char *field2, double val2);

/* 主循环里调用，处理下行（订阅主题的回包） */
void OneNET_Process(void);

/* 查询连接状态（粗略：上次 Connect/Disconnect 操作结果） */
int  OneNET_IsConnected(void);

#endif /* __ONENET_MQTT_H__ */

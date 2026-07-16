/**
  ******************************************************************************
  * @file    aliyun_iot.h
  * @brief   阿里云物联网平台物模型适配层
  *
  *   功能：
  *     - 一机一密认证 (HMAC-SHA1)
  *     - 属性上报 (PropertyPost)
  *     - 属性设置响应 (PropertySet)
  *     - 事件上报
  *
  *   基于 MQTT 3.1.1 协议，通过 ${productKey}.iot-as-mqtt.cn-shanghai.aliyuncs.com:1883
  ******************************************************************************
  */

#ifndef __ALIYUN_IOT_H__
#define __ALIYUN_IOT_H__

#include <stdint.h>

typedef struct {
    const char *product_key;
    const char *device_name;
    const char *device_secret;
} Aliyun_DeviceInfo;

/* 初始化三元组 */
void Aliyun_Init(Aliyun_DeviceInfo *info);

/* 连接到阿里云 IoT（内部执行 MQTT CONNECT + 订阅属性设置 Topic） */
int Aliyun_Connect(void);

/* 上报属性（JSON 负载，如温度/湿度） */
int Aliyun_PropertyPost(const char *json_props);

/* 事件上报 */
int Aliyun_EventPost(const char *event_id, const char *json_params);

/* 主循环调用 */
void Aliyun_Process(void);

/* 获取 MQTT 连接状态 */
int Aliyun_IsConnected(void);

#endif /* __ALIYUN_IOT_H__ */

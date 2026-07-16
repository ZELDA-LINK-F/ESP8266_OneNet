/**
  ******************************************************************************
  * @file    aht20.h
  * @brief   AHT20 温湿度传感器驱动（HAL 库 I2C 接口）
  *
  *   AHT20 是奥松电子（Aosong）出品的 I2C 数字温湿度传感器，替代 DHT22/SHT20 等。
  *   关键参数：
  *     - I2C 7-bit 地址：0x38
  *     - 测量范围：温度 -40~+85 ℃（±0.3℃），湿度 0~100 %RH（±2%RH）
  *     - 上电等待 40ms 后才能发命令
  *     - 触发测量后等待 ≥75ms 读结果
  *
  *   接线：VDD(3.3V) / GND / SCL / SDA，SDA/SCL 必须外接 4.7K 上拉
  ******************************************************************************
  */

#ifndef __AHT20_H
#define __AHT20_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include <stdint.h>

/* AHT20 固定 I2C 地址（7-bit） */
#define AHT20_I2C_ADDR            (0x38U)

/* AHT20 寄存器 / 命令 */
#define AHT20_CMD_SOFT_RESET      (0xBAU)   /* 软复位 */
#define AHT20_CMD_CALIBRATE        (0xBEU)   /* 初始化校准，需带 0x08 0x00 两字节 */
#define AHT20_CMD_TRIGGER_MEAS    (0xACU)   /* 触发测量，需带 0x33 0x00 两字节 */
#define AHT20_CALIB_ARG_1          (0x08U)
#define AHT20_CALIB_ARG_2          (0x00U)
#define AHT20_TRIG_ARG_1           (0x33U)
#define AHT20_TRIG_ARG_2           (0x00U)

/* 状态寄存器 bit */
#define AHT20_STATUS_BUSY          (0x80U)   /* 1 = 测量中 */
#define AHT20_STATUS_CALIBRATED    (0x08U)   /* 1 = 已校准 */

/**
  * @brief  初始化 AHT20：上电等待、软复位、触发校准
  * @note   必须在 MX_I2C1_Init() 之后调用
  * @retval HAL_OK / HAL_ERROR
  */
HAL_StatusTypeDef AHT20_Init(void);

/**
  * @brief  触发一次测量并读取温湿度
  * @param  [out] temperature  温度值（℃）
  * @param  [out] humidity     湿度值（%RH）
  * @retval HAL_OK / HAL_ERROR / HAL_TIMEOUT
  */
HAL_StatusTypeDef AHT20_Read(float *temperature, float *humidity);

/**
  * @brief  软复位 AHT20（200us 内完成）
  * @retval HAL_OK / HAL_ERROR
  */
HAL_StatusTypeDef AHT20_SoftReset(void);

#ifdef __cplusplus
}
#endif

#endif /* __AHT20_H */

/**
  ******************************************************************************
  * @file    aht20.c
  * @brief   AHT20 温湿度传感器驱动实现（HAL 库 I2C 接口）
  *
  *   通信时序：
  *     1) 上电后等待 ≥40ms
  *     2) 读 status（0x71），bit[3]=0 则发 0xBE 0x08 0x00 触发校准
  *        （首次上电 / 复位后必须）
  *     3) 发送 0xAC 0x33 0x00 触发测量
  *     4) 轮询 status，busy=0 后读 7 字节：status + hum[19:0] + temp[19:0] + CRC
  *     5) 转换：
  *        RH[%]   = (hum_raw  / 2^20) * 100
  *        T[°C]   = (temp_raw / 2^20) * 200 - 50
  ******************************************************************************
  */

#include "aht20.h"

extern I2C_HandleTypeDef hi2c1;   /* 在 main.c 里定义 */

/* ========================== 私有辅助函数 ================================ */

/**
  * @brief 读 AHT20 状态寄存器
  * @retval 状态字节（错误时返回 0xFF）
  */
static uint8_t AHT20_ReadStatus(void)
{
  uint8_t status = 0xFFU;
  if (HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(AHT20_I2C_ADDR << 1),
                              &status, 1U, 100U) != HAL_OK)
  {
    return 0xFFU;
  }
  return status;
}

/**
  * @brief 等待 AHT20 内部 busy 清除（测量完成）
  * @param timeout_ms 最长等待时间
  * @retval HAL_OK / HAL_TIMEOUT / HAL_ERROR
  */
static HAL_StatusTypeDef AHT20_WaitForReady(uint32_t timeout_ms)
{
  uint8_t status = 0U;
  uint32_t tick = HAL_GetTick();
  while ((HAL_GetTick() - tick) < timeout_ms)
  {
    status = AHT20_ReadStatus();
    if (status == 0xFFU)
    {
      return HAL_ERROR;
    }
    if ((status & AHT20_STATUS_BUSY) == 0U)
    {
      return HAL_OK;
    }
    HAL_Delay(5);
  }
  return HAL_TIMEOUT;
}

/* ========================== 对外接口实现 ================================== */

/**
  * @brief  软复位
  */
HAL_StatusTypeDef AHT20_SoftReset(void)
{
  uint8_t cmd = AHT20_CMD_SOFT_RESET;
  if (HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(AHT20_I2C_ADDR << 1),
                               &cmd, 1U, 100U) != HAL_OK)
  {
    return HAL_ERROR;
  }
  HAL_Delay(20);   /* 软复位后需等待 20ms 才能下次访问 */
  return HAL_OK;
}

/**
  * @brief  初始化：上电等待 → 软复位 → 触发校准
  */
HAL_StatusTypeDef AHT20_Init(void)
{
  HAL_StatusTypeDef rc;
  uint8_t status;
  uint8_t cmd[3];

  /* 1. 上电后必须等待 ≥40ms（手册） */
  HAL_Delay(50);

  /* 2. 软复位（保守起见每次都做） */
  rc = AHT20_SoftReset();
  if (rc != HAL_OK) { return rc; }

  /* 3. 检查校准状态，未校准则触发校准命令 0xBE 0x08 0x00 */
  status = AHT20_ReadStatus();
  if (status == 0xFFU) { return HAL_ERROR; }

  if ((status & AHT20_STATUS_CALIBRATED) == 0U)
  {
    cmd[0] = AHT20_CMD_CALIBRATE;
    cmd[1] = AHT20_CALIB_ARG_1;
    cmd[2] = AHT20_CALIB_ARG_2;
    rc = HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(AHT20_I2C_ADDR << 1),
                                  cmd, 3U, 100U);
    if (rc != HAL_OK) { return rc; }
    HAL_Delay(10);
  }

  return HAL_OK;
}

/**
  * @brief  触发测量 + 读 7 字节 + 解析
  */
HAL_StatusTypeDef AHT20_Read(float *temperature, float *humidity)
{
  HAL_StatusTypeDef rc;
  uint8_t cmd[3];
  uint8_t buf[7] = {0};

  if (temperature == NULL || humidity == NULL)
  {
    return HAL_ERROR;
  }

  /* 1. 触发测量：0xAC 0x33 0x00 */
  cmd[0] = AHT20_CMD_TRIGGER_MEAS;
  cmd[1] = AHT20_TRIG_ARG_1;
  cmd[2] = AHT20_TRIG_ARG_2;
  rc = HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(AHT20_I2C_ADDR << 1),
                                cmd, 3U, 100U);
  if (rc != HAL_OK) { return rc; }

  /* 2. 等待测量完成（手册：典型 75ms，给 100ms 余量） */
  rc = AHT20_WaitForReady(100U);
  if (rc != HAL_OK) { return rc; }

  /* 3. 读取 7 字节：status + hum[19:0] + temp[19:0] + CRC */
  rc = HAL_I2C_Master_Receive(&hi2c1, (uint16_t)(AHT20_I2C_ADDR << 1),
                               buf, 7U, 100U);
  if (rc != HAL_OK) { return rc; }

  /* 4. 校验：bit[7] busy 应为 0，否则数据无效 */
  if (buf[0] & AHT20_STATUS_BUSY)
  {
    return HAL_ERROR;
  }

  /* 5. 解析原始值（20-bit 大端） */
  uint32_t hum_raw  = ((uint32_t)buf[1] << 12) |
                      ((uint32_t)buf[2] <<  4) |
                      ((uint32_t)buf[3] >>  4);
  uint32_t temp_raw = ((uint32_t)buf[3] & 0x0FU) << 16 |
                      ((uint32_t)buf[4] <<  8) |
                      ((uint32_t)buf[5]);

  /* 6. 转换为物理量（公式见 AHT20 datasheet） */
  *humidity    = ((float)hum_raw  / 1048576.0f) * 100.0f;
  *temperature = ((float)temp_raw / 1048576.0f) * 200.0f - 50.0f;

  return HAL_OK;
}

#include "aht20_soft.h"
#include "stm32f1xx_hal.h"
#include "pins.h"
#include <stdio.h>

/* ============================ 软件 I2C (PB6=SCL, PB7=SDA) ================ */
#define SI2C_SCL_PIN 6
#define SI2C_SDA_PIN 7

static void si2c_delay(void)
{
  volatile uint32_t i = 30;
  while (i--) __NOP();
}

static void si2c_init(void)
{
  GPIOB->CRL &= ~(0xFU << (SI2C_SCL_PIN * 4));
  GPIOB->CRL |=  (0x3U << (SI2C_SCL_PIN * 4));
  GPIOB->BSRR = GPIO_PIN_6;

  GPIOB->CRL &= ~(0xFU << (SI2C_SDA_PIN * 4));
  GPIOB->CRL |=  (0x8U << (SI2C_SDA_PIN * 4));
  GPIOB->BSRR = GPIO_PIN_7;
}

#define SCL_H()  (GPIOB->BSRR = GPIO_PIN_6)
#define SCL_L()  (GPIOB->BRR  = GPIO_PIN_6)

static void sda_out0(void)
{
  GPIOB->CRL &= ~(0xFU << (SI2C_SDA_PIN * 4));
  GPIOB->CRL |=  (0x7U << (SI2C_SDA_PIN * 4));
  GPIOB->BRR = GPIO_PIN_7;
}

static void sda_in_pu(void)
{
  GPIOB->CRL &= ~(0xFU << (SI2C_SDA_PIN * 4));
  GPIOB->CRL |=  (0x8U << (SI2C_SDA_PIN * 4));
  GPIOB->BSRR = GPIO_PIN_7;
}

static void sda_restore_for_lcd(void)
{
  GPIOB->CRL &= ~(0xFU << (SI2C_SDA_PIN * 4));
  GPIOB->CRL |=  (0x1U << (SI2C_SDA_PIN * 4));
  GPIOB->BSRR = GPIO_PIN_7;
}

#define SDA_RD()  ((GPIOB->IDR & GPIO_PIN_7) ? 1 : 0)

static void si2c_start(void)
{
  sda_in_pu(); si2c_delay();
  SCL_H();     si2c_delay();
  sda_out0();  si2c_delay();
  SCL_L();     si2c_delay();
}

static void si2c_stop(void)
{
  sda_out0();  si2c_delay();
  SCL_H();     si2c_delay();
  sda_in_pu(); si2c_delay();
}

static uint8_t si2c_wait_ack(void)
{
  uint8_t ack;
  sda_in_pu(); si2c_delay();
  SCL_H();     si2c_delay();
  ack = SDA_RD() ? 0 : 1;
  SCL_L();     si2c_delay();
  return ack;
}

static void si2c_write_byte(uint8_t data)
{
  for (int i = 0; i < 8; i++)
  {
    if (data & 0x80) sda_in_pu(); else sda_out0();
    data <<= 1;
    si2c_delay();
    SCL_H(); si2c_delay();
    SCL_L(); si2c_delay();
  }
  si2c_wait_ack();
}

static uint8_t si2c_read_byte(uint8_t send_ack)
{
  uint8_t data = 0;
  sda_in_pu();
  for (int i = 0; i < 8; i++)
  {
    data <<= 1;
    SCL_H(); si2c_delay();
    if (SDA_RD()) data |= 1;
    SCL_L(); si2c_delay();
  }
  if (send_ack) sda_out0(); else sda_in_pu();
  si2c_delay();
  SCL_H(); si2c_delay();
  SCL_L(); si2c_delay();
  sda_in_pu();
  return data;
}

/* ============================ AHT20 通过软 I2C =========================== */
#define AHT20_ADDR         0x38
#define AHT20_CMD_INIT     0xBE
#define AHT20_CMD_MEASURE  0xAC
#define AHT20_CMD_RESET    0xBA
#define AHT20_STATUS_BUSY  0x80
#define AHT20_STATUS_CAL   0x08

static uint8_t AHT20_CRC8(uint8_t *data, uint8_t len)
{
  uint8_t crc = 0xFF;
  for (int b = 0; b < len; b++)
  {
    crc ^= data[b];
    for (int i = 0; i < 8; i++)
    {
      if (crc & 0x80) crc = (crc << 1) ^ 0x31;
      else            crc <<= 1;
    }
  }
  return crc;
}

uint8_t AHT20_SoftInit(void)
{
  si2c_init();

  si2c_start();
  si2c_write_byte(AHT20_ADDR << 1);
  si2c_write_byte(AHT20_CMD_RESET);
  si2c_stop();
  HAL_Delay(20);
  HAL_Delay(40);

  uint8_t s;
  si2c_start();
  si2c_write_byte((AHT20_ADDR << 1) | 1);
  s = si2c_read_byte(0);
  si2c_stop();

  printf("AHT20 INIT: S=%02X\r\n", s);

  if (!(s & AHT20_STATUS_CAL))
  {
    si2c_start();
    si2c_write_byte(AHT20_ADDR << 1);
    si2c_write_byte(AHT20_CMD_INIT);
    si2c_write_byte(0x08);
    si2c_write_byte(0x00);
    si2c_stop();
    HAL_Delay(100);
    si2c_start();
    si2c_write_byte((AHT20_ADDR << 1) | 1);
    s = si2c_read_byte(0);
    si2c_stop();
    printf("AHT20 CAL: S=%02X\r\n", s);
  }

  sda_restore_for_lcd();
  return 0;
}

uint8_t AHT20_SoftRead(float *t, float *h)
{
  uint8_t buf[7];

  si2c_start();
  si2c_write_byte(AHT20_ADDR << 1);
  si2c_write_byte(AHT20_CMD_MEASURE);
  si2c_write_byte(0x33);
  si2c_write_byte(0x00);
  si2c_stop();
  HAL_Delay(80);

  uint8_t busy_ok = 0;
  for (uint32_t r = 0; r < 200; r++)
  {
    uint8_t s;
    si2c_start();
    si2c_write_byte((AHT20_ADDR << 1) | 1);
    s = si2c_read_byte(0);
    si2c_stop();
    if (!(s & AHT20_STATUS_BUSY)) { busy_ok = 1; break; }
    HAL_Delay(1);
  }

  if (!busy_ok)
  {
    printf("[AHT20] BUSY timeout!\r\n");
    sda_restore_for_lcd();
    return 1;
  }

  si2c_start();
  si2c_write_byte((AHT20_ADDR << 1) | 1);
  buf[0] = si2c_read_byte(1);
  buf[1] = si2c_read_byte(1);
  buf[2] = si2c_read_byte(1);
  buf[3] = si2c_read_byte(1);
  buf[4] = si2c_read_byte(1);
  buf[5] = si2c_read_byte(1);
  buf[6] = si2c_read_byte(0);
  si2c_stop();

  uint32_t rh = ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | ((uint32_t)buf[3] >> 4);
  uint32_t rt = ((uint32_t)(buf[3] & 0x0F) << 16) | ((uint32_t)buf[4] << 8) | ((uint32_t)buf[5]);

  if (AHT20_CRC8(buf, 6) != buf[6])
  {
    printf("[AHT20] CRC error!\r\n");
    LED_RED_TOGGLE();
    sda_restore_for_lcd();
    return 1;
  }

  *h = (float)rh * 100.0f / 1048576.0f;
  *t = (float)rt * 200.0f / 1048576.0f - 50.0f;

  sda_restore_for_lcd();
  return 0;
}

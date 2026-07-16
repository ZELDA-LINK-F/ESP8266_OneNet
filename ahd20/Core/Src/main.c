/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : AHT20 温湿度 + ILI9341 LCD + ESP8266 WiFi + OneNET MQTT
  *
  *  ╔══════════════════════════════════════════════════════════════╗
  *  ║  板子：苏宙嵌孝教育 STM32-F103RCT6 V5.4.1                     ║
  *  ║  MCU ：STM32F103RCT6 (256K Flash / 48K RAM / LQFP64)          ║
  *  ║  时钟：HSI 8MHz（无外部晶振）                                ║
  *  ║  调试：板载 CMSIS-DAP（PA13/PA14）                           ║
  *  ║  串口：USART1 (PA9/PA10) → 板载 CH340 → COM13 / 115200       ║
  *  ║  WiFi：ESP8266 接 USART3 (PC10=TX / PC11=RX) + RST=PC4       ║
  *  ║  云  ：OneNET Studio MQTT (mqtts.heclouds.com:1883)           ║
  *  ╚══════════════════════════════════════════════════════════════╝
  *
  *  引脚定义    ：Inc/pins.h  ← 改引脚只改这里
  *  引脚对照表  ：board_pinout.md
  *  详细避坑    ：README.md / DEBUG_SUMMARY.md
  *
  *  ⚠ 不要动 #1：msp.c 里是 __HAL_AFIO_REMAP_SWJ_NOJTAG()
  *                改回 DISABLE 会永久失联 DAP-Link（PA13/PA14 变 GPIO）
  *
  *  ⚠ 不要动 #2：Keil → Options → Target → 勾选 Use MicroLIB
  *                否则启动时 newlib 走 semihosting 触发 BKPT 0xAB，main() 跑不到
  *
  *  ⚠ 不要动 #3：Keil Debug Settings → Connect=under Reset + Reset=Hardware
  *                DAP-Link 失联时用这个配置恢复
  *
  *  ⚠ 板子丝印陷阱：PC10/PC11 丝印标 "UART4_TX/RX"，RCT6 没有 UART4/5，丝印是装饰
  *                   实际串口用 USART1 (PA9/PA10)
  ******************************************************************************
  */
/* USER CODE END Header */
#include "main.h"
#include "stm32f1xx_hal.h"
#include "pins.h"             /* 引脚定义集中管理 */
#include "bsp_LCD_ILI9341.h"
#include "esp8266.h"
#include "onenet_mqtt.h"
#include "mqtt_raw.h"
#include <stdio.h>
#include <string.h>

/* ============================ 用户配置（改这里！）========================== */

/* --- WiFi --- */
#define WIFI_SSID       "Xiaomi 15"
#define WIFI_PASSWORD   "88888888"

/* --- OneNET 配置 --- */
/* 产品: 7Sp4dA99m3, 设备: dev1
 * broker: mqtts.heclouds.com:1883 (公共 MQTT broker, 非 TLS)
 * 鉴权: 产品级 token (SHA1, 2030年过期)
 */
#define ONENET_SERVER_HOST  "mqtts.heclouds.com"
#define ONENET_SERVER_PORT  1883
#define ONENET_PRODUCT_ID   "7Sp4dA99m3"
#define ONENET_DEVICE_NAME  "dev1"
#define ONENET_FIELD_TEMP   "SHT30_T"
#define ONENET_FIELD_HUMI   "SHT30_H"

/* 产品级 token (SHA1, et=2030-01-01) */
#define ONENET_TOKEN \
  "version=2018-10-31&res=products%2F7Sp4dA99m3" \
  "&et=1893456000&method=sha1" \
  "&sign=8PtQO6%2FQeF7vSAOc2OQPUL1DJZg%3D"

/* ============================ 私有声明 ==================================== */
void Error_Handler(void);

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* ============================ printf 重定向 ============================== */
int fputc(int ch, FILE *f)
{
  (void)f;
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1U, 0xFFFFU);
  return ch;
}

/* ============================ SystemClock_Config ========================== */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) { Error_Handler(); }

  RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK |
                                     RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK) { Error_Handler(); }
}

/* ============================ MX_GPIO_Init =============================== */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /* PB2 蓝灯 */
  HAL_GPIO_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, GPIO_PIN_SET);
  GPIO_InitStruct.Pin   = LED_BLUE_PIN;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_BLUE_PORT, &GPIO_InitStruct);

  /* PC5 红灯 */
  HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
  GPIO_InitStruct.Pin   = LED_RED_PIN;
  HAL_GPIO_Init(LED_RED_PORT, &GPIO_InitStruct);
}

/* ============================ MX_USART1_UART_Init ======================= */
static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK) { Error_Handler(); }
}

/* ============================ MX_USART3_UART_Init ======================= */
/* ESP8266 通信口：PC10=TX, PC11=RX, 115200 8N1, RXNE 中断 */
static void MX_USART3_UART_Init(void)
{
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK) { Error_Handler(); }

  /* 使能 USART3 接收中断 + NVIC */
  __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
  HAL_NVIC_SetPriority(USART3_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(USART3_IRQn);
}

/* ============================ 软件 I2C (PB6=SCL, PB7=SDA) ================ */
/*
 * PB6/PB7 与 LCD 数据总线 D6/D7 共用 GPIOB，因此：
 *  - PB6 (SCL) 始终配置为推挽输出（与 LCD 兼容）
 *  - PB7 (SDA) 在 I2C 操作时动态切换输入/输出，结束后恢复推挽输出
 *  - I2C 空闲时 PB6/PB7 均为推挽输出高电平，不影响 LCD 写操作
 */
#define SI2C_SCL_PIN 6
#define SI2C_SDA_PIN 7

static void si2c_delay(void)
{
  volatile uint32_t i = 30;
  while (i--) __NOP();
}

static void si2c_init(void)
{
  /* PB6(SCL): 推挽输出 */
  GPIOB->CRL &= ~(0xFU << (SI2C_SCL_PIN * 4));
  GPIOB->CRL |=  (0x3U << (SI2C_SCL_PIN * 4));
  GPIOB->BSRR = GPIO_PIN_6;

  /* PB7(SDA): 输入上拉（空闲高） */
  GPIOB->CRL &= ~(0xFU << (SI2C_SDA_PIN * 4));
  GPIOB->CRL |=  (0x8U << (SI2C_SDA_PIN * 4));
  GPIOB->BSRR = GPIO_PIN_7;
}

#define SCL_H()  (GPIOB->BSRR = GPIO_PIN_6)
#define SCL_L()  (GPIOB->BRR  = GPIO_PIN_6)

static void sda_out0(void)
{
  GPIOB->CRL &= ~(0xFU << (SI2C_SDA_PIN * 4));
  GPIOB->CRL |=  (0x7U << (SI2C_SDA_PIN * 4));  /* 开漏输出 */
  GPIOB->BRR = GPIO_PIN_7;  /* 驱低 */
}

static void sda_in_pu(void)
{
  GPIOB->CRL &= ~(0xFU << (SI2C_SDA_PIN * 4));
  GPIOB->CRL |=  (0x8U << (SI2C_SDA_PIN * 4));  /* 输入上拉 */
  GPIOB->BSRR = GPIO_PIN_7;  /* ODR=1 → 上拉 */
}

static void sda_restore_for_lcd(void)
{
  GPIOB->CRL &= ~(0xFU << (SI2C_SDA_PIN * 4));
  GPIOB->CRL |=  (0x1U << (SI2C_SDA_PIN * 4));  /* 推挽输出 10MHz */
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

/* ============================ main ======================================= */
int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();

  /* LCD 初始化（需先于 AHT20，因为共用 PB6/PB7） */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  LCD_Init();
  LCD_Fill(0, 0, 239, 319, BLACK);
  LCD_String(10, 5, "IoT AHT20+ESP8266", 16, WHITE, BLACK);
  LCD_Fill(0, 30, 239, 59, BLUE);
  LCD_String(10, 35, "Temperature:", 16, WHITE, BLUE);
  LCD_Fill(0, 150, 239, 179, GREEN);
  LCD_String(10, 155, "Humidity:", 16, WHITE, GREEN);
  /* WiFi 状态行 */
  LCD_Fill(0, 130, 239, 148, BROWN);
  LCD_String(10, 132, "WiFi:", 12, WHITE, BROWN);
  LCD_Fill(0, 250, 239, 268, BROWN);
  LCD_String(10, 252, "Cloud:", 12, WHITE, BROWN);

  printf("\r\n===== ahd20 IoT =====\r\n");
  printf("  '1'=Blue  '2'=Red  '3'=Both  '0'=Off\r\n");
  printf("======================\r\n\r\n");

  /* AHT20 初始化 */
  uint8_t aht20_ok = 1;
  if (AHT20_SoftInit() != 0)
  {
    aht20_ok = 0;
    LED_RED_ON();
    printf("AHT20 init failed!\r\n");
  }

  /* ---- ESP8266 + WiFi + OneNET 初始化 ---- */
  int wifi_ok = 0;
  int cloud_ok = 0;

  printf("\r\n--- ESP8266 Init ---\r\n");
  ESP8266_Init(&huart3);

  if (ESP8266_Reset() == ESP8266_OK)
  {
    printf("[SYS] Connecting WiFi: %s ...\r\n", WIFI_SSID);
    if (ESP8266_ConnectWiFi(WIFI_SSID, WIFI_PASSWORD) == ESP8266_OK)
    {
      wifi_ok = 1;
      cloud_ok = 0;
      LCD_String(50, 132, "OK   ", 12, GREEN, BROWN);
      printf("[SYS] WiFi connected!\r\n");

      /* 连接 OneNET（MQTT） */
      OneNET_Config cfg = {
        .product_id  = ONENET_PRODUCT_ID,
        .device_name = ONENET_DEVICE_NAME,
        .token       = ONENET_TOKEN,
        .server      = ONENET_SERVER_HOST,
        .port        = ONENET_SERVER_PORT,
      };
      OneNET_Init(&cfg);

      if (OneNET_Connect() == 0)
      {
        cloud_ok = 1;
        LCD_String(50, 252, "OK   ", 12, GREEN, BROWN);
        printf("[SYS] OneNET connected!\r\n");
      }
      else
      {
        LCD_String(50, 252, "FAIL ", 12, RED, BROWN);
        printf("[SYS] OneNET connect failed\r\n");
      }
    }
    else
    {
      LCD_String(50, 132, "FAIL ", 12, RED, BROWN);
      printf("[SYS] WiFi connect failed\r\n");
    }
  }
  else
  {
    LCD_String(50, 132, "N/A  ", 12, YELLOW, BROWN);
    printf("[SYS] ESP8266 init failed\r\n");
  }

  /* 主循环 */
  uint8_t mode = 0;
  uint32_t aht20_last = 0;
  uint32_t post_last = 0;
  float g_temp = 0.0f, g_humi = 0.0f;

  printf("\r\n=== Running ===\r\n");

  while (1)
  {
    /* 1. WiFi / MQTT 轮询（处理接收数据、心跳等） */
    OneNET_Process();

    /* 2. 串口 1 命令（LED 控制） */
    uint8_t c;
    if (HAL_UART_Receive(&huart1, &c, 1, 0) == HAL_OK)
    {
      if (c != '\r' && c != '\n')
      {
        uint8_t new_mode = 0xFF;
        if      (c == '1') new_mode = 1;
        else if (c == '2') new_mode = 2;
        else if (c == '3') new_mode = 3;
        else if (c == '0') new_mode = 0;

        if (new_mode != 0xFF)
        {
          mode = new_mode;
          LED_BLUE_OFF(); LED_RED_OFF();
          if (mode == 1 || mode == 3) LED_BLUE_ON();
          if (mode == 2 || mode == 3) LED_RED_ON();
        }
      }
    }

    /* 3. AHT20 温湿度采集（2s 周期） */
    if (HAL_GetTick() - aht20_last >= 2000)
    {
      aht20_last = HAL_GetTick();

      if (aht20_ok)
      {
        if (AHT20_SoftRead(&g_temp, &g_humi) == 0)
        {
          char b[32];
          int ti = (int)(g_temp * 10.0f);
          int hi = (int)(g_humi * 10.0f);
          int ta = (ti > 0) ? ti : -ti;
          int ha = (hi > 0) ? hi : -hi;

          LCD_Fill(0, 60, 239, 125, BLACK);
          snprintf(b, sizeof(b), "%s%d.%d C", (ti < 0) ? "-" : "", ta / 10, ta % 10);
          LCD_String(10, 80, b, 24, YELLOW, BLACK);

          LCD_Fill(0, 180, 239, 245, BLACK);
          snprintf(b, sizeof(b), "%d.%d %%RH", ha / 10, ha % 10);
          LCD_String(10, 200, b, 24, CYAN, BLACK);

          printf("[AHT20] T=%.1f C  H=%.1f %%RH\r\n", g_temp, g_humi);
        }
        else
        {
          printf("[AHT20] read failed, skip\r\n");
        }
      }
    }

    /* 4. 属性上报（5s 周期，仅云连接时）— 内联实现，避免栈溢出 */
    if (cloud_ok && HAL_GetTick() - post_last >= 5000)
    {
      post_last = HAL_GetTick();

      if (aht20_ok)
      {
        static int msg_id = 1;
        /* 所有 buffer 都用 static，不占栈 */
        static char topic[64];
        static char payload[128];
        static uint8_t pkt[384];

        /* 构造 topic */
        snprintf(topic, sizeof(topic), "$sys/%s/%s/dp/post/json",
                 ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);

        /* 构造 OneJSON payload */
        int plen = snprintf(payload, sizeof(payload),
            "{\"id\":%d,\"dp\":{\"%s\":[{\"v\":%.1f}],\"%s\":[{\"v\":%.1f}]}}",
            msg_id, ONENET_FIELD_TEMP, g_temp, ONENET_FIELD_HUMI, g_humi);

        /* 构造 MQTT PUBLISH 报文 */
        int pkt_len = MQTT_BuildPublish(pkt, sizeof(pkt), topic,
                                         (const uint8_t *)payload, (uint16_t)plen);
        if (pkt_len > 0 && ESP8266_SendData(pkt, (uint16_t)pkt_len) == ESP8266_OK)
        {
          printf("[SYS] publish OK, id=%d\r\n", msg_id);
          msg_id++;
        }
        else
        {
          printf("[SYS] publish FAIL\r\n");
          cloud_ok = 0;
          LCD_String(50, 252, "LOST", 12, RED, BROWN);
        }
      }
    }

    /* 5. 云连接丢失时尝试重连（每 10s 一次） */
    if (!cloud_ok && wifi_ok)
    {
      static uint32_t reconnect_last = 0;
      if (HAL_GetTick() - reconnect_last >= 10000)
      {
        reconnect_last = HAL_GetTick();
        printf("[SYS] Reconnecting OneNET...\r\n");
        LCD_String(50, 252, "....", 12, YELLOW, BROWN);

        /* 先测试 ESP8266 是否还活着 */
        printf("[SYS] Testing ESP8266 with Reset...\r\n");
        ESP8266_Status test_s = ESP8266_Reset();
        printf("[SYS] ESP8266 Reset result: %d\r\n", (int)test_s);

        OneNET_Disconnect();
        HAL_Delay(500);

        if (OneNET_Connect() == 0)
        {
          cloud_ok = 1;
          LCD_String(50, 252, "OK   ", 12, GREEN, BROWN);
          printf("[SYS] OneNET reconnected!\r\n");
        }
        else
        {
          LCD_String(50, 252, "FAIL ", 12, RED, BROWN);
          printf("[SYS] Reconnect failed\r\n");
        }
      }
    }

    HAL_Delay(10);
  }
}

/* ============================ Error_Handler ============================== */
void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) { }
#endif

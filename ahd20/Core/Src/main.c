/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : AHT20 温湿度 + ILI9341 LCD + ESP8266 WiFi + OneNET MQTT
  *
  *  板子：苏宙嵌孝教育 STM32-F103RCT6 V5.4.1
  *  MCU ：STM32F103RCT6 (256K Flash / 48K RAM / LQFP64)
  *  时钟：HSI 8MHz（无外部晶振）
  *  调试：板载 CMSIS-DAP（PA13/PA14）
  *  串口：USART1 (PA9/PA10) -> 板载 CH340 -> COM13 / 115200
  *  WiFi：ESP8266 接 USART3 (PC10=TX / PC11=RX) + RST=PC4
  *  云  ：OneNET Studio MQTT (mqtts.heclouds.com:1883)
  *
  *  引脚定义    ：Inc/pins.h  <- 改引脚只改这里
  *  引脚对照表  ：board_pinout.md
  *  详细避坑    ：README.md / DEBUG_SUMMARY.md
  *
  *  ! 不要动 #1：msp.c 里是 __HAL_AFIO_REMAP_SWJ_NOJTAG()
  *               改回 DISABLE 会永久失联 DAP-Link（PA13/PA14 变 GPIO）
  *
  *  ! 不要动 #2：Keil -> Options -> Target -> 勾选 Use MicroLIB
  *               否则启动时 newlib 走 semihosting 触发 BKPT 0xAB，main() 跑不到
  *
  *  ! 不要动 #3：Keil Debug Settings -> Connect=under Reset + Reset=Hardware
  *               DAP-Link 失联时用这个配置恢复
  *
  *  ! 板子丝印陷阱：PC10/PC11 丝印标 "UART4_TX/RX"，RCT6 没有 UART4/5，丝印是装饰
  *                   实际串口用 USART1 (PA9/PA10)
  ******************************************************************************
  */
/* USER CODE END Header */
#include "main.h"
#include "stm32f1xx_hal.h"
#include "pins.h"
#include "bsp_LCD_ILI9341.h"
#include "esp8266.h"
#include "onenet_mqtt.h"
#include "aht20_soft.h"
#include "app_tasks.h"
#include <stdio.h>
#include <string.h>

/* ============================ 用户配置（改这里！）========================== */

/* --- WiFi --- */
#define WIFI_SSID       "Xiaomi 15"
#define WIFI_PASSWORD   "88888888"

/* --- OneNET 配置 --- */
#define ONENET_SERVER_HOST  "mqtts.heclouds.com"
#define ONENET_SERVER_PORT  1883
#define ONENET_PRODUCT_ID   "7Sp4dA99m3"
#define ONENET_DEVICE_NAME  "dev1"

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

  __HAL_UART_ENABLE_IT(&huart3, UART_IT_RXNE);
  HAL_NVIC_SetPriority(USART3_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(USART3_IRQn);
}

/* ============================ 初始化包装 ================================== */

static void Init_Peripherals(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
}

static void Init_LCD_UI(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  LCD_Init();
  LCD_Fill(0, 0, 239, 319, BLACK);
  LCD_String(10, 5, "IoT AHT20+ESP8266", 16, WHITE, BLACK);
  LCD_Fill(0, 30, 239, 59, BLUE);
  LCD_String(10, 35, "Temperature:", 16, WHITE, BLUE);
  LCD_Fill(0, 150, 239, 179, GREEN);
  LCD_String(10, 155, "Humidity:", 16, WHITE, GREEN);
  LCD_Fill(0, 130, 239, 148, BROWN);
  LCD_String(10, 132, "WiFi:", 12, WHITE, BROWN);
  LCD_Fill(0, 250, 239, 268, BROWN);
  LCD_String(10, 252, "Cloud:", 12, WHITE, BROWN);

  printf("\r\n===== ahd20 IoT =====\r\n");
  printf("  '1'=Blue  '2'=Red  '3'=Both  '0'=Off\r\n");
  printf("======================\r\n\r\n");
}

static void Init_Network(int *wifi_ok, int *cloud_ok)
{
  *wifi_ok = 0;
  *cloud_ok = 0;

  printf("\r\n--- ESP8266 Init ---\r\n");
  ESP8266_Init(&huart3);

  if (ESP8266_Reset() != ESP8266_OK) {
    LCD_String(50, 132, "N/A  ", 12, YELLOW, BROWN);
    printf("[SYS] ESP8266 init failed\r\n");
    return;
  }

  printf("[SYS] Connecting WiFi: %s ...\r\n", WIFI_SSID);
  if (ESP8266_ConnectWiFi(WIFI_SSID, WIFI_PASSWORD) != ESP8266_OK) {
    LCD_String(50, 132, "FAIL ", 12, RED, BROWN);
    printf("[SYS] WiFi connect failed\r\n");
    return;
  }

  *wifi_ok = 1;
  LCD_String(50, 132, "OK   ", 12, GREEN, BROWN);
  printf("[SYS] WiFi connected!\r\n");

  static OneNET_Config cfg = {
    .product_id  = ONENET_PRODUCT_ID,
    .device_name = ONENET_DEVICE_NAME,
    .token       = ONENET_TOKEN,
    .server      = ONENET_SERVER_HOST,
    .port        = ONENET_SERVER_PORT,
  };
  OneNET_Init(&cfg);

  if (OneNET_Connect() == 0) {
    *cloud_ok = 1;
    LCD_String(50, 252, "OK   ", 12, GREEN, BROWN);
    printf("[SYS] OneNET connected!\r\n");
  } else {
    LCD_String(50, 252, "FAIL ", 12, RED, BROWN);
    printf("[SYS] OneNET connect failed\r\n");
  }
}

/* ============================ main ======================================= */
int main(void)
{
  Init_Peripherals();
  Init_LCD_UI();

  uint8_t aht20_ok = 1;
  if (AHT20_SoftInit() != 0) {
    aht20_ok = 0;
    LED_RED_ON();
    printf("AHT20 init failed!\r\n");
  }

  int wifi_ok = 0, cloud_ok = 0;
  Init_Network(&wifi_ok, &cloud_ok);

  App_Run(aht20_ok, wifi_ok, cloud_ok);
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


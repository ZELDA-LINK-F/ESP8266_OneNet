/**
  ******************************************************************************
  * @file    pins.h
  * @brief   引脚定义集中管理 —— 苏宙嵌孝教育 STM32-F103RCT6 V5.4.1
  *
  *   这个文件是板子引脚的唯一参考点。任何 .c 文件要操作引脚，先 #include "pins.h"
  *   改引脚只改这里，main.c / 中断处理 / BSP 驱动都不用动。
  ******************************************************************************
  */

#ifndef __PINS_H__
#define __PINS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* ===========================================================================
 * 板子信息（参考点）
 * ===========================================================================
 * 板子型号：苏宙嵌孝教育 STM32-F103RCT6 V5.4.1
 * MCU    ：STM32F103RCT6  (256K Flash / 48K RAM / LQFP64)
 * 调试器 ：板载 CMSIS-DAP  (走 PA13 SWDIO / PA14 SWCLK)
 * USB-TTL：板载 CH340      (USART1 → COM13)
 * 供电   ：CMSIS-DAP USB 接口 (5V)
 * 时钟   ：HSI 8MHz（默认，无外部晶振）
 * ===========================================================================
 */

/* ===========================================================================
 * ⚠ 关键避坑（不要改！）
 * ===========================================================================
 * 1. msp.c 里是 __HAL_AFIO_REMAP_SWJ_NOJTAG()
 *    千万不要改成 DISABLE —— PA13/PA14 永久变 GPIO，DAP-Link 失联
 *
 * 2. Keil → Options → Target → Use MicroLIB 必须勾选
 *    否则启动时 newlib 走 semihosting，CPU 卡死在 BKPT 0xAB（main() 跑不到）
 *
 * 3. Keil Debug Settings → Connect=under Reset + Reset=Hardware
 *    DAP-Link 失联时能恢复连接
 *
 * 4. 板子丝印 PC10/PC11 标 "UART4_TX/RX"，RCT6 没有 UART4/5，丝印是装饰
 * ===========================================================================
 */

/* ===========================================================================
 * LED —— 推挽输出，置低电平点亮
 * =========================================================================== */
#define LED_BLUE_PORT      GPIOB
#define LED_BLUE_PIN       GPIO_PIN_2     /* PB2  蓝灯（板子左侧） */
#define LED_RED_PORT       GPIOC
#define LED_RED_PIN        GPIO_PIN_5     /* PC5  红灯（板子右侧） */

#define LED_BLUE_ON()      HAL_GPIO_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, GPIO_PIN_RESET)
#define LED_BLUE_OFF()     HAL_GPIO_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, GPIO_PIN_SET)
#define LED_BLUE_TOGGLE()  HAL_GPIO_TogglePin(LED_BLUE_PORT, LED_BLUE_PIN)

#define LED_RED_ON()       HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET)
#define LED_RED_OFF()      HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET)
#define LED_RED_TOGGLE()   HAL_GPIO_TogglePin(LED_RED_PORT, LED_RED_PIN)

/* ===========================================================================
 * 按键 —— 输入上拉/下拉，触发逻辑见 KEYx_PRESSED
 * =========================================================================== */
#define KEY1_PORT          GPIOA
#define KEY1_PIN           GPIO_PIN_0     /* PA0  KEY_1（Wakeup 键） */
#define KEY1_PRESSED       GPIO_PIN_SET   /* PA0 按下 = 高电平（外部下拉到 GND） */

#define KEY2_PORT          GPIOA
#define KEY2_PIN           GPIO_PIN_1     /* PA1  KEY_2 */
#define KEY2_PRESSED       GPIO_PIN_RESET /* PA1 按下 = 低电平（外部上拉到 VCC） */

#define KEY3_PORT          GPIOA
#define KEY3_PIN           GPIO_PIN_4     /* PA4  KEY_3 */
#define KEY3_PRESSED       GPIO_PIN_RESET /* PA4 按下 = 低电平（外部上拉到 VCC） */

#define KEY1_READ()        HAL_GPIO_ReadPin(KEY1_PORT, KEY1_PIN)
#define KEY2_READ()        HAL_GPIO_ReadPin(KEY2_PORT, KEY2_PIN)
#define KEY3_READ()        HAL_GPIO_ReadPin(KEY3_PORT, KEY3_PIN)

/* 统一"按下 = 1"的判断宏 */
#define KEY1_IS_DOWN()     (KEY1_READ() == KEY1_PRESSED)
#define KEY2_IS_DOWN()     (KEY2_READ() == KEY2_PRESSED)
#define KEY3_IS_DOWN()     (KEY3_READ() == KEY3_PRESSED)

/* ===========================================================================
 * USART1 —— 板载 CH340 → COM13
 * =========================================================================== */
#define USART1_TX_PORT     GPIOA
#define USART1_TX_PIN      GPIO_PIN_9
#define USART1_RX_PORT     GPIOA
#define USART1_RX_PIN      GPIO_PIN_10
#define USART1_BAUDRATE    115200
#define USART1_INSTANCE    USART1

/* ===========================================================================
 * I2C1 —— AHT20 温湿度传感器（板载 10K 上拉到 3.3V）
 * =========================================================================== */
#define I2C1_SCL_PORT      GPIOB
#define I2C1_SCL_PIN       GPIO_PIN_6
#define I2C1_SDA_PORT      GPIOB
#define I2C1_SDA_PIN       GPIO_PIN_7
#define I2C1_CLOCK_SPEED   100000        /* 100 KHz */
#define I2C1_INSTANCE      I2C1

/* ===========================================================================
 * USART3 —— ESP8266 WiFi 模块（板上跳线帽切到 ESP8266）
 *           PC10 = TX (AF_PP) / PC11 = RX (浮空输入)
 * =========================================================================== */
#define USART3_TX_PORT     GPIOC
#define USART3_TX_PIN      GPIO_PIN_10
#define USART3_RX_PORT     GPIOC
#define USART3_RX_PIN      GPIO_PIN_11
#define USART3_BAUDRATE    115200
#define USART3_INSTANCE    USART3

/* ===========================================================================
 * ESP8266 控制引脚
 * =========================================================================== */
#define ESP8266_RST_PORT   GPIOC
#define ESP8266_RST_PIN    GPIO_PIN_4    /* PC4, 低电平复位 */

/* ===========================================================================
 * 调试器 SWD（板载 DAP-Link，不要改）
 * =========================================================================== */
#define SWDIO_PORT         GPIOA
#define SWDIO_PIN          GPIO_PIN_13
#define SWCLK_PORT         GPIOA
#define SWCLK_PIN          GPIO_PIN_14

#ifdef __cplusplus
}
#endif

#endif /* __PINS_H__ */

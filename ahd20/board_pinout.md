# STM32-F103RCT6 板子引脚表

> 板子：**苏宙嵌孝教育 STM32-F103RCT6 V5.4.1**
> 创建日期：2026-07-14
> 资料来源：板子背面丝印 + 实测验证

---

## 🔑 按键（**重点关注**）

| 标号 | 引脚 | 触发逻辑 | 备注 |
|------|------|----------|------|
| **KEY_1** | **PA0** | **按下高电平** | PA0 也是 WKUP 待机唤醒引脚 |
| **KEY_2** | **PA1** | **按下低电平** | 普通 GPIO，最常见接法 |
| **KEY_3** | **PA4** | **按下低电平** | 普通 GPIO，最常见接法 |
| RESET | NRST | 复位 | 板子右下角有 RESET 按钮 |

> ⚠️ **KEY_1 与 KEY_2/KEY_3 触发逻辑不同！** KEY_1 按下时 PA0 变高，KEY_2/KEY_3 按下时对应引脚变低。代码初始化时需要注意：
> - KEY_1：配置为下拉输入（`GPIO_PULLDOWN`），空闲低电平，按下高电平
> - KEY_2/KEY_3：配置为上拉输入（`GPIO_PULLUP`），空闲高电平，按下低电平

---

## 💡 LED（已验证）

| 标号 | 引脚 | 触发逻辑 | 备注 |
|------|------|----------|------|
| RED   | **PC5** | 置低电平点亮 | 已实测：板子最右红色 LED 1Hz 翻转正常 |
| BLUE  | **PB2** | 置低电平点亮 | 已实测：板子最左蓝色 LED 1Hz 翻转正常 |

---

## 🔌 I2C（板载 24C02 EEPROM）

| 信号 | 引脚 | 备注 |
|------|------|------|
| I2C1_SCL | **PB6** | 板载 10K 电阻上拉 3.3V |
| I2C1_SDA | **PB7** | 板载 10K 电阻上拉 3.3V |
| 器件地址 | 0b1010000 | = 0x50（24C02） |

> 📌 板载已经有 10K 上拉电阻，**AHT20 外部 4.7K 上拉可加可不加**（已经能正常通讯）

---

## 📡 串口 / UART

| 标号 | 引脚 | 用途 | 备注 |
|------|------|------|------|
| USART1_TX | **PA9** | 板载 CH340 USB转TTL 的 RXD | **已验证：COM13 收到 printf 数据** |
| USART1_RX | **PA10** | 板载 CH340 USB转TTL 的 TXD | 同上 |
| UART4_TX | PC10 | RS-485 / ESP8266（跳线帽切换） | 板子标"UART4"，实际 RCT6 没有 UART4，**丝印可能错** |
| UART4_RX | PC11 | 同上 | 同上 |

> 📌 默认调试串口走 USART1 (PA9/PA10) → 板载 CH340 → USB → COM13

---

## 🔥 SPI Flash (W25Q128)

| 信号 | 引脚 |
|------|------|
| CS   | **PC13** |
| SCK  | **PA5** |
| MISO | **PA6** |
| MOSI | **PA7** |

---

## 🚌 CAN

| 信号 | 引脚 |
|------|------|
| CAN1_TX | **PA12** |
| CAN1_RX | **PA11** |

---

## 💾 TF-CARD (SDIO)

| 信号 | 引脚 |
|------|------|
| D0  | PC8 |
| D1  | PC9 |
| D2  | PC10 |
| D3  | PC11 |
| CMD | PD2 |
| CLK | PC12 |

---

## 🐛 调试 / 板载外设

| 标号 | 引脚/位置 | 备注 |
|------|----------|------|
| 板载 CH340 | USART1 (PA9/PA10) | 板载 USB 转 TTL，已验证可用 |
| 板载 CMSIS-DAP | PA13 (SWDIO) / PA14 (SWCLK) | 板载 DAP-Link 调试器 |
| 板载 TFT LCD | LCD 专用接口 | 板子顶部排针（绿色） |
| 板载 USB-Slave | Type-C 接口 | 直连 STM32 USB 引脚（STM32F103 无 USB OTG，**该接口可能仅供电**） |

---

## 🔌 电源排针

- 3.3V（多处）
- 5V（多处）
- GND（多处）

---

## 📊 占用汇总（参考）

| 引脚 | 占用 | 状态 |
|------|------|------|
| PA0  | KEY_1 | 空闲（用户可调用） |
| PA1  | KEY_2 | 空闲（用户可调用） |
| PA2  | USART2_TX | **未启用**（如需 USART2，可直接用） |
| PA3  | USART2_RX | 同上 |
| PA4  | KEY_3 | 空闲（用户可调用） |
| PA5  | SPI1_SCK | W25Q128 占用（如不用可释放） |
| PA6  | SPI1_MISO | 同上 |
| PA7  | SPI1_MOSI | 同上 |
| PA8  | — | 空闲（**可做用户 LED/按键**） |
| PA9  | USART1_TX | 板载 CH340 占用（**调试串口**） |
| PA10 | USART1_RX | 同上 |
| PA11 | CAN1_RX | 空闲（CAN 未启用） |
| PA12 | CAN1_TX | 空闲（CAN 未启用） |
| PA13 | SWDIO | **调试器占用**（不可挪用） |
| PA14 | SWCLK | **调试器占用**（不可挪用） |
| PA15 | — | 空闲 |
| PB0  | — | 空闲 |
| PB1  | — | 空闲 |
| PB2  | BLUE_LED | **已使用**（蓝色 LED） |
| PB3  | — | 空闲 |
| PB4  | — | 空闲 |
| PB5  | — | 空闲（**常被用作 LCD 背光**） |
| PB6  | I2C1_SCL | **AHT20 / 24C02 占用** |
| PB7  | I2C1_SDA | 同上 |
| PB8  | — | 空闲 |
| PB9  | — | 空闲 |
| PB10 | USART3_TX / I2C2_SCL | 空闲（如需 USART3 可直接用） |
| PB11 | USART3_RX / I2C2_SDA | 同上 |
| PB12 | — | 空闲（**原计划用作用户 LED，已知板上没接**） |
| PB13 | — | 空闲 |
| PB14 | — | 空闲 |
| PB15 | — | 空闲 |
| PC0  | — | 空闲 |
| PC1  | — | 空闲 |
| PC2  | — | 空闲 |
| PC3  | — | 空闲 |
| PC4  | — | 空闲 |
| PC5  | RED_LED | **已使用**（红色 LED） |
| PC6  | — | 空闲 |
| PC7  | — | 空闲 |
| PC8  | SDIO_D0 | TF 卡占用 |
| PC9  | SDIO_D1 | 同上 |
| PC10 | SDIO_D2 / UART4_TX | 占用（与 TF 卡共用） |
| PC11 | SDIO_D3 / UART4_RX | 同上 |
| PC12 | SDIO_CLK | TF 卡占用 |
| PC13 | SPI_CS / LED 候选 | W25Q128 CS 占用（**板上没接 LED**） |
| PC14 | — | 空闲 |
| PC15 | — | 空闲 |
| PD0  | — | 空闲 |
| PD1  | — | 空闲 |
| PD2  | SDIO_CMD | TF 卡占用 |

---

## 🧪 引脚查询速查（按用途）

### 按键
```c
// KEY_1: PA0, 按下高电平 —— 用下拉输入
GPIO_InitStruct.Pin = GPIO_PIN_0;
GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
GPIO_InitStruct.Pull = GPIO_PULLDOWN;   // 空闲低
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

// KEY_2/KEY_3: PA1/PA4, 按下低电平 —— 用上拉输入
GPIO_InitStruct.Pin = GPIO_PIN_1;  // 或 GPIO_PIN_4
GPIO_InitStruct.Pull = GPIO_PULLUP;   // 空闲高
HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
```

### 读按键电平
```c
uint8_t k1 = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_SET);  // 1 = 按下
uint8_t k2 = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET); // 0 = 按下
uint8_t k3 = (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET); // 0 = 按下
```

---

## 📝 待补

- [ ] 板载 LCD 具体型号 + 接口（FSMC/8080/SPI？）+ 控制引脚
- [ ] 板载 DAP-Link 固件版本（决定 V1/V2 协议，影响 SWD 速度）
- [ ] 板子是否带 RTC 纽扣电池（看图2 右下角有电池座，是 CR1220？）

---

> 维护：所有项目成员都能改这个文件。改引脚时**必须验证板上丝印**+ **实测**，不要凭印象。

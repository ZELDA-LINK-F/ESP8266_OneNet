# main.c 轻量化重构方案

> 目标：将 main.c 从 643 行精简到约 80 行，`main()` 只保留顶层函数调用。
> 原则：纯搬迁，零逻辑改动，所有现有功能保持不变。

---

## 一、现状分析

| 指标 | 数值 |
|------|------|
| main.c 总行数 | **643 行** |
| 软件 I2C + AHT20 代码 | 170~402 行（**232 行**，占 36%） |
| 主循环逻辑（while(1)） | 500~631 行（**130 行**，占 20%） |
| 数据上报内联 MQTT 拼接 | 559~596 行（**38 行**，与已有 API 重复） |
| 外设初始化 | 95~168 行（74 行） |
| LCD UI 初始化 | 414~426 行（13 行） |
| ESP8266 + OneNET 初始化 | 441~490 行（50 行） |

**冗余发现：** `onenet_mqtt.c` 中已有 `OneNET_PublishDataPoint()` 函数（255 行文件的 207~231 行），功能与 main.c 内联的 MQTT PUBLISH 代码完全一致，可直接复用。

---

## 二、重构方案总览

```
重构前：                          重构后：
main.c (643 行)                   main.c (~80 行)
├── includes                      ├── includes + 用户配置宏
├── fputc() 重定向                ├── fputc() 重定向
├── SystemClock_Config()          │
├── MX_GPIO_Init()                ├── Init_Peripherals() ← static 包装
├── MX_USART1_UART_Init()         │
├── MX_USART3_UART_Init()         │
├── 软件 I2C + AHT20 ────────────→ Drivers/BSP/AHT20/aht20_soft.c/h (新建)
├── main()                        ├── Init_LCD_UI()       ← static 包装
│   ├── LCD 初始化                ├── Init_Network()      ← static 包装
│   ├── AHT20 初始化              │
│   ├── ESP8266 + OneNET 初始化   ├── main()
│   └── while(1) ────────────────→ Core/Src/app_tasks.c/h         (新建)
│       ├── 串口命令解析               └── App_Run()
│       ├── AHT20 采集+LCD显示
│       ├── 云平台数据上报
│       └── 断线重连
└── Error_Handler()               └── Error_Handler()
```

---

## 三、新建文件清单

### 3.1 `Drivers/BSP/AHT20/aht20_soft.h`

```c
#ifndef __AHT20_SOFT_H__
#define __AHT20_SOFT_H__

#include <stdint.h>

/**
 * @brief  软件 I2C 初始化 AHT20（软复位 + 校准）
 * @retval 0=成功, 非0=失败
 */
uint8_t AHT20_SoftInit(void);

/**
 * @brief  通过软件 I2C 读取温湿度
 * @param  t [out] 温度值（℃）
 * @param  h [out] 湿度值（%RH）
 * @retval 0=成功, 非0=失败（CRC 错误 / 超时）
 */
uint8_t AHT20_SoftRead(float *t, float *h);

#endif /* __AHT20_SOFT_H__ */
```

### 3.2 `Drivers/BSP/AHT20/aht20_soft.c`

**内容来源：** main.c 第 170~402 行（完整搬迁，不动任何逻辑）

| 包含内容 | 备注 |
|----------|------|
| `#define SI2C_SCL_PIN 6` / `SI2C_SDA_PIN 7` | 引脚定义 |
| `si2c_delay()` ~ `si2c_read_byte()` | 软件 I2C 底层（static） |
| `sda_restore_for_lcd()` | 恢复 PB7 为推挽输出，避免 LCD 花屏（static） |
| `AHT20_CRC8()` | CRC 校验（static） |
| `#define AHT20_ADDR 0x38` 等 | AHT20 命令宏 |
| `AHT20_SoftInit()` | 对外函数 |
| `AHT20_SoftRead()` | 对外函数 |

**依赖头文件：**
```c
#include "aht20_soft.h"
#include "stm32f1xx_hal.h"   /* HAL_Delay, __NOP */
#include "pins.h"             /* LED_RED_TOGGLE */
#include <stdio.h>            /* printf */
```

### 3.3 `Core/Inc/app_tasks.h`

```c
#ifndef __APP_TASKS_H__
#define __APP_TASKS_H__

#include <stdint.h>

/**
 * @brief  应用全局状态（主循环中各任务共享）
 */
typedef struct {
    uint8_t  mode;               /* LED 控制模式: 0=关 1=蓝 2=红 3=全亮 */
    uint8_t  aht20_ok;           /* AHT20 传感器是否正常 */
    int      wifi_ok;            /* WiFi 是否已连接 */
    int      cloud_ok;           /* OneNET MQTT 是否已连接 */
    float    temp;               /* 最新温度值（℃） */
    float    humi;               /* 最新湿度值（%RH） */
    uint32_t aht20_last_ms;      /* 上次 AHT20 读数时间戳 */
    uint32_t post_last_ms;       /* 上次 OneNET 上报时间戳 */
    uint32_t reconnect_last_ms;  /* 上次重连尝试时间戳 */
    int      msg_id;             /* OneNET 消息递增 ID */
} AppState;

/**
 * @brief  应用程序主循环入口（永不返回）
 * @param  aht20_ok  AHT20 初始化结果: 1=正常 0=失败
 * @param  wifi_ok   WiFi 连接结果: 1=成功 0=失败
 * @param  cloud_ok  云连接结果: 1=成功 0=失败
 */
void App_Run(uint8_t aht20_ok, int wifi_ok, int cloud_ok);

#endif /* __APP_TASKS_H__ */
```

### 3.4 `Core/Src/app_tasks.c`

**内容来源：** main.c 主循环（第 500~631 行）拆分为 4 个任务函数。

```c
#include "app_tasks.h"
#include "stm32f1xx_hal.h"
#include "pins.h"
#include "bsp_LCD_ILI9341.h"
#include "onenet_mqtt.h"
#include "mqtt_raw.h"
#include "esp8266.h"
#include "aht20_soft.h"
#include <stdio.h>

/* ---- 用户配置宏（从 main.c 迁移）---- */
#define ONENET_FIELD_TEMP   "SHT30_T"
#define ONENET_FIELD_HUMI   "SHT30_H"

/* ---- 内部辅助 ---- */
extern UART_HandleTypeDef huart1;

/* ---- 任务函数 ---- */

/**
 * @brief  串口命令解析（按键 0-3 控制 LED）
 */
static void Console_Task(AppState *s)
{
    uint8_t c;
    if (HAL_UART_Receive(&huart1, &c, 1, 0) == HAL_OK) {
        if (c != '\r' && c != '\n') {
            uint8_t new_mode = 0xFF;
            if      (c == '1') new_mode = 1;
            else if (c == '2') new_mode = 2;
            else if (c == '3') new_mode = 3;
            else if (c == '0') new_mode = 0;
            if (new_mode != 0xFF) {
                s->mode = new_mode;
                LED_BLUE_OFF(); LED_RED_OFF();
                if (s->mode == 1 || s->mode == 3) LED_BLUE_ON();
                if (s->mode == 2 || s->mode == 3) LED_RED_ON();
            }
        }
    }
}

/**
 * @brief  AHT20 传感器采集 + LCD 显示（每 2 秒）
 */
static void Sensor_Task(AppState *s)
{
    if (HAL_GetTick() - s->aht20_last_ms < 2000) return;
    s->aht20_last_ms = HAL_GetTick();

    if (!s->aht20_ok) return;

    if (AHT20_SoftRead(&s->temp, &s->humi) == 0) {
        char b[32];
        int ti = (int)(s->temp * 10.0f);
        int hi = (int)(s->humi * 10.0f);
        int ta = (ti > 0) ? ti : -ti;
        int ha = (hi > 0) ? hi : -hi;

        LCD_Fill(0, 60, 239, 125, BLACK);
        snprintf(b, sizeof(b), "%s%d.%d C", (ti < 0) ? "-" : "", ta / 10, ta % 10);
        LCD_String(10, 80, b, 24, YELLOW, BLACK);

        LCD_Fill(0, 180, 239, 245, BLACK);
        snprintf(b, sizeof(b), "%d.%d %%RH", ha / 10, ha % 10);
        LCD_String(10, 200, b, 24, CYAN, BLACK);

        printf("[AHT20] T=%.1f C  H=%.1f %%RH\r\n", s->temp, s->humi);
    } else {
        printf("[AHT20] read failed, skip\r\n");
    }
}

/**
 * @brief  OneNET 云平台数据上报（每 5 秒）
 * @note   改用已有的 OneNET_PublishDataPoint() 替代内联 MQTT 拼接
 */
static void Cloud_Task(AppState *s)
{
    if (!s->cloud_ok || !s->aht20_ok) return;
    if (HAL_GetTick() - s->post_last_ms < 5000) return;
    s->post_last_ms = HAL_GetTick();

    int rc = OneNET_PublishDataPoint(s->msg_id,
                                      ONENET_FIELD_TEMP, (double)s->temp,
                                      ONENET_FIELD_HUMI, (double)s->humi);
    if (rc == 0) {
        printf("[SYS] publish OK, id=%d\r\n", s->msg_id);
        s->msg_id++;
    } else {
        printf("[SYS] publish FAIL\r\n");
        s->cloud_ok = 0;
        LCD_String(50, 252, "LOST", 12, RED, BROWN);
    }
}

/**
 * @brief  OneNET 断线自动重连（每 10 秒）
 */
static void Reconnect_Task(AppState *s)
{
    if (s->cloud_ok || !s->wifi_ok) return;
    if (HAL_GetTick() - s->reconnect_last_ms < 10000) return;
    s->reconnect_last_ms = HAL_GetTick();

    printf("[SYS] Reconnecting OneNET...\r\n");
    LCD_String(50, 252, "....", 12, YELLOW, BROWN);

    printf("[SYS] Testing ESP8266 with Reset...\r\n");
    ESP8266_Status test_s = ESP8266_Reset();
    printf("[SYS] ESP8266 Reset result: %d\r\n", (int)test_s);

    OneNET_Disconnect();
    HAL_Delay(500);

    if (OneNET_Connect() == 0) {
        s->cloud_ok = 1;
        LCD_String(50, 252, "OK   ", 12, GREEN, BROWN);
        printf("[SYS] OneNET reconnected!\r\n");
    } else {
        LCD_String(50, 252, "FAIL ", 12, RED, BROWN);
        printf("[SYS] Reconnect failed\r\n");
    }
}

/* ---- 主循环入口 ---- */
void App_Run(uint8_t aht20_ok, int wifi_ok, int cloud_ok)
{
    AppState s = {0};
    s.aht20_ok = aht20_ok;
    s.wifi_ok  = wifi_ok;
    s.cloud_ok = cloud_ok;

    printf("\r\n=== Running ===\r\n");

    while (1) {
        OneNET_Process();
        Console_Task(&s);
        Sensor_Task(&s);
        Cloud_Task(&s);
        Reconnect_Task(&s);
        HAL_Delay(10);
    }
}
```

---

## 四、main.c 修改内容

### 4.1 新增的 include

```c
#include "aht20_soft.h"   /* 替代原来内联的软件 I2C 代码 */
#include "app_tasks.h"    /* 主循环任务调度 */
```

### 4.2 移除的代码

| 删除行号 | 内容 | 去处 |
|---------|------|------|
| 170~402 | 软件 I2C + AHT20 全部代码 | -> `aht20_soft.c` |
| 500~631 | while(1) 主循环 | -> `app_tasks.c` 的 `App_Run()` |

同时移除 `#include "mqtt_raw.h"`（main.c 不再直接调用 MQTT_BuildPublish，改由 OneNET_PublishDataPoint 内部调用）。

### 4.3 新增的 static 包装函数（在 main.c 内部）

```c
/* ============================ 初始化包装 ================================ */

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

    OneNET_Config cfg = {
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
```

### 4.4 重构后的 `main()`

```c
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
```

---

## 五、需要同步修改的工程配置

### 5.1 Keil 工程（`MDK-ARM/ahd20.uvprojx`）

在 `<Groups>` 中新增目标组或加入现有组：

| 文件 | 所属 Group |
|------|-----------|
| `Drivers/BSP/AHT20/aht20_soft.c` | `Drivers/BSP` |
| `Core/Src/app_tasks.c` | `Application/User` |

新增头文件路径（`Options -> C/C++ -> Include Paths`，如果路径未覆盖）：

| 路径 |
|------|
| `..\Drivers\BSP\AHT20` |
| `..\Core\Inc` |

### 5.2 CubeMX（`ahd20.ioc`）

**不需要修改。** 只搬迁代码，不改变任何外设配置。CubeMX 重新生成代码时不会覆盖 `aht20_soft.c`、`app_tasks.c`（这些不在 CubeMX 管理范围内）。

---

## 六、安全性逐项确认

| # | 检查项 | 结论 |
|---|--------|------|
| 1 | `AHT20_SoftInit/SoftRead` 函数签名不变 | 搬迁后调用方式完全一致 |
| 2 | 软件 I2C 依赖的 `GPIOB->CRL/BSRR/IDR` 寄存器在任何 .c 文件均可访问 | CMSIS 全局定义 |
| 3 | `sda_restore_for_lcd()` 在函数返回前自动调用 | 两个函数末尾均已调用 |
| 4 | `OneNET_PublishDataPoint()` 与内联代码行为等价 | 均使用 static 缓冲区 + 相同 Topic/Payload 格式 |
| 5 | `ESP8266_Reset()` 在重连时的环形缓冲操作与现有代码一致 | 未改变调用顺序 |
| 6 | `huart1` 在 `app_tasks.c` 中通过 `extern` 声明可见 | 与中断处理文件中的用法一致 |
| 7 | 所有 `#define` 宏在迁移后仍可见 | 通过头文件 #include 链可达 |
| 8 | 不涉及 msp.c 的 SWJ 配置 | AGENTS.md 避坑 #1 不受影响 |
| 9 | 不涉及 Keil MicroLIB 选项 | 避坑 #2 不受影响 |

---

## 七、验证方法

重构完成后，按以下步骤验证功能完整性：

| 步骤 | 操作 | 预期结果 | 对应原功能 |
|:---:|------|---------|-----------|
| 1 | F7 编译 -> F8 烧录 | 编译零错误零警告 | — |
| 2 | 观察 LCD 启动画面 | 显示 Temperature: / Humidity: / WiFi: / Cloud: 四行 | LCD UI 初始化 |
| 3 | 连接 COM13 串口助手 | 看到 `===== ahd20 IoT =====` 启动横幅 | printf 重定向 |
| 4 | 观察 ESP8266 初始化日志 | 看到 `[ESP] Ready` / `WiFi connected` / `OneNET connected` | 网络初始化 |
| 5 | 等待 2 秒 | LCD 上温度/湿度数值更新，串口输出 `[AHT20] T=... H=...` | 传感器采集 |
| 6 | 登录 OneNET 控制台 | 每 5 秒收到一条温湿度数据点 | 云平台上报 |
| 7 | 串口发送 `1` / `2` / `3` / `0` | 蓝灯/红灯按预期亮灭 | LED 串口控制 |
| 8 | 断开 WiFi 路由器 | LCD 显示 Cloud: LOST，10 秒后自动重连 | 断线重连 |

---

## 八、文件变动汇总

| 操作 | 文件 |
|------|------|
| **新建** | `ahd20/Drivers/BSP/AHT20/aht20_soft.h` |
| **新建** | `ahd20/Drivers/BSP/AHT20/aht20_soft.c` |
| **新建** | `ahd20/Core/Inc/app_tasks.h` |
| **新建** | `ahd20/Core/Src/app_tasks.c` |
| **修改** | `ahd20/Core/Src/main.c`（643 行 -> ~80 行） |
| **修改** | `ahd20/MDK-ARM/ahd20.uvprojx`（注册新源文件） |
| **不动** | 其他所有文件（pins.h、esp8266、onenet_mqtt、LCD 驱动、HAL 库等） |

# AGENTS.md

IoT 工程实训项目 — STM32F103RCT6 + ESP8266 + AHT20 + ILI9341 LCD + OneNET MQTT

## 仓库结构

```
工程实训/
├── ahd20/                  # 主固件工程（Keil V5 + HAL 库）
│   ├── Core/Src/main.c     # 主程序入口
│   ├── Inc/pins.h          # 引脚集中定义（改引脚只改这里）
│   ├── Drivers/BSP/        # 板级驱动（AHT20、ESP8266、LCD）
│   ├── MDK-ARM/ahd20.uvprojx  # Keil 工程文件
│   └── ahd20.ioc           # CubeMX 配置（MCU 以 .ioc 为准）
├── STM_249044368/          # Altium Designer PCB 源文件
├── OneNET资料/             # OneNET 云平台参考资料
├── 报告素材/               # 实验截图/视频素材
├── onenet_token_calc.py    # OneNET 产品级 token 计算脚本
├── onenet_mqtt_calc.py     # OneNET 签名算法测试脚本
└── OneNET_改动说明.md      # OneNET 集成改动清单
```

## 工具链

- **固件**: Keil V5（无 CLI 构建，只能 IDE 里 F7 编译 / F8 烧录）
- **PCB**: Altium Designer 20.2.6
- **时钟**: HSI 8MHz（无外部晶振，PLL 未启用）
- **调试器**: 板载 CMSIS-DAP（SWD，PA13/PA14）
- **串口**: 板载 CH340 → USART1 (PA9/PA10) → COM13 @ 115200 8N1

## 板子信息

**苏宙嵌孝教育 STM32-F103RCT6 V5.4.1**

| 资源 | 说明 |
|------|------|
| MCU | STM32F103RCT6 (256K Flash / 48K RAM / LQFP64) |
| 调试器 | 板载 CMSIS-DAP（PA13 SWDIO / PA14 SWCLK） |
| USB-TTL | 板载 CH340（COM13 / 115200） |
| 供电 | CMSIS-DAP USB 接口（5V） |

## 引脚速查

| 功能 | 引脚 | 触发逻辑 | 备注 |
|------|------|----------|------|
| 蓝灯（左） | PB2 | 低电平点亮 | `LED_BLUE_*` 宏 |
| 红灯（右） | PC5 | 低电平点亮 | `LED_RED_*` 宏 |
| KEY_1 | PA0 | 按下高电平 | Wakeup 键，下拉输入 |
| KEY_2 | PA1 | 按下低电平 | 上拉输入 |
| KEY_3 | PA4 | 按下低电平 | 上拉输入 |
| USART1 | PA9/PA10 | — | 板载 CH340 → COM13 |
| USART3 | PC10/PC11 | — | ESP8266（需部分重映射） |
| I2C1 | PB6/PB7 | — | AHT20（软件模拟 I2C） |
| ESP8266 RST | PC4 | 低电平复位 | 当前代码跳过硬件 RST |
| SWD | PA13/PA14 | — | 调试器占用，不可挪用 |

详细定义见 `ahd20/Inc/pins.h`，完整引脚占用表见 `ahd20/board_pinout.md`。

## 关键避坑（必须遵守）

### 1. msp.c 的 SWJ 配置

`Core/Src/stm32f1xx_hal_msp.c` 中 **必须** 保持：
```c
__HAL_AFIO_REMAP_SWJ_NOJTAG();   // 保留 SWD，关 JTAG
```
**禁止** 使用 `__HAL_AFIO_REMAP_SWJ_DISABLE()` —— 会同时关闭 JTAG 和 SWD，PA13/PA14 永久变 GPIO，DAP-Link 永久失联（只能用串口 ISP 救）。

### 2. Keil 必须勾选 Use MicroLIB

Keil → Options for Target → Target → **勾选 Use MicroLIB**

不勾选时，启动时 newlib 走 semihosting（`BKPT 0xAB`），CPU 卡死，main() 永远跑不到。现象：F8 烧录成功但板子完全无反应。

### 3. Keil Debug 恢复设置

Keil → Options → Debug → Settings：
- Connect: `under Reset`
- Reset: `Hardware`

DAP-Link 失联时：按住板子 RESET 按钮不松 → F8 Load → 看到 `Flash Load finished` 才松手。

### 4. 板子丝印陷阱

PC10/PC11 丝印标 "UART4_TX/RX" —— **RCT6 没有 UART4/5**，丝印是装饰。实际 ESP8266 接在 USART3 部分重映射到 PC10/PC11。

### 5. USART3 必须显式开重映射

HAL 代码中必须在 MspInit 里调用：
```c
__HAL_RCC_AFIO_CLK_ENABLE();
__HAL_AFIO_REMAP_USART3_PARTIAL();  // USART3 从 PB10/PB11 重映射到 PC10/PC11
```
不调用则 USART3 跑在 PB10/PB11，PC10/PC11 配的 GPIO 是空的，ESP8266 不响应。

### 6. PB6/PB7 引脚冲突 — 软件 I2C

PB6/PB7 同时接 LCD 数据总线 D6/D7 和 I2C1。**不能用硬件 I2C（HAL 库）**，必须用软件模拟 I2C。I2C 通信结束后必须调用 `sda_restore_for_lcd()` 恢复 PB7 为推挽输出，否则 LCD 花屏。

### 7. Keil 编译缓存

改了 `.c` 文件后，Keil 可能 link 旧 `.o` 文件。**必须**：`Project → Clean Targets` → 再 F7。或手动删 `MDK-ARM/Objects/` 下对应 `.o`。

### 8. Keil 工程树同步

新增/删除 `.c` 文件后，**必须** 在 `.uvprojx` 的 `<Groups>` 中同步注册/移除。复制文件到目录不等于加入工程。

### 9. font.h 中文编码

`font.h` 中 `z_GB_16[]` 的中文字符必须用 GB2312 原始 hex 字节（如 `0xCE,0xC2`），不能用字符串字面量（文件流转可能变成 UTF-8，ARMCC V5 报错）。

### 10. ARMCC V5 移位溢出

移位运算的常量必须加 `U` 后缀：`0xFU << 28`，否则 `0xF << 28` 超出 `int32` 范围产生警告。

## ESP8266 + OneNET 集成

### 通信链路

```
STM32 → USART3 (PC10/PC11) → ESP8266 (AT 指令) → WiFi → OneNET Studio MQTT
```

### OneNET 配置

| 配置项 | 值 |
|--------|-----|
| Broker | `mqtts.heclouds.com:1883`（非 TLS）|
| Product ID | `7Sp4dA99m3` |
| Device | `dev1` |
| Auth | 产品级 token（access_key + md5） |
| 上行 Topic | `$sys/7Sp4dA99m3/dev1/dp/post/json` |
| 数据格式 | OneJSON（`dp` 字段，值用 `v` 包裹在数组里） |

### Token 计算

```python
import hashlib, base64, urllib.parse
key = '6/HRq7d7o5/DvRet5Wm7wfcdWVVbw6Gbu5nRzTdjhVc'
et  = 1893456000  # 2030-01-01
sig = base64.b64encode(hashlib.md5((key + str(et)).encode()).digest()).decode()
token = f'version=2018-10-31&res=products%2F7Sp4dA99m3&et={et}&method=md5&sign={urllib.parse.quote(sig)}'
```

Token 写死在 `main.c` 顶部 `ONENET_TOKEN_MODE1` 宏里。

### ESP8266 AT 指令关键点

- 用 `AT+MQTTPUBRAW`（不是 `AT+MQTTPUB`）发 JSON —— RAW 模式直接发字节，避免双引号被 AT 解析器搞乱
- ESP8266 RST 引脚当前跳过硬件复位（板载 RST 引脚未确认），直接发 `AT` 测试
- ESP8266 只支持 2.4GHz WiFi
- PC 和 ESP8266 必须在同一 WiFi 网络

### 烧录前检查清单

1. `main.c` 顶部 `WIFI_SSID` / `WIFI_PASSWORD` 改成真实值
2. PC 端 `ipconfig` 查真实 WiFi IP（不要选 VMware 虚拟网卡）
3. 网络调试助手本地 IP 选 `0.0.0.0` 或 PC 真实 WiFi IP，点启动按钮
4. PC 防火墙关闭（Public network）

## 参考文档

| 文件 | 内容 |
|------|------|
| `ahd20/README.md` | 固件工程概览、引脚表、避坑要点 |
| `ahd20/DEBUG_SUMMARY.md` | 完整调试踩坑记录（3 个核心问题 + 修复） |
| `ahd20/ESP8266集成笔记.md` | ESP8266 集成全过程、AT 指令调试流程 |
| `ahd20/踩坑记录.md` | AHT20 + LCD 移植踩坑（引脚冲突、编码问题） |
| `ahd20/board_pinout.md` | 完整引脚占用表（按用途分类） |
| `OneNET_改动说明.md` | OneNET 集成改动清单、鉴权方案、常见问题 |

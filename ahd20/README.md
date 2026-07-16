# ahd20 工程

STM32F103RCT6 HAL 基础工程 —— 按键测试版（2026-07-14 跑通）

---

## 板子

**苏宙嵌孝教育 STM32-F103RCT6 V5.4.1**

| 资源 | 说明 |
|---|---|
| MCU | STM32F103RCT6（256K Flash / 48K RAM / LQFP64） |
| 时钟 | HSI 8MHz（无外部晶振） |
| 调试器 | 板载 CMSIS-DAP（PA13/PA14） |
| USB-TTL | 板载 CH340（COM13 / 115200） |
| 供电 | CMSIS-DAP USB 接口（5V） |

---

## 引脚表（速查）

| 功能 | 引脚 | 触发逻辑 | 备注 |
|---|---|---|---|
| 蓝灯（左） | PB2 | 低电平点亮 | |
| 红灯（右） | PC5 | 低电平点亮 | |
| KEY_1 | PA0 | **按下高电平** | Wakeup 键 |
| KEY_2 | PA1 | **按下低电平** | |
| KEY_3 | PA4 | **按下低电平** | |
| USART1 | PA9/PA10 | — | → CH340 → COM13 |
| I2C1 | PB6/PB7 | — | AHT20（10K 上拉） |
| SWD | PA13/PA14 | — | 板载 DAP-Link |

**详细定义见 `Inc/pins.h`**

---

## ⚠ 关键避坑（不要动）

### 1. `Core/Src/stm32f1xx_hal_msp.c`

```c
__HAL_AFIO_REMAP_SWJ_NOJTAG();   // ✅ 保留 SWD，关 JTAG
// __HAL_AFIO_REMAP_SWJ_DISABLE();  // ❌ 永久失联 DAP-Link
```

### 2. Keil → Options → Target

- ✅ **勾选 `Use MicroLIB`**（不勾会卡 semihosting）

### 3. Keil → Options → Debug → Settings

- Connect: `under Reset`
- Reset: `Hardware`

---

## 当前功能（按键测试版）

| 操作 | 现象 |
|---|---|
| 上电 | 串口打印 banner（COM13 / 115200） |
| 每 1s | 串口打印心跳 + 当前按键/LED 状态 |
| 按 KEY_1 | 串口打印 `[KEY_1] PA0 PRESSED at tick=...` |
| 按 KEY_2 | 蓝灯 PB2 **常亮**（松手灭） |
| 按 KEY_3 | 红灯 PC5 **常亮**（松手灭） |

---

## 文件结构

```
ahd20/
├── Core/
│   ├── Inc/                # 头文件
│   │   ├── pins.h          # ✨ 引脚集中定义（新加）
│   │   ├── main.h
│   │   ├── stm32f1xx_hal_conf.h
│   │   └── ...
│   └── Src/
│       ├── main.c          # 按键测试版主程序
│       ├── stm32f1xx_hal_msp.c   # ⚠ 含 NOJTAG 修复
│       └── ...
├── Drivers/
│   └── BSP/
│       └── AHT20/          # 温湿度驱动（暂未启用）
├── MDK-ARM/                # Keil 工程
├── board_pinout.md         # 板子引脚对照表
├── DEBUG_SUMMARY.md        # 调试经验总结
└── README.md               # 本文件
```

---

## 调试经验

完整踩坑记录见 `DEBUG_SUMMARY.md`（3 个核心问题 + 修复方案）

---

## 后续 TODO

- [ ] 接 AHT20 传感器（PB6/PB7），调 `MX_I2C1_Init` 里的 `HAL_I2C_Init`
- [ ] OLED 显示屏（如有）
- [ ] 串口交互菜单
- [ ] 装 git 提交版本管理

# OneNET MQTT 数据上报问题修复

> 日期：2026-07-16

## 问题现象

MQTT 连接成功（CONNACK return_code=0），但：
- 只输出一次 AHT20 数据后程序卡死
- OneNET 平台无数据更新
- 重连时 ESP8266 完全无响应

## 根因

###1. `>` 提示符未被识别

ESP8266 的 `AT+CIPSEND` 命令返回 `OK\r\n> `，其中 `>` 不以 `\n` 结尾。原来的行解析器按 `\n` 分割，永远等不到完整的 `>` 行。

###2. 栈溢出

`OneNET_PublishDataPoint` 函数有 864 字节的局部变量（topic[96]+payload[256]+pkt[512]），加上调用栈帧，超过 STM32 默认 1KB 栈限制。

###3. OK 检查冲突

通用的 `OK` 检查会在 `cmd_expect=">"` 时提前触发 `cmd_done`，导致 `send_at_cmd` 提前返回。

## 修复方案

1. `ESP8266_SendData`：直接在字节流中找 `>` 字符
2. `parse_char`：通用 OK 检查只在没有设置 `cmd_expect` 时生效
3. `main.c`：publish 逻辑内联，所有 buffer 用 `static`
4. `ESP8266_Reset`：AT 无响应时硬件复位（RST 引脚）

## 详细调试记录

见 `ahd20/DEBUG_SUMMARY.md`

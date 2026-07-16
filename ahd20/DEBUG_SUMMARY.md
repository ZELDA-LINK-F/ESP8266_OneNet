# OneNET MQTT 数据上报调试记录

> 日期：2026-07-16
> 状态：已修复

---

## 一、问题现象

MQTT 连接成功（CONNACK return_code=0），但：
1. 只输出一次 AHT20 数据后程序卡死
2. OneNET 平台无数据更新
3. 重连时 ESP8266 完全无响应

---

## 二、根因分析

### 问题1：`>` 提示符未被正确识别

**现象**：`AT+CIPSEND=141` 发送后，ESP8266 返回 `OK\r\n> `，但程序卡在等待 `>`。

**根因**：`send_at_cmd` 使用行解析器（按 `\n` 分割），但 ESP8266 的 `>` 提示符**不以 `\n` 结尾**，行解析器永远等不到完整的 `>` 行。

**修复**：`ESP8266_SendData` 中直接在字节流中查找 `>` 字符，不依赖行解析器。

### 问题2：通用 OK 检查覆盖 cmd_expect

**现象**：当 `expect=">"` 时，收到 `OK` 也触发了 `cmd_complete(1)`。

**根因**：`parse_char` 中的通用 `OK` 检查（`strstr(line_buf, "OK")`）会在 `cmd_expect` 匹配之前执行，导致 `send_at_cmd` 提前返回。

**修复**：通用 `OK` 检查只在 `cmd_expect[0] == '\0'` 时生效。

### 问题3：栈溢出导致函数调用卡死

**现象**：`main.c` 调用 `OneNET_PublishDataPoint` 时卡死，函数内的 `printf` 没有输出。

**根因**：`OneNET_PublishDataPoint` 中有 `topic[96] + payload[256] + pkt[512] = 864` 字节的局部变量，加上 `main.c` 的调用栈帧，总栈深度超过 STM32 默认的 1KB 栈限制。

**修复**：
1. 将 publish 逻辑内联到 `main.c`，避免跨文件调用的栈帧叠加
2. 所有 buffer 改为 `static`，不占栈空间

### 问题4：ESP8266 异常状态无法恢复

**现象**：重连时 ESP8266 完全无响应（`rx_head=0, rx_tail=0`）。

**根因**：ESP8266 在异常通信后进入死锁状态，AT 命令无响应。

**修复**：`ESP8266_Reset` 中添加硬件复位逻辑（RST 引脚拉低 100ms）。

---

## 三、改动文件清单

| 文件 | 改动 |
|------|------|
| `Drivers/BSP/ESP8266/esp8266.c` | `ESP8266_SendData`: 直接查找 `>` 字符；`ESP8266_Reset`: 添加硬件复位；`parse_char`: 修复通用 OK 检查 |
| `Drivers/BSP/ESP8266/onenet_mqtt.c` | `OneNET_PublishDataPoint`: 使用 static buffer |
| `Core/Src/main.c` | publish 逻辑内联，避免栈溢出；添加 `mqtt_raw.h` include |

---

## 四、关键修复代码

### 1. ESP8266_SendData - 直接查找 `>` 字符

```c
/* 等 '>' 提示符（直接在字节流中查找，不用行解析器） */
uint32_t start = HAL_GetTick();
int got_prompt = 0;
while (HAL_GetTick() - start < 5000) {
    uint8_t ch;
    while (ring_get(&ch)) {
        parse_char(ch);
        if (ch == '>') {
            got_prompt = 1;
        }
    }
    if (got_prompt) break;
    HAL_Delay(1);
}
```

### 2. parse_char - 通用 OK 检查条件

```c
/* 通用 OK/ERROR 检查：仅当没有设置特定 expect 时才生效 */
if (cmd_done == 0 && cmd_expect[0] == '\0') {
    if (strstr(line_buf, "OK") == line_buf ||
        strstr(line_buf, "SEND OK") == line_buf) {
        cmd_complete(1);
    }
}
```

### 3. main.c - 内联 publish 逻辑

```c
/* 所有 buffer 都用 static，不占栈 */
static char topic[64];
static char payload[128];
static uint8_t pkt[384];

snprintf(topic, sizeof(topic), "$sys/%s/%s/dp/post/json",
         ONENET_PRODUCT_ID, ONENET_DEVICE_NAME);

int plen = snprintf(payload, sizeof(payload),
    "{\"id\":%d,\"dp\":{\"%s\":[{\"v\":%.1f}],\"%s\":[{\"v\":%.1f}]}}",
    msg_id, ONENET_FIELD_TEMP, g_temp, ONENET_FIELD_HUMI, g_humi);

int pkt_len = MQTT_BuildPublish(pkt, sizeof(pkt), topic,
                                 (const uint8_t *)payload, (uint16_t)plen);
if (pkt_len > 0 && ESP8266_SendData(pkt, (uint16_t)pkt_len) == ESP8266_OK)
{
    printf("[SYS] publish OK, id=%d\r\n", msg_id);
    msg_id++;
}
```

### 4. ESP8266_Reset - 硬件复位

```c
/* AT 无响应，尝试硬件复位（RST 拉低 100ms 再拉高） */
ESP_RST_LOW();
HAL_Delay(100);
ESP_RST_HIGH();
HAL_Delay(1000);

/* 清空复位后 ESP8266 可能吐出的垃圾数据 */
ring_clear();
pstate = PARSE_IDLE;
```

---

## 五、经验总结

1. **ESP8266 的 `>` 提示符不以 `\n` 结尾**：不能用行解析器处理，必须直接在字节流中查找。

2. **STM32 默认栈很小（1KB）**：大 buffer 必须用 `static`，避免栈溢出。

3. **AT 命令的响应顺序不确定**：`OK` 可能在 `>` 之前或之后到达，解析逻辑需要考虑各种情况。

4. **ESP8266 可能进入死锁状态**：需要硬件复位（RST 引脚）才能恢复。

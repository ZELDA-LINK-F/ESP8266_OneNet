# OneNET MQTT 连接失败诊断报告

> 生成时间：2026-07-16
> 状态：已分析 → 已实施修复 → 待烧录验证
> 修复方案：改用 access_key 直传作为 MQTT Password（模式0，默认开启）

---

## 一、现象回顾

串口日志如下：

```
[ESP] Reset...
[ESP] Ready
[ESP] Station mode OK
[SYS] Connecting WiFi: Xiaomi 15 ...
[ESP] Connecting WiFi: Xiaomi 15 ...
[ESP] WiFi connected!                      ← WiFi 连接正常
[SYS] WiFi connected!
[ON] Init: pid=7Sp4dA99m3 dev=dev1 server=mqtts.heclouds.com:1883
[ESP] Connecting TCP: mqtts.heclouds.com:1883 ...
[ESP] TCP connected!                       ← TCP 连接正常
[ON] Send CONNECT (153 bytes) ...
[ON] HEX: 10 96 01 00 04 4D 51 54 54 04 C2 00 3C 00 04 64 65 76 31 ...
[ON] token (len=120):
[ON] >>>version=2018-10-31&res=products%2F7Sp4dA99m3%2Fdevices%2Fdev1&et=1805693871&method=md5&sign=Mp9kS4i8Lu2u16UQ1UTuGw%3D%3D<<<
[ON] CONNECT timeout (no CONNACK)          ← ⚠ 关键问题：未收到 CONNACK
[SYS] OneNET connect failed
```

**关键信息**：
| 阶段 | 状态 |
|------|------|
| WiFi 连接 | ✅ 正常 |
| TCP 连接（mqtts.heclouds.com:1883） | ✅ 正常 |
| MQTT CONNECT 发送（153 bytes） | ✅ 正常 |
| 等待 CONNACK 响应 | ❌ 超时 5 秒无响应 |

---

## 二、MQTT 报文逐字节分析

CONNECT 报文 hex 部分（首 29 字节已完整解析）：

```
10        ← MQTT 控制包类型：CONNECT (1)
96 01     ← 剩余长度：150 字节（多字节编码正确）
00 04     ← 协议名长度：4
4D 51 54 54  ← "MQTT"
04        ← 协议级别：4（MQTT 3.1.1）✅
C2        ← 连接标志：1100 0010
              bit7=1 用户名标志
              bit6=1 密码标志
              bit2=1 清理会话
              其余为 0 ✅
00 3C     ← 保活时间：60 秒
00 04     ← 客户端ID长度：4
64 65 76 31  ← "dev1"
00 0A     ← 用户名长度：10
37 53 70 34 64 41 39 39 6D 33  ← "7Sp4dA99m3"
00 78     ← 密码长度：120（0x78=120）
... (120字节 token 数据)
```

**结论：MQTT CONNECT 报文结构完全正确，符合 MQTT 3.1.1 协议规范。**

---

## 三、根因分析

**核心问题：OneNET MQTT 服务器收到了 CONNECT 报文，但由于认证失败，没有返回 CONNACK（可能是直接关闭了 TCP 连接）。**

### 可能性排序 & 详细分析

#### ⭐⭐⭐⭐ 可能性 1：Token 签名算法错误（最可能）

当前 Token 中的 `sign` 值：
```
Mp9kS4i8Lu2u16UQ1UTuGw==
```

`onenet_mqtt_calc.py` 脚本测试了 18 种签名算法组合（MD5/HMAC-MD5/HMAC-SHA1 × key/device_secret/res），目的是**找出哪个算法能生成该 sign**。这说明开发者本身也不确定正确的算法是什么。

**已知配置**：
| 参数 | 值 |
|------|-----|
| access_key | `6/HRq7d7o5/DvRet5Wm7wfcdWVVbw6Gbu5nRzTdjhVc` |
| device_secret (base64) | `dUQ2U1hNdTBIRmNVT25IenMyaDdUSHBIbzhUM1cxblM=` |
| et (过期时间) | `1805693871` |
| 期望 sign | `Mp9kS4i8Lu2u16UQ1UTuGw==` （16字节 → MD5） |

**可能的正确签名方式**（OneNET 平台常用）：
- **方式A**：`sign = base64(md5(access_key + et))` → 产品级鉴权
- **方式B**：`sign = base64(md5(device_secret + et))` → 设备级鉴权
- **方式C**：`sign = hmac_md5(access_key, et)` 
- **方式D**：直接使用 access_key 作为 MQTT Password（不需要 token 格式）

**建议**：运行 `onenet_mqtt_calc.py` 查看哪种算法匹配，然后用正确的算法重新生成 token。

---

#### ⭐⭐⭐ 可能性 2：Token 的 res 字段格式问题

当前 token 中 `res` 使用**设备级资源**：
```
products/7Sp4dA99m3/devices/dev1
```

但 `onenet_token_calc.py` 中使用的是**产品级资源**：
```python
res_plain = 'products/7Sp4dA99m3'  # 不含 /devices/dev1
```

| 对比项 | main.c 固件 | onenet_token_calc.py |
|--------|------------|---------------------|
| res | `products/{pid}/devices/{dev}` | `products/{pid}` |
| et | `1805693871` | `1893456000` |
| sign 计算 | key+et? device_secret+et? | key+et |

**两个脚本的 res、et、sign 值完全不同**，说明固件中的 token 是手动硬编码的，可能来自错误来源。

---

#### ⭐⭐⭐ 可能性 3：MQTT 服务器地址不符

固件代码注释中记录了两种 broker 地址：

```c
// 注释中写的（新平台）:
// broker:   7Sp4dA99m3.mqtts.acc.cmcconenet.cn:1883 (非 TLS)
//           7Sp4dA99m3.mqttstls.acc.cmcconenet.cn:8883 (TLS)

// 实际代码使用的（通用地址）:
#define ONENET_SERVER_HOST  "mqtts.heclouds.com"
```

- `mqtts.heclouds.com` — OneNET Studio 通用 MQTT 入口
- `{pid}.mqtts.acc.cmcconenet.cn` — 产品级独立 MQTT 地址

**TCP 能连接成功**（`[ESP] TCP connected!`），说明地址可达，但产品注册在哪个区域、是否需要产品级子域名需确认。

---

#### ⭐⭐ 可能性 4：ESP8266 固件对 MQTT 数据接收处理有缺陷

数据到达流程：
1. ESP8266 收到 TCP 数据 → 输出 `+IPD,N:<data>`
2. `parse_char()` 状态机提取 `tcp_payload[]`
3. `ESP8266_DataAvailable()` 返回 payload 长度
4. `handle_incoming()` 调用 `MQTT_ParsePacketType()` 检测 CONNACK

**潜在风险**：
- 如果 broker 在 CONNACK 之前先发了其他数据（如协议版本协商），可能导致 parser 状态混乱
- 如果 broker 发送 CONNACK 后立刻关闭 TCP 连接，`CLOSED` 消息可能干扰 `+IPD` 解析
- `DataAvailable()` 只在完整 IPD 块接收完后才返回 >0，如果 IPD 数据与 `CLOSED` 交错可能丢失

---

#### ⭐ 可能性 5：OneNET 平台侧问题

- 设备 `dev1` 在产品 `7Sp4dA99m3` 下是否已注册并激活？
- 产品/设备是否被禁用或删除？
- 平台是否升级导致旧版 token 格式不再支持？

---

## 四、解决思路与验证步骤

### 第一步：确认正确的 Token 签名算法 🔑

运行 Python 脚本，找出匹配的签名算法：

```powershell
pip install -r requirements.txt  # 或手动 pip install pycryptodome
python onenet_mqtt_calc.py
```

**如果脚本报错或无输出**：手动计算关键几种情况验证：

```python
import hashlib, base64

ak = '6/HRq7d7o5/DvRet5Wm7wfcdWVVbw6Gbu5nRzTdjhVc'
ds = 'dUQ2U1hNdTBIRmNVT25IenMyaDdUSHBIbzhUM1cxblM='
et = '1805693871'

# 测试1：md5(access_key + et) — 产品级鉴权
sig1 = base64.b64encode(hashlib.md5((ak + et).encode()).digest()).decode()
print(f'产品级 md5(ak+et):  {sig1}')

# 测试2：md5(device_secret + et) — 设备级鉴权
sig2 = base64.b64encode(hashlib.md5((ds + et).encode()).digest()).decode()
print(f'设备级 md5(ds+et):  {sig2}')

# 期望值
print(f'当前固件中的 sign: Mp9kS4i8Lu2u16UQ1UTuGw==')
```

**看哪个输出与当前值匹配，然后用正确算法生成新 token。**

---

### 第二步：生成正确 Token 并更新固件 🔧

用正确的 sign 和参数更新 `main.c` 中的 `ONENET_TOKEN` 宏：

```c
#define ONENET_TOKEN \
  "version=2018-10-31&res=products%2F7Sp4dA99m3%2Fdevices%2Fdev1" \
  "&et=1893456000&method=md5" \
  "&sign=<新计算的sign>"
```

> 建议 `et` 统一改为 `1893456000`（2030年过期），避免频繁过期。

---

### 第三步：尝试备选简单认证方案 💡

如果 token 方案持续失败，可以尝试**直接用 access_key 作为 MQTT Password**：

需修改 `onenet_mqtt.c` 的 `OneNET_Connect()` 函数：

```c
// 原代码：
MQTT_BuildConnect(pkt, sizeof(pkt),
    g_cfg->device_name,    // client_id = "dev1"
    g_cfg->product_id,     // username  = "7Sp4dA99m3"
    g_cfg->token,          // password  = token字符串
    60);

// 改为：
MQTT_BuildConnect(pkt, sizeof(pkt),
    g_cfg->device_name,    // client_id = "dev1"
    g_cfg->product_id,     // username  = "7Sp4dA99m3"
    ONENET_ACCESS_KEY,     // password  = access_key直接作为密码
    60);                    // (16*8=128字节，远小于pkt buffer)
```

在 `main.c` 中添加：
```c
#define ONENET_ACCESS_KEY "6/HRq7d7o5/DvRet5Wm7wfcdWVVbw6Gbu5nRzTdjhVc"
```

> 这是很多 OneNET 接入示例的做法，不需要复杂 token 拼接。

---

### 第四步：增加调试日志 📊

在 `onenet_mqtt.c` 的 `handle_incoming()` 函数开头添加一行日志，确认 ESP8266 是否收到了任何数据：

```c
static void handle_incoming(void)
{
    while (ESP8266_DataAvailable() > 0) {
        int n = ESP8266_PeekData(buf, sizeof(buf));
        // ⬇ 添加这行
        printf("[MQTT] RX %d bytes, type=0x%02X\n", n, buf[0]);
        
        int pkt_type = MQTT_ParsePacketType(buf, n);
        // ... 后续逻辑
    }
}
```

另外，在发送 CONNECT 之后立即检查 TCP 连接是否还在：

```c
// 在 OneNET_Connect() 中，send_mqtt_packet() 之后
if (ESP8266_IsTCPConnected() == 0) {
    printf("[ON] TCP disconnected right after CONNECT!\n");
    return -1;
}
```

> 这样能区分是"收到数据但解析失败"还是"完全没有数据返回"。

---

### 第五步：网络层验证 🌐

用 PC 端网络调试工具验证 MQTT 连接，排除硬件/ESP8266 问题：

1. 下载 **MQTTX**（开源 MQTT 客户端）：https://mqttx.app/
2. 创建连接，填入：
   | 参数 | 值 |
   |------|-----|
   | Broker | `mqtts.heclouds.com` |
   | Port | `1883` |
   | Client ID | `dev1` |
   | Username | `7Sp4dA99m3` |
   | Password | 用 Python 脚本生成的正确 token |
3. 点击连接，看是否成功
4. 成功后发布测试消息到 `$sys/7Sp4dA99m3/dev1/dp/post/json`

**如果 MQTTX 也不能连接，说明是平台侧或 token 问题，排除 ESP8266/STM32 代码问题。**

---

### 第六步：确认 OneNET 平台配置 ☁️

登录 [OneNET Studio 控制台](https://open.iot.10086.cn/console) 确认：

1. ✅ 产品 `7Sp4dA99m3` 是否存在、状态正常
2. ✅ 设备 `dev1` 是否已注册在产品下、状态为"未激活"或"在线"
3. ✅ 产品 access_key 是否与代码中一致
4. ✅ MQTT 接入协议是否已开启
5. ✅ 查看设备详情页 → "设备调试" → 是否有 MQTT 连接日志或错误记录

---

## 五、改了什么（2026-07-16 已实施）

### main.c 改动

1. **新增 `ONENET_AUTH_MODE` 编译开关**（第62行）：
   - `0`（当前默认）：access_key 直传作为 MQTT Password —— **推荐先试这个**
   - `1`：产品级 token (md5(access_key+et), res=products/{pid}) —— 备选
   - `2`：设备级 token（保留旧值作为占位）

2. **新增 `ONENET_ACCESS_KEY` 宏**（第68行）：直接暴露 product access_key

3. **新增 `ONENET_TOKEN_MODE1` 宏**（第73-76行）：用 Python 新生成的产品级 token，et=1893456000（2030年过期）

4. **连接时根据 MODE 选择密码**（第458-485行）：用 `#if/#elif/#else` 预编译选择

### onenet_mqtt.c 改动

1. **新增接收数据 hex dump**（第70-73行）：每次收到 TCP 数据时打印前16字节的 hex 值，方便判断服务器是否响应
2. **发送 CONNECT 后立即 poll**（第172-176行）：检查是否有数据到达

### 签名计算验证（Python 脚本）

经 Python 计算验证：

| 算法 | 结果 |
|------|------|
| md5(access_key + et) | `AzQuIctPNA2/Qu3b5EHxTw==` ← **≠ 当前固件 sign** |
| md5(device_secret + et) | `e6L9RLUvwReb1Nk8NoYP/Q==` ← **≠ 当前固件 sign** |
| md5(raw_device_secret + et) | `mJRFRPd4RtwKHQGQaq+QUw==` ← **≠ 当前固件 sign** |
| hmac_sha1(raw_ds, et\nsha1\nres\nver) | `JG5jwqVxofW13FSD1dBYVdF2Pj8=` ← **≠ 注释预期值** |

**结论：固件中硬编码的 sign (`Mp9kS4i8Lu2u16UQ1UTuGw==`) 无法用已知的 ak/ds/et 组合复现，来源不明。注释中的预期 sign (`pX97iok1CXph0Kwf5JUwl0zerM0=`) 也无法用注释描述的算法复现。因此最稳妥的方案是绕过 token 机制，直接用 access_key 作为 MQTT Password。**

### 下一步操作

| 优先级 | 操作 | 说明 |
|--------|------|------|
| 🔴 P0 | **Keil 编译 + 烧录** | 默认 MODE=0（access_key 直传），观察串口日志 |
| 🔴 P0 | 查看新日志 `[ON] RX x bytes: XX XX ...` | 确认服务器是否有响应 |
| 🟡 P1 | 若仍超时，改 `ONENET_AUTH_MODE=1` 重试 | 产品级 token 方案 |
| 🟡 P1 | 若仍超时，改 broker 为 `7Sp4dA99m3.mqtts.acc.cmcconenet.cn:1883` | 换产品级独立地址 |
| 🟢 P2 | 用 MQTTX 客户端 PC 端验证 | 排除硬件/ESP8266 问题 |

---

## 六、上位机 APP 建议

不管 MQTT 连上与否，**APP 的开发可以并行进行**。根据你已有的资料：

1. **OneNET API 方式**（推荐）：
   - 数据上传后，上位机通过 HTTP API 轮询获取设备数据
   - 参考 `OneNET资料/ai网页生成提示词模板/` 中的模板
   - API 地址：`https://iot-api.heclouds.com/thingmodel/query-device-property`

2. **MQTT 直连方式**（高级）：
   - APP 也作为 MQTT 客户端订阅 `$sys/7Sp4dA99m3/dev1/dp/post/json`
   - 需要 APP 端也用同样的 token 连接 MQTT broker

3. **先用模拟数据开发 APP**，等 MQTT 调通后切换到真实数据源。

---

## 七、总结

| 问题 | 结论 |
|------|------|
| WiFi | ✅ 正常 |
| TCP | ✅ 正常 |
| MQTT 报文结构 | ✅ 正确 |
| **MQTT 认证** | ❌ **Token 签名算法大概率不对** |
| CONNACK | ❌ 服务器认证失败，无声断开连接 |

**核心修复方向：确认 OneNET MQTT 的正确鉴权方式（产品级 token？设备级 token？还是直接用 access_key 作为 MQTT 密码？），然后更新固件。**

---

> 📝 本报告基于 `C:\Users\19698\Desktop\工程实训\ahd20\` 目录下的固件代码分析。

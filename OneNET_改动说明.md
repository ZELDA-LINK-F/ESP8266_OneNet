# OneNET 集成改动说明

> 板子端 ESP8266 → OneNET Studio（新版）MQTT 接入
> 改完了，待你 build + 烧录验证

## 改动文件清单

| 文件 | 状态 | 改动 |
|------|------|------|
| `Drivers/BSP/ESP8266/esp8266.h` | 改 | +6 个 MQTT 函数声明 |
| `Drivers/BSP/ESP8266/esp8266.c` | 改 | +MQTT 下行环形缓冲（解析 `+MQTTSUBRECV`）+ `AT+MQTT*` 指令封装 + `AT+MQTTPUBRAW`（用 raw 模式发 JSON 避免双引号问题） |
| `Drivers/BSP/ESP8266/onenet_mqtt.h` | 新建 | OneNET 客户端接口（产品级鉴权） |
| `Drivers/BSP/ESP8266/onenet_mqtt.c` | 新建 | OneNET 客户端实现（topic 拼接 + JSON 构造 + Publish） |
| `Core/Src/main.c` | 改 | include 替换 aliyun_iot→onenet_mqtt；用 OneJSON 上报；字段名 temperature/humidity |
| `MDK-ARM/ahd20.uvprojx` | 改 | 删 aliyun_iot.c/.h，加 onenet_mqtt.c/.h |
| `Drivers/BSP/ESP8266/aliyun_iot.c/.h` | 保留 | 旧文件（不删，万一新方案不好用能快速回退） |

## 关键技术点

### 1. 鉴权（产品级）

不需要设备密钥，用产品 access_key 算 token。已算好：

```
res   = products/7Sp4dA99m3
key   = 6/HRq7d7o5/DvRet5Wm7wfcdWVVbw6Gbu5nRzTdjhVc  (产品 access_key)
et    = 1893456000  (2030-01-01 过期)
method= md5
sign  = base64(md5(key + et))
token = version=2018-10-31&res=products%2F7Sp4dA99m3&et=1893456000&method=md5&sign=M%2B7kHpSX4CLFxgiJeS3BCw%3D%3D
```

⚠ **设备密钥被截断了**（`dUQ2U1hNdTBiRmNVT25I...`），所以用产品 access_key 鉴权更稳。
Token 在 `main.c` 顶部 `ONENET_TOKEN` 宏里直接写死，要改过期时间重新算：

```python
import hashlib, base64, urllib.parse
key = '6/HRq7d7o5/DvRet5Wm7wfcdWVVbw6Gbu5nRzTdjhVc'
et  = 1893456000
sig = base64.b64encode(hashlib.md5((key + str(et)).encode()).digest()).decode()
print(f'version=2018-10-31&res=products%2F7Sp4dA99m3&et={et}&method=md5&sign={urllib.parse.quote(sig)}')
```

### 2. ESP8266 AT 指令选型

用 `AT+MQTTPUBRAW`（不是 `AT+MQTTPUB`）发 JSON payload：

```
AT+MQTTPUBRAW=0,"$sys/7Sp4dA99m3/dev1/dp/post/json",86,0,0
> （ESP8266 吐 > 后）
{"id":1,"dp":{"temperature":[{"v":25.5}],"humidity":[{"v":60.0}]}}  ← raw 字节，不引号包裹
OK
```

**为什么用 RAW**：JSON 里的 `"` 在 `AT+MQTTPUB` 的字符串参数里会被 AT 解析器搞乱。RAW 模式直接发字节，干净。

### 3. OneJSON 数据流格式

用的是 OneNET Studio 的"数据流模板"模式（不是严格物模型），格式：

```json
{
  "id": 1,
  "dp": {
    "temperature": [{"v": 25.5}],
    "humidity":    [{"v": 60.0}]
  }
}
```

字段名是 `dp` 不是 `params`，值用 `v` 包裹在数组里。

### 4. 主题

上行（板子→云）：`$sys/7Sp4dA99m3/dev1/dp/post/json`
下行（云→板子）：暂不订阅

## 你需要做的（烧录前）

### 1. 字段名跟 OneNET 控台对（无需改控台）

**OneNET 数据流模板的 identifier 创建后不可改**！所以**保持 SHT30_T / SHT30_H 不动**，板子端代码用宏跟随平台字段名（见 `main.c` 顶部 `ONENET_FIELD_TEMP` / `ONENET_FIELD_HUMI`）。

- 当前平台字段是 `SHT30_T`（温度）/`SHT30_H`（湿度）
- STM32 板子上报也用这两个名字，控台就能收数据
- 字段名只是 identifier，对功能没影响

> ⚠ 想换成 `temperature`/`humidity`？只能在 OneNET 控台**删 SHT30_T/SHT30_H 重建**，会丢历史数据。**不建议**——名字是次要的。

### 2. 烧录验证流程

1. **Keil 编译**（F7）—— 期望 0 errors。如果有错，把错误贴给我
2. **烧录**（F8）—— 烧完按一下 RESET
3. **打开串口**（115200 / COM13）看日志
4. **预期日志**：
   ```
   [ESP] Reset...
   [ESP] Ready
   [ESP] Station mode OK
   [ESP] Connecting WiFi: Xiaomi 15 ...
   [ESP] WiFi connected!
   [SYS] WiFi connected!
   [MQTT] UserCfg client=dev1 user=7Sp4dA99m3 pwd_len=180
   [MQTT] Connect mqtts.heclouds.com:1883 keepalive=60 ...
   [MQTT] Connected!
   [ON] OneNET connected!
   [SYS] OneNET connected!
   
   [AHT20] T=25.5 C  H=60.0 %RH
   [ON] pub: {"id":1,"dp":{"temperature":[{"v":25.5}],"humidity":[{"v":60.0}]}}
   ...
   ```
5. **登录 OneNET 控台** → 设备管理 → 选 dev1 → 详情 → 设备状态应该变"在线" → 数据流 → 看到 temperature/humidity 跳数字
6. **前端可视化**（下一步）—— 控台 → 应用开发 → 数据可视化 → 拖图表绑字段 → 分享链接

## 常见问题（提前预判）

| 现象 | 原因 | 解决 |
|------|------|------|
| 编译报 `undefined reference to OneNET_xxx` | 工程树没加 onenet_mqtt.c | 改 .uvprojx 我已做，再 Rebuild 一下 |
| 编译报 `hi2c1 undefined` | .uvprojx 里残留 aht20.c | 检查工程树，删 aht20.c |
| 串口打印 `[MQTT] UserCfg` 之后无响应 | password 含特殊字符 | 当前 token 已验证不含 `,` `"` `\`，安全 |
| 串口打印 `[MQTT] Connect failed` | 网络/TLS 问题 | 确认 WiFi 已连；试 `AT+CIPSTART="TCP","mqtts.heclouds.com",1883` |
| OneNET 设备一直离线 | 字段名不匹配 / token 错 | 查字段名是否已改 `temperature/humidity`；token 是否对 |
| OneNET 设备在线但数据流空白 | topic 拼错 / JSON 格式错 | 抓串口看 `[ON] pub: ...` 行的 payload |

## 前端下一步（待定）

板子跑通后，做 OneNET 自带的"数据可视化" dashboard：
1. OneNET 控台 → 左侧栏 → **应用开发** / **数据可视化**
2. 新建项目 → 空白模板
3. 拖个"实时数据"组件 → 数据源选"物联网开放平台" → 设备选 dev1 → 字段选 temperature
4. 再拖一个绑 humidity
5. 保存 + 发布 → 分享链接给同学/老师

有需要我帮你生成仪表盘截图 / 帮你做 HTML 漂亮化再说。

---

## 改完没验证

⚠ 我这边没有 Keil 工具，没法 build 验证。代码是按你之前能 build 过的项目结构 + 标准 MQTT 协议写的，**但**：
- 没准 ESP8266 固件不支持 `AT+MQTTPUBRAW`（需要固件版本 ≥ 1.5.x）
- 没准 `+MQTTSUBRECV` 解析有边界 bug
- 没准 OneJSON 格式细节有偏差

**你 build 后有任何错误，截图发我**。最常见的小问题我可以秒改。

/**
  ******************************************************************************
  * @file    esp8266.c
  * @brief   ESP8266 WiFi 模块 AT 指令驱动
  *
  *   工作流程：
  *     1. 初始化 USART2（PA2=TX / PA3=RX）115200 8N1
  *     2. RST 引脚（PC4）复位模块
  *     3. AT 指令序列：AT → CWMODE=1 → CWJAP → CIPSTART
  *     4. 发送 TCP 数据：CIPSEND=N → > → 原始数据 → SEND OK
  *     5. 接收 TCP 数据：解析 +IPD,N:data
  *
  *   环形缓冲 + 状态机解析，无阻塞轮询。
  ******************************************************************************
  */

#include "esp8266.h"
#include <string.h>
#include <stdio.h>

/* ============================== 引脚 ===================================== */
/* ESP8266 RST: PC4, 低电平复位, 高电平运行 */
#define ESP_RST_PORT     GPIOC
#define ESP_RST_PIN      GPIO_PIN_4
#define ESP_RST_LOW()    HAL_GPIO_WritePin(ESP_RST_PORT, ESP_RST_PIN, GPIO_PIN_RESET)
#define ESP_RST_HIGH()   HAL_GPIO_WritePin(ESP_RST_PORT, ESP_RST_PIN, GPIO_PIN_SET)

/* =========================== 环形缓冲区 ================================== */
static UART_HandleTypeDef *esp_uart = NULL;

static uint8_t  rx_buf[ESP8266_RX_BUF_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

static void ring_put(uint8_t ch) {
    uint16_t next = (rx_head + 1) % ESP8266_RX_BUF_SIZE;
    if (next != rx_tail) {
        rx_buf[rx_head] = ch;
        rx_head = next;
    }
}

static int ring_get(uint8_t *ch) {
    if (rx_head == rx_tail) return 0;
    *ch = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % ESP8266_RX_BUF_SIZE;
    return 1;
}

static void ring_clear(void) {
    rx_head = 0;
    rx_tail = 0;
}

/* =========================== TCP 载荷缓冲 ================================= */
/* 仅存储 +IPD 提取出的 TCP 数据，不包含 AT 响应行 */
static uint8_t tcp_payload[2048];
static volatile uint16_t tcp_payload_len = 0;   /* 已缓冲的有效 TCP 数据长度 */

/* =========================== 解析状态机 ================================== */
typedef enum {
    PARSE_IDLE,
    PARSE_LINE,           /* 正在接收一行 */
    PARSE_IPD_LEN,        /* 看到 "+IPD,"，正在解析长度 */
    PARSE_IPD_DATA,       /* 正在读取 +IPD 数据 */
} parse_state_t;

static parse_state_t pstate = PARSE_IDLE;
static char     line_buf[256];
static uint16_t line_idx = 0;
static uint16_t ipd_datalen = 0;
static uint16_t ipd_cnt = 0;

/* AT 指令等待期望响应 */
static volatile int cmd_done = 0;
static volatile int cmd_result = 0;   /* 0=OK, -1=ERROR/TIMEOUT */
static char    cmd_expect[64];

static void cmd_complete(int ok) {
    cmd_done = 1;
    cmd_result = ok ? 0 : -1;
}

/* =========================== 底层收发 ==================================== */

/* 发送原始字符串（不含 \r\n，由上层添加） */
static void uart_send(const char *str) {
    if (esp_uart == NULL) return;
    HAL_UART_Transmit(esp_uart, (uint8_t *)str, (uint16_t)strlen(str), 1000);
}

static void uart_send_bytes(const uint8_t *data, uint16_t len) {
    if (esp_uart == NULL) return;
    HAL_UART_Transmit(esp_uart, (uint8_t *)data, len, 5000);
}

/* =========================== 初始化 ====================================== */
void ESP8266_Init(UART_HandleTypeDef *huart)
{
    esp_uart = huart;

    /* 配置 RST 引脚 */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = ESP_RST_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(ESP_RST_PORT, &gpio);
    ESP_RST_HIGH();

    /* 清空缓冲区 */
    ring_clear();
    tcp_payload_len = 0;
    pstate = PARSE_IDLE;
    line_idx = 0;
}

/* =========================== 中断处理 ==================================== */
void ESP8266_UART_IRQHandler(uint8_t byte)
{
    /* 只写入环形缓冲区，行解析时再分离 TCP 载荷 */
    ring_put(byte);
}

/* =========================== 行解析核心 ================================== */
static void parse_char(uint8_t ch)
{
    switch (pstate) {
    case PARSE_IDLE:
        if (ch == '\n' || ch == '\r') break;
        line_idx = 0;
        if (line_idx < sizeof(line_buf) - 1)
            line_buf[line_idx++] = (char)ch;
        pstate = PARSE_LINE;
        break;

    case PARSE_LINE:
        if (ch == '\r') break;
        if (ch == '\n') {
            line_buf[line_idx] = '\0';
            if (line_idx == 0) {
                pstate = PARSE_IDLE;
            } else if (strstr(line_buf, "+IPD,") == line_buf) {
                int n = 0;
                if (sscanf(line_buf, "+IPD,%d:", &n) == 1 && n > 0) {
                    ipd_datalen = (uint16_t)n;
                    ipd_cnt = 0;
                    pstate = PARSE_IPD_DATA;
                } else {
                    pstate = PARSE_IDLE;
                }
            } else {
                /* 打印所有非 IPD 的响应行，方便排查 */
                printf("[ESP] LINE: %s\r\n", line_buf);
                /* 检查期望响应 */
                if (cmd_done == 0 && cmd_expect[0] != '\0') {
                    if (strstr(line_buf, cmd_expect)) {
                        cmd_complete(1);
                    }
                }
                /* 通用 OK/ERROR 检查：仅当没有设置特定 expect 时才生效 */
                if (cmd_done == 0 && cmd_expect[0] == '\0') {
                    if (strstr(line_buf, "OK") == line_buf ||
                        strstr(line_buf, "SEND OK") == line_buf) {
                        cmd_complete(1);
                    }
                }
                if (cmd_done == 0) {
                    if (strstr(line_buf, "ERROR") ||
                        strstr(line_buf, "FAIL") ||
                        strstr(line_buf, "CLOSED")) {
                        cmd_complete(0);
                    }
                }
                pstate = PARSE_IDLE;
            }
        } else {
            if (line_idx < sizeof(line_buf) - 1)
                line_buf[line_idx++] = (char)ch;

            /* 检测 +IPD,N: 格式（数据紧跟在 : 后面，不等 \n） */
            if (ch == ':' && line_idx >= 6 && line_buf[0] == '+' &&
                line_buf[1] == 'I' && line_buf[2] == 'P' &&
                line_buf[3] == 'D' && line_buf[4] == ',') {
                line_buf[line_idx] = '\0';
                int n = 0;
                if (sscanf(line_buf, "+IPD,%d:", &n) == 1 && n > 0) {
                    ipd_datalen = (uint16_t)n;
                    ipd_cnt = 0;
                    pstate = PARSE_IPD_DATA;
                    line_idx = 0;
                    break;
                }
            }
        }
        break;

    case PARSE_IPD_DATA:
        /* 将 TCP 载荷字节写入独立缓冲区 */
        if (tcp_payload_len < sizeof(tcp_payload)) {
            tcp_payload[tcp_payload_len++] = ch;
        }
        ipd_cnt++;
        if (ipd_cnt >= ipd_datalen) {
            pstate = PARSE_IDLE;
        }
        break;

    default:
        pstate = PARSE_IDLE;
        break;
    }
}

/* =========================== 主循环轮询 ================================== */
void ESP8266_Poll(void)
{
    uint8_t ch;
    while (ring_get(&ch)) {
        parse_char(ch);
    }
}

/* ---------- TCP 载荷读取 ---------- */
int ESP8266_DataAvailable(void)
{
    return (int)tcp_payload_len;
}

int ESP8266_PeekData(uint8_t *buf, uint16_t max_len)
{
    uint16_t len = tcp_payload_len;
    if (len > max_len) len = max_len;
    if (len > 0) memcpy(buf, tcp_payload, len);
    return (int)len;
}

void ESP8266_ConsumeData(uint16_t len)
{
    if (len >= tcp_payload_len) {
        tcp_payload_len = 0;
    } else {
        uint16_t remaining = tcp_payload_len - len;
        memmove(tcp_payload, tcp_payload + len, remaining);
        tcp_payload_len = remaining;
    }
}

/* =========================== AT 指令发送 ================================= */

/**
 * @brief  发送 AT 指令并等待期望响应
 * @param  cmd      指令字符串（不含 \r\n）
 * @param  expect   期望响应关键字（NULL 则只等 OK/ERROR）
 * @param  timeout  超时毫秒
 * @return ESP8266_OK / ERROR / TIMEOUT
 */
static ESP8266_Status send_at_cmd(const char *cmd, const char *expect, uint32_t timeout)
{
    if (esp_uart == NULL) return ESP8266_ERROR;

    /* 清空环形缓冲 */
    ring_clear();

    /* 设置期望 */
    cmd_done = 0;
    cmd_result = 0;
    if (expect) {
        strncpy(cmd_expect, expect, sizeof(cmd_expect) - 1);
        cmd_expect[sizeof(cmd_expect) - 1] = '\0';
    } else {
        cmd_expect[0] = '\0';
    }

    /* 发送指令 */
    uart_send(cmd);
    uart_send("\r\n");

    /* 等待响应 */
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < timeout) {
        ESP8266_Poll();
        if (cmd_done) {
            if (cmd_result == 0) return ESP8266_OK;
            return ESP8266_ERROR;
        }
        HAL_Delay(5);
    }

    return ESP8266_TIMEOUT;
}

/* =========================== 公开接口 ==================================== */

ESP8266_Status ESP8266_Reset(void)
{
    printf("[ESP] Reset...\r\n");

    /* 先测试 AT 响应 */
    ESP8266_Status s = send_at_cmd("AT", "OK", 2000);
    if (s == ESP8266_OK) {
        printf("[ESP] Ready\r\n");
        goto config;
    }

    /* AT 无响应，尝试硬件复位（RST 拉低 100ms 再拉高） */
    printf("[ESP] AT no response, trying HW reset...\r\n");
    ESP_RST_LOW();
    HAL_Delay(100);
    ESP_RST_HIGH();
    HAL_Delay(1000);

    /* 清空复位后 ESP8266 可能吐出的垃圾数据 */
    ring_clear();
    pstate = PARSE_IDLE;

    s = send_at_cmd("AT", "OK", 3000);
    if (s != ESP8266_OK) {
        printf("[ESP] Reset failed (AT no response after HW reset)\r\n");
        return s;
    }
    printf("[ESP] Ready (after HW reset)\r\n");

config:
    /* 关闭回显 */
    send_at_cmd("ATE0", "OK", 1000);

    /* 设置为 Station 模式 */
    s = send_at_cmd("AT+CWMODE=1", "OK", 2000);
    if (s != ESP8266_OK) {
        printf("[ESP] CWMODE failed\r\n");
        return s;
    }
    printf("[ESP] Station mode OK\r\n");
    return ESP8266_OK;
}

ESP8266_Status ESP8266_ConnectWiFi(const char *ssid, const char *password)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"", ssid, password);
    printf("[ESP] Connecting WiFi: %s ...\r\n", ssid);

    ESP8266_Status s = send_at_cmd(cmd, "OK", ESP8266_CONN_TIMEOUT);
    if (s == ESP8266_OK) {
        printf("[ESP] WiFi connected!\r\n");
    } else {
        printf("[ESP] WiFi connect failed\r\n");
    }
    return s;
}

ESP8266_Status ESP8266_ConnectTCP(const char *host, uint16_t port)
{
    char cmd[128];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u", host, port);
    printf("[ESP] Connecting TCP: %s:%u ...\r\n", host, port);

    ESP8266_Status s = send_at_cmd(cmd, "CONNECT", ESP8266_CONN_TIMEOUT);
    if (s != ESP8266_OK) {
        /* 也可能是 "OK,CONNECT" 或 "ALREADY CONNECTED" */
        s = send_at_cmd(cmd, "OK", ESP8266_CONN_TIMEOUT);
    }
    if (s == ESP8266_OK) {
        printf("[ESP] TCP connected!\r\n");
    } else {
        printf("[ESP] TCP connect failed\r\n");
    }
    return s;
}

ESP8266_Status ESP8266_SendData(const uint8_t *data, uint16_t len)
{
    if (len == 0) return ESP8266_OK;

    /* 1) 发 AT+CIPSEND=N 命令 */
    char cmd[20];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u", len);
    uart_send(cmd);
    uart_send("\r\n");

    /* 2) 等 '>' 提示符（直接在字节流中查找，不用行解析器）
     *    ESP8266 回复：OK\r\n> （注意 > 后面可能没有 \r\n）
     *    只要看到 '>' 就说明可以发数据了 */
    uint32_t start = HAL_GetTick();
    int got_prompt = 0;
    while (HAL_GetTick() - start < 5000) {
        uint8_t ch;
        while (ring_get(&ch)) {
            /* 行解析照常（用于打印日志） */
            parse_char(ch);
            if (ch == '>') {
                got_prompt = 1;
            }
        }
        if (got_prompt) break;
        HAL_Delay(1);
    }

    if (!got_prompt) {
        printf("[ESP] CIPSEND '>' timeout\r\n");
        return ESP8266_TIMEOUT;
    }

    /* 3) 立即发送原始数据 */
    uart_send_bytes(data, len);

    /* 4) 等 SEND OK */
    cmd_done = 0;
    cmd_result = 0;
    strncpy(cmd_expect, "SEND OK", sizeof(cmd_expect) - 1);
    cmd_expect[sizeof(cmd_expect) - 1] = '\0';

    start = HAL_GetTick();
    while (HAL_GetTick() - start < 10000) {
        ESP8266_Poll();
        if (cmd_done) {
            return (cmd_result == 0) ? ESP8266_OK : ESP8266_ERROR;
        }
        HAL_Delay(5);
    }

    printf("[ESP] SEND OK timeout\r\n");
    return ESP8266_TIMEOUT;
}

ESP8266_Status ESP8266_CloseTCP(void)
{
    printf("[ESP] Close TCP...\r\n");
    return send_at_cmd("AT+CIPCLOSE", "OK", 3000);
}

ESP8266_Status ESP8266_DisconnectWiFi(void)
{
    printf("[ESP] Disconnect WiFi...\r\n");
    return send_at_cmd("AT+CWQAP", "OK", 3000);
}

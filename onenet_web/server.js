/**
 * OneNET 环境监测 Web 服务（串口版）
 *
 * 架构（完全不动 STM32）：
 *   AHT20 → STM32 → printf → USART1 (CH340) → 电脑 (COM13 @ 115200)
 *                                                ↓ SerialPort
 *                                            本服务解析
 *                                                ↓ WebSocket
 *                                            浏览器（手机/电脑）
 *
 * STM32 主循环每 2s 通过 USART1 打印：[AHT20] T=25.3 C  H=60.2 %RH
 */
const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');
const WebSocket = require('ws');
const http = require('http');
const fs = require('fs');
const path = require('path');

// ============== 配置 ==============
const SERIAL_PORT = 'COM13';
const SERIAL_BAUD = 115200;
const HTTP_PORT   = 8080;
const MAX_HISTORY = 500;

// ============== 状态 ==============
let latest  = { temperature: null, humidity: null, updated_at: null, online: false };
let history = [];
let wsClients = new Set();
let lastSerialAt = 0;

// ============== 串口 ==============
const port = new SerialPort({
  path: SERIAL_PORT,
  baudRate: SERIAL_BAUD,
  dataBits: 8,
  stopBits: 1,
  parity: 'none',
  autoOpen: false,
});

port.open((err) => {
  if (err) {
    console.error(`[Serial] open ${SERIAL_PORT} FAIL: ${err.message}`);
    console.error(`[Serial] 排查：①CH340 驱动 ②串口被占用 ③端口号对不对（设备管理器看）`);
    process.exit(1);
  }
  console.log(`[Serial] ${SERIAL_PORT} @ ${SERIAL_BAUD} opened`);
});

const parser = port.pipe(new ReadlineParser({ delimiter: '\r\n' }));

parser.on('data', (line) => {
  // STM32 实际输出: [AHT20] T=25.3 C  H=60.2 %RH
  // 兼容: 温度负数 / 整数 / 一位小数
  const m = line.match(/\[AHT20\]\s+T=([-\d.]+)\s*C\s+H=([-\d.]+)\s*%RH/);
  if (!m) return;

  const temp = parseFloat(m[1]);
  const hum  = parseFloat(m[2]);
  if (isNaN(temp) || isNaN(hum)) return;

  const now = new Date();
  const ts = now.toTimeString().slice(0, 8);
  latest = { temperature: temp, humidity: hum, updated_at: ts, online: true };
  history.push({ t: ts, temp, hum });
  if (history.length > MAX_HISTORY) history.shift();
  lastSerialAt = Date.now();

  // 推给所有 WS 客户端
  const msg = JSON.stringify({ type: 'data', latest, history });
  wsClients.forEach(ws => {
    if (ws.readyState === WebSocket.OPEN) ws.send(msg);
  });
  console.log(`[Serial] T=${temp}°C H=${hum}%RH @${ts} (history=${history.length})`);
});

// 串口断开检测：3s 没数据就标 offline
setInterval(() => {
  if (latest.online && Date.now() - lastSerialAt > 3000) {
    latest.online = false;
    const msg = JSON.stringify({ type: 'data', latest, history });
    wsClients.forEach(ws => {
      if (ws.readyState === WebSocket.OPEN) ws.send(msg);
    });
    console.log('[Serial] timeout, marked offline');
  }
}, 1000);

port.on('error', (err) => console.error('[Serial] error:', err.message));
port.on('close', () => console.log('[Serial] closed'));

// ============== HTTP + WebSocket ==============
const server = http.createServer((req, res) => {
  if (req.url === '/' || req.url === '/index.html') {
    fs.readFile(path.join(__dirname, 'public', 'index.html'), (err, content) => {
      if (err) { res.writeHead(500); res.end('Index not found'); return; }
      res.writeHead(200, { 'Content-Type': 'text/html; charset=utf-8' });
      res.end(content);
    });
  } else {
    res.writeHead(404);
    res.end('404');
  }
});

const wss = new WebSocket.Server({ server });
wss.on('connection', (ws, req) => {
  const ip = req.socket.remoteAddress;
  console.log(`[WS] client connected from ${ip}, total=${wsClients.size + 1}`);
  wsClients.add(ws);
  ws.send(JSON.stringify({ type: 'data', latest, history }));
  ws.on('close', () => {
    wsClients.delete(ws);
    console.log(`[WS] client disconnected, total=${wsClients.size}`);
  });
});

server.listen(HTTP_PORT, '0.0.0.0', () => {
  console.log(`\n========================================`);
  console.log(`[HTTP] http://localhost:${HTTP_PORT}/`);
  console.log(`[WS]   ws://localhost:${HTTP_PORT}/`);
  console.log(`[Serial] ${SERIAL_PORT} @ ${SERIAL_BAUD}`);
  console.log(`[Match] /\\[AHT20\\] T=(\\S+) C\\s+H=(\\S+) %RH/`);
  console.log(`========================================\n`);
  console.log('手机访问方式（稍后告诉你）');
});

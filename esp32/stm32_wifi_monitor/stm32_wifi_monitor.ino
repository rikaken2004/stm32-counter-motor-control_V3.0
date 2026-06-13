/*
 * ESP32-S3 STM32 WiFi Monitor
 *
 * 功能：
 *   - UART (GPIO18=RX, GPIO17=TX) 接收 STM32 USART3 状态数据
 *   - WiFi Web Server 显示 STM32 状态
 *   - 网页 RESET 按钮通过 UART 向 STM32 发送 RESET 命令
 *
 * 接线：
 *   STM32 PB10 (USART3_TX) → ESP32 GPIO18 (UART1 RX)
 *   ESP32 GPIO17 (UART1 TX) → STM32 PB11 (USART3_RX)
 *   STM32 GND → ESP32 GND
 *
 * 通信参数：115200 8N1
 */

#include <WiFi.h>
#include <WebServer.h>

/* ── WiFi 配置 ── */
const char *ssid     = "YOUR_SSID";          // ← 改为你的 WiFi 名（不要提交真实密码）
const char *password = "YOUR_PASSWORD";      // ← 改为你的 WiFi 密码

/* ── Web Server ── */
WebServer server(80);

/* ── STM32 串口 (UART1) ── */
HardwareSerial STM32Serial(1);  // UART1: RX=GPIO18, TX=GPIO17

/* ── 状态数据 ── */
String stateValue   = "--";
String targetValue  = "--";
String currentValue = "--";
String totalValue   = "--";
String motorValue   = "--";
String rawLine      = "";

/* ── 辅助：从 "KEY=VALUE,KEY=VALUE,..." 中提取某个 KEY 的值 ── */
String getValue(String line, String key) {
    int pos = line.indexOf(key + "=");
    if (pos < 0) return "--";
    pos += key.length() + 1;                   // 跳过 "KEY="
    int end = line.indexOf(",", pos);
    if (end < 0) {                             // 最后一个字段
        String val = line.substring(pos);
        val.trim();
        return val;
    }
    return line.substring(pos, end);
}

/* ── 解析 STM32 发来的数据行 ── */
void updateData(String line) {
    rawLine = line;

    /* 只解析 STATE= 开头的状态行，其他日志只显示在 rawLine */
    if (!line.startsWith("STATE=")) {
        return;
    }

    stateValue   = getValue(line, "STATE");
    targetValue  = getValue(line, "TARGET");
    currentValue = getValue(line, "CURRENT");
    totalValue   = getValue(line, "TOTAL");
    motorValue   = getValue(line, "MOTOR");
}

/* ── 根页面 / ── */
void handleRoot() {
    String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>STM32 WiFi Monitor</title>";
    html += "<style>";
    html += "body{font-family:-apple-system,sans-serif;background:#0b1120;color:#e2e8f0;";
    html += "max-width:600px;margin:0 auto;padding:24px;}";
    html += "h1{color:#38bdf8;font-size:1.4rem;}";
    html += ".card{background:#111827;border:1px solid #1e2d3d;border-radius:12px;";
    html += "padding:16px 20px;margin-bottom:12px;}";
    html += ".label{font-size:0.8rem;color:#94a3b8;}";
    html += ".value{font-size:1.5rem;font-weight:700;color:#e2e8f0;}";
    html += ".raw{font-family:monospace;font-size:0.8rem;color:#94a3b8;";
    html += "background:#0f1729;padding:10px 14px;border-radius:8px;margin-top:12px;";
    html += "word-break:break-all;}";
    html += "button{font-size:18px;padding:10px 24px;border:none;border-radius:8px;";
    html += "background:#ef4444;color:white;font-weight:600;cursor:pointer;";
    html += "width:100%;}";
    html += "button:hover{background:#dc2626;}";
    html += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;}";
    html += "</style></head><body>";

    html += "<h1>STM32 计数系统 Monitor</h1>";

    /* 状态卡片 */
    html += "<div class='grid'>";
    html += "<div class='card'><div class='label'>STATE</div><div class='value'>"
         + stateValue + "</div></div>";
    html += "<div class='card'><div class='label'>MOTOR</div><div class='value'>"
         + motorValue + "</div></div>";
    html += "<div class='card'><div class='label'>TARGET</div><div class='value'>"
         + targetValue + "</div></div>";
    html += "<div class='card'><div class='label'>CURRENT</div><div class='value'>"
         + currentValue + "</div></div>";
    html += "<div class='card'><div class='label'>TOTAL</div><div class='value'>"
         + totalValue + "</div></div>";
    html += "</div>";

    /* Raw UART Data */
    html += "<div class='card'>";
    html += "<div class='label'>Raw UART Data</div>";
    html += "<div class='raw'>" + rawLine + "</div>";
    html += "</div>";

    /* RESET 按钮 */
    html += "<p><a href='/reset'><button>RESET STM32</button></a></p>";

    /* 自动刷新每 2 秒 */
    html += "<p style='color:#64748b;font-size:0.75rem;text-align:center;'>";
    html += "Auto-refresh: 2s | <a href='/' style='color:#38bdf8;'>Manual Refresh</a></p>";
    html += "<meta http-equiv='refresh' content='2'>";

    html += "</body></html>";
    server.send(200, "text/html", html);
}

/* ── /reset 路由 ── */
void handleReset() {
    STM32Serial.println("RESET");
    Serial.println("[INFO] Sent command to STM32: RESET");
    server.sendHeader("Location", "/");
    server.send(303);
}

/* ── /api/state 路由 (JSON 格式, 方便程序化访问) ── */
void handleApiState() {
    String json = "{";
    json += "\"state\":\"" + stateValue + "\",";
    json += "\"target\":" + targetValue + ",";
    json += "\"current\":" + currentValue + ",";
    json += "\"total\":" + totalValue + ",";
    json += "\"motor\":\"" + motorValue + "\"";
    json += "}";
    server.send(200, "application/json", json);
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n[BOOT] ESP32-S3 STM32 WiFi Monitor");

    /* ── WiFi 连接 ── */
    WiFi.begin(ssid, password);
    Serial.print("[WiFi] Connecting");
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 30) {
        delay(500);
        Serial.print(".");
        retry++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Connected!");
        Serial.print("[WiFi] IP: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\n[WiFi] Failed to connect! Continuing as AP-less...");
    }

    /* ── Web Server 路由 ── */
    server.on("/", handleRoot);
    server.on("/reset", handleReset);
    server.on("/api/state", handleApiState);
    server.begin();
    Serial.println("[HTTP] Server started on port 80");

    /* ── STM32 串口 ── */
    STM32Serial.begin(115200, SERIAL_8N1, 18, 17);  // RX=GPIO18, TX=GPIO17
    Serial.println("[UART] STM32Serial ready (RX=GPIO18, TX=GPIO17, 115200 8N1)");
}

void loop() {
    server.handleClient();

    /* ── 逐行读取 STM32 串口数据 ── */
    while (STM32Serial.available()) {
        String line = STM32Serial.readStringUntil('\n');
        line.trim();                     // 去除 \r 和首尾空白
        if (line.length() > 0) {
            Serial.print("[STM32] ");
            Serial.println(line);        // ESP32 串口监视器回显
            updateData(line);
        }
    }
}

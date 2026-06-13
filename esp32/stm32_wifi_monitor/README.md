# ESP32-S3 STM32 WiFi Monitor

ESP32-S3 Arduino 工程。通过 UART 接收 STM32 系统状态，WiFi Web Server 显示，支持网页 RESET 按钮反向控制 STM32。

## 开发环境

- **IDE**：Arduino IDE
- **开发板**：`ESP32S3 Dev Module`（在 Tools → Board → esp32 中选择）
- **依赖**：需在 Boards Manager 安装 `esp32 by Espressif Systems`

## 上传

1. 用 USB 线连接 ESP32-S3 的 **USB-UART 口**（显示 CH340 的那个 Type-C）
2. Arduino IDE 选择对应的端口
3. 修改 `.ino` 文件中的 WiFi 名称和密码为你的网络
4. 点击 Upload
5. 上传完成后打开 Serial Monitor，波特率选 **115200**

## WiFi 配置

```cpp
const char *ssid     = "YOUR_SSID";       // 改为你的 WiFi 名
const char *password = "YOUR_PASSWORD";   // 改为你的 WiFi 密码
```

**不要将真实密码提交到 Git！**

## 接线

| STM32 | ESP32-S3 |
|-------|----------|
| PB10 (USART3_TX) | GPIO18 (UART1 RX) |
| PB11 (USART3_RX) | GPIO17 (UART1 TX) |
| GND | GND |

两块板各自 USB 供电，不连接 3.3V。

## 使用

1. 烧录后打开串口监视器（115200），看到 `[HTTP] Server started on port 80` 和 IP 地址
2. 浏览器访问该 IP
3. 网页每 2 秒自动刷新，显示 STM32 状态
4. 点击 **RESET STM32** 按钮可远程复位 STM32

## 路由

| 路由 | 功能 |
|------|------|
| `/` | 主页（状态卡片 + Raw UART Data + RESET 按钮） |
| `/reset` | 发送 RESET 命令到 STM32，然后重定向回主页 |
| `/api/state` | JSON 格式状态接口 |

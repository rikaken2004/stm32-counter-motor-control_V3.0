# ESP32-S3 STM32 WiFi Monitor — 无线状态监控与复位控制

## 功能目标

ESP32-S3 通过 UART 接收 STM32 周期性输出的状态行（`STATE=...`），在 WiFi Web 页面上实时显示。网页上的 **RESET STM32** 按钮可通过 UART 向 STM32 发送 `RESET` 命令，实现远程复位控制，形成双向通信闭环。

## 接线表

| STM32 引脚 | ESP32-S3 引脚 | 说明 |
|------------|---------------|------|
| PB10 / USART3_TX | GPIO18 / UART1 RX | STM32 → ESP32 状态数据 |
| PB11 / USART3_RX | GPIO17 / UART1 TX | ESP32 → STM32 RESET 命令 |
| GND | GND | **必须共地** |

**重要**：
- 两块板各自 USB 供电，**不要连接 3.3V**
- 不要使用 GPIO43/GPIO44（板载 CH340 下载串口）
- 不要接 5V 到任何 GPIO

## 为什么不用 GPIO43/GPIO44

GPIO43 = U0TXD, GPIO44 = U0RXD。这两个引脚已经连接到 ESP32-S3 开发板的 CH340 USB 转串口芯片，用于**烧录固件**和 Arduino IDE **Serial Monitor**。如果把它俩同时用于 STM32 通信，会产生三组设备（电脑 CH340 ↔ STM32 ↔ ESP32）信号冲突。

因此 STM32 通信选择独立的 GPIO18/GPIO17 作为 UART1。

## 通信参数

| 参数 | 值 |
|------|-----|
| 波特率 | 115200 |
| 数据位 | 8 |
| 校验位 | None |
| 停止位 | 1 |
| 流控 | None |

## STM32 输出格式（状态行）

```
STATE=IDLE,TARGET=5,CURRENT=0,TOTAL=25,MOTOR=OFF
STATE=COUNTING,TARGET=5,CURRENT=3,TOTAL=25,MOTOR=OFF
STATE=MOTOR_RUN,TARGET=5,CURRENT=5,TOTAL=30,MOTOR=ON
STATE=DONE,TARGET=5,CURRENT=5,TOTAL=30,MOTOR=OFF
```

每 500ms 输出一行。`KEY=VALUE` 逗号分隔，方便 ESP32/Linux 解析。

## ESP32 发给 STM32 的命令

```
RESET\n
```

点击网页 RESET 按钮后由 ESP32 发送。STM32 收到后执行复位并回复：

```
CMD RESET OK
```

若收到未支持的命令则回复：

```
CMD UNKNOWN
```

## ESP32 网页功能

| 页面/路由 | 功能 |
|-----------|------|
| `/` | 主页：5 个状态卡片（STATE/TARGET/CURRENT/TOTAL/MOTOR）+ Raw UART Data + RESET 按钮 + 每 2 秒自动刷新 |
| `/reset` | 向 STM32 发送 `RESET` 命令，重定向回 `/` |
| `/api/state` | JSON 格式状态接口，方便程序化访问 |

## 数据解析规则

STM32 不只输出 `STATE=` 行，还会输出：

```
KEY ADD target=3
STATE IDLE->COUNTING target=5
IR count current=3 target=5 lvl=1
MOTOR speed ramp=30
CMD RESET OK
```

ESP32 端 **只解析以 `STATE=` 开头的行**来更新状态卡片。其他日志只显示在 Raw UART Data 区域，不会把 STATE/TARGET/CURRENT/TOTAL/MOTOR 刷成 `--`。

关键代码：

```cpp
void updateData(String line) {
    rawLine = line;

    if (!line.startsWith("STATE=")) {
        return;   // 非状态行只显示 Raw，不更新卡片
    }

    stateValue   = getValue(line, "STATE");
    targetValue  = getValue(line, "TARGET");
    currentValue = getValue(line, "CURRENT");
    totalValue   = getValue(line, "TOTAL");
    motorValue   = getValue(line, "MOTOR");
}
```

## ESP32 侧关键代码

- `HardwareSerial STM32Serial(1)` — UART1, GPIO18 RX, GPIO17 TX
- `updateData()` — 解析 `STATE=` 行，提取各字段
- `handleRoot()` — 生成 HTML 页面（状态卡片 + Raw 数据 + RESET 按钮）
- `handleReset()` — `/reset` 路由，发送 `RESET\n` 到 STM32
- `handleApiState()` — `/api/state` JSON API

## STM32 侧关键代码

- **USART3_RX 中断**：`USART3_IRQHandler` 中逐字符接收，跳过 `\r`，遇 `\n` 判断命令
- **32 字节命令缓冲区**：`rx_buf[32]`，防溢出
- **RESET 命令识别**：匹配 `rx_buf == "RESET"` → 设置 `uart_reset_request = 1`
- **标志位 + 主循环执行**：不在 ISR 中操作复杂状态机。`App_Task()` 检查标志 → 调用 `App_ResetToIdle()`
- **App_ResetToIdle()**：`Motor_Stop()` + `LED10_OFF()` + target/current 清零 + state=IDLE

## 调试步骤

### 第一步：单向通信（STM32 → ESP32）

1. **只接** STM32 PB10 → ESP32 GPIO18 + GND
2. ESP32 烧录代码 → Serial Monitor 115200
3. 确认收到 `[STM32] STATE=IDLE,...`
4. 浏览器打开 ESP32 IP → 确认状态卡片有数据

### 第二步：双向通信（STM32 ←→ ESP32）

1. **再接** ESP32 GPIO17 → STM32 PB11
2. 打开 STM32 串口助手（115200）
3. 浏览器点击 **RESET STM32** 按钮
4. STM32 OLED 回到 IDLE，T/C 清零
5. STM32 串口有 `CMD RESET OK`
6. 网页 Raw Data 同步显示反馈

### 第三步：完整工作流

1. ADD 键设 target → CONFIRM 开始计数 → 红外触发 → CURRENT 递增 → 电机运行 → 点击网页 RESET → 系统回 IDLE
2. 确认整个闭环：OLED + STM32 串口 + ESP32 网页三方显示一致

## 常见问题

| 问题 | 可能原因 | 解决 |
|------|----------|------|
| 收到乱码 | 波特率不一致 | 确认双方都是 115200 |
| 收到乱码 | GND 未共地 | GND ↔ GND |
| 收不到任何数据 | TX/RX 接反 | STM32 TX → ESP32 RX |
| 收不到任何数据 | GPIO 接错 | 确认接 GPIO18 非 GPIO43 |
| 网页有 Raw 数据但卡片为 `--` | 非 `STATE=` 行刷掉了状态 | 检查 `startsWith("STATE=")` 过滤 |
| 点击 RESET 无反应 | 双向接线未接 | 检查 GPIO17 → PB11 |
| 两块板均不上电 | 没插 USB 线 | 各自 USB 供电 |

## 文件位置

```
esp32/stm32_wifi_monitor/
├── stm32_wifi_monitor.ino    # Arduino 工程
└── README.md                 # ESP32 工程说明
```

## STM32 侧代码改动摘要

| 文件 | 改动 |
|------|------|
| `uart_debug.c` | USART3 RXNE 中断使能 + `USART3_IRQHandler` + 命令缓冲区 + `uart_reset_request` |
| `uart_debug.h` | `UART_Debug_GetResetRequest()` / `UART_Debug_ClearResetRequest()` |
| `app.c` | `App_ResetToIdle()` 统一复位 + 主循环 UART 标志检查 + 3 个 RESET 路径重构 |

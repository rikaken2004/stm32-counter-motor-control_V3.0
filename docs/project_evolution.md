# STM32 计数/电机控制系统扩展过程记录

> 从本地控制到 Linux 日志采集、ESP32 无线监控、网页 RESET 控制的逐步迭代过程。不是"最终成果展示"，而是真实推进记录。

---

## 0. 起点：STM32 本地控制系统

最初是一个基于 STM32F103 的计数与电机控制系统：

- 按键设定目标计数 target
- 红外传感器 EXTI14 触发计数 current
- 达到目标后电机软启动运行 3 秒
- OLED 显示四行状态
- USART3 输出调试日志
- PCB V1 打样、焊接、调试完成

系统结构：

```
红外/按键/OLED/电机 → STM32F103 → 本地 OLED + 串口调试
```

**局限**：状态只能在 OLED 看，不能长期记录，不能无线查看。

---

## 1. 第一阶段：STM32 增加周期性状态输出

### 为什么要做

要让外部设备（Linux/ESP32）能解析 STM32 状态，必须先有稳定、清晰的机器可读输出格式。

### 设计

- USART3 PB10 = TX
- 每 500ms 输出一行：`STATE=IDLE,TARGET=3,CURRENT=0,TOTAL=25,MOTOR=OFF`
- `KEY=VALUE` 格式，方便 Linux/ESP32 解析

### STM32 关键代码思路

```c
static void print_status_line(void)
{
    UART_Debug_SendString("STATE=");
    // ... 拼接 TARGET/CURRENT/TOTAL/MOTOR
    UART_Debug_SendString("\r\n");
}

void App_Task(void)
{
    if (App_Millis() - last_status_print_ms >= 500) {
        last_status_print_ms = App_Millis();
        print_status_line();
    }
}
```

### 成果

STM32 稳定输出标准化状态行，为后续扩展打下格式基础。

---

## 2. 第二阶段：Ubuntu Linux 读取 STM32 串口日志

### 为什么要做

OLED 只能看当前状态，不能长期保存。Linux 上位机可行：
- 串口读取
- 自动加时间戳
- 保存日志文件

系统结构变为：

```
STM32 PB10 → CH340 → Ubuntu /dev/ttyUSB0 → Python → 时间戳日志
```

### 环境准备

```bash
sudo apt update
sudo apt install python3-serial minicom -y
lsusb           # 确认 CH340
ls /dev/ttyUSB* # 查看设备
```

### 接线

```
STM32 PB10 (USART3_TX) → CH340 RXD
STM32 GND              → CH340 GND
```

### 先用 minicom 验证

```bash
sudo minicom -D /dev/ttyUSB0 -b 115200
```

### Python 日志采集程序

代码位置：[linux/serial_logger/serial_logger.py](../linux/serial_logger/serial_logger.py)

运行：

```bash
python3 serial_logger.py
```

成功输出：

```
2026-06-11 01:07:02 STATE=IDLE,TARGET=3,CURRENT=0,TOTAL=0,MOTOR=OFF
```

### 遇到的问题

| 问题 | 现象 | 根因 | 解决 |
|------|------|------|------|
| Ubuntu apt 卡住 | 软件包无法下载 | VMware NAT 服务 | 重启 VMware NAT/DHCP |
| 虚拟机无法复制粘贴 | 粘贴不进去 | 缺少 open-vm-tools | `sudo apt install open-vm-tools open-vm-tools-desktop -y` |
| 没有 /dev/ttyUSB0 | 找不到设备 | CH340 未连接虚拟机 | VMware → 可移动设备 → 连接 |
| 日志文件权限 | 无法编辑 | root 权限 | `sudo chown $USER:$USER stm32_log_*.txt` |
| KeyboardInterrupt | 退出显示异常 | 正常 Ctrl+C | 这是正常停止，不是报错 |

### 成果

项目从"本地显示"扩展到"上位机日志采集"。

---

## 3. 第三阶段：ESP32-S3 基础上手

### 为什么要做

Linux 适合记录数据，不能解决无线查看问题。引入 ESP32-S3：

```
STM32 → UART → ESP32-S3 → WiFi → 浏览器查看状态
```

### 环境搭建

插入 ESP32-S3 时 Windows 设备管理器显示 `USB-SERIAL CH340`。

最初误开了 Arduino App Lab（显示 No boards found），应使用 **Arduino IDE**。

- Boards Manager 安装 `esp32 by Espressif Systems`
- 开发板选择 `ESP32S3 Dev Module`

### 串口测试

```cpp
void setup() {
    Serial.begin(115200);
    Serial.println("ESP32-S3 start");
}
```

成功标志：串口监视器看到输出。

### WiFi 测试

```cpp
WiFi.begin(ssid, password);
// ... 等待连接
Serial.println(WiFi.localIP());
```

成功标志：看到 IP 地址。

### Web Server 假数据测试

在接 STM32 之前，先让 ESP32 自己生成假数据显示在网页上，把 WiFi + Web Server 功能独立验证通过。

---

## 4. 第四阶段：STM32 → ESP32 单向串口通信

### 接线

```
STM32 PB10 (USART3_TX) → ESP32 GPIO18 (UART1 RX)
STM32 GND              → ESP32 GND
```

两块板各自 USB 供电，不连接 3.3V。

### 引脚选择

U0TXD/U0RXD (GPIO43/44) 已用于板载 CH340 下载串口，不能再用。因此选择独立 GPIO：

```
ESP32 GPIO18 = UART1 RX（收 STM32 数据）
ESP32 GPIO17 = UART1 TX（后续发 RESET 命令）
```

### 遇到的乱码问题

| 排查项 | 说明 |
|--------|------|
| ESP32 自身打印是否正常 | 本地 Serial.print 正常 → 电脑串口波特率没问题 |
| STM32 接 CH340 是否正常 | 在 PC 串口助手确认 115200 输出正常 |
| TX/RX 是否接反 | 应为 STM32 TX → ESP32 RX |
| GND 是否共地 | 未共地会出现能收到但乱码 |
| GPIO 是否接错 | 代码里 RX=18 就必须接 GPIO18 |

逐一排查后确认：GND 共地和波特率一致是关键。

---

## 5. 第五阶段：ESP32 网页显示 STM32 真实数据

### 功能目标

把网页里的假数据换成 STM32 真实状态。

### 数据解析

```cpp
String getValue(String line, String key) {
    int pos = line.indexOf(key + "=");
    if (pos < 0) return "--";
    pos += key.length() + 1;
    int end = line.indexOf(",", pos);
    if (end < 0) {
        String val = line.substring(pos);
        val.trim();
        return val;
    }
    return line.substring(pos, end);
}
```

### 为什么需要 `startsWith("STATE=")` 过滤

调试中发现 STM32 不只输出 `STATE=...`，还会输出：

```
KEY ADD target=3
MOTOR RUN START speed=35
CMD RESET OK
```

如果每行都调用 `getValue()`，非 `STATE=` 行会返回 `--`，把状态卡片刷掉。因此必须过滤：

```cpp
void updateData(String line) {
    rawLine = line;
    if (!line.startsWith("STATE=")) return;  // 非状态行只显示 Raw，不更新卡片
    stateValue   = getValue(line, "STATE");
    targetValue  = getValue(line, "TARGET");
    // ...
}
```

### 成果

浏览器访问 ESP32 IP，实时看到 STM32 状态。项目实现：

```
STM32 本地控制系统 → UART → ESP32-S3 → WiFi → 浏览器实时查看状态
```

---

## 6. 第六阶段：ESP32 → STM32 反向 RESET 控制

### 为什么要做

单向显示只能"看"。增加反向控制后：

```
STM32 → ESP32 → 网页显示
网页按钮 → ESP32 → STM32  ← 双向！
```

### 完整接线

```
STM32 PB10 (USART3_TX) → ESP32 GPIO18 (UART1 RX)
ESP32 GPIO17 (UART1 TX) → STM32 PB11 (USART3_RX)
STM32 GND              → ESP32 GND
各自 USB 供电，不连接 3.3V
```

### ESP32 侧

`/reset` 路由发送 `RESET\n`：

```cpp
void handleReset() {
    STM32Serial.println("RESET");
    server.sendHeader("Location", "/");
    server.send(303);
}
```

网页按钮：`<a href='/reset'><button>RESET STM32</button></a>`

### STM32 侧

USART3 RXNE 中断接收命令 → 命令缓冲区 → 识别 `RESET` → 设置标志位 → `App_Task()` 主循环执行 `App_ResetToIdle()`：

```c
void USART3_IRQHandler(void) {
    // 收字符到 rx_buf，遇 '\n' 判断命令
    // 匹配 "RESET" → uart_reset_request = 1
}

void App_Task(void) {
    if (UART_Debug_GetResetRequest()) {
        UART_Debug_ClearResetRequest();
        App_ResetToIdle();
    }
}
```

### 成功效果

点击网页 RESET 按钮后：
- STM32 OLED 回到 IDLE
- TARGET/CURRENT 清零
- 串口回发 `CMD RESET OK`
- 网页 Raw UART Data 同步显示反馈

---

## 7. 最终系统结构

```
                    ┌──────────────────────┐
                    │      浏览器网页       │
                    │ 状态显示 / RESET按钮  │
                    └──────────▲───────────┘
                               │ WiFi
                               │
┌──────────────────────┐ UART  │   ┌──────────────────────┐
│       STM32F103       │◄─────►│   │      ESP32-S3        │
│ 计数 / 电机 / OLED    │       │   │ Web Server / WiFi    │
│ PB10 TX / PB11 RX     │       │   │ GPIO18 RX / GPIO17 TX│
└──────────┬───────────┘       └──────────────────────┘
           │ CH340 / USB
           ▼
┌──────────────────────┐
│ Ubuntu Linux Logger   │
│ Python 时间戳日志保存 │
└──────────────────────┘
```

---

## 8. 项目价值

不是单点功能堆砌，而是逐步升级的工程过程：

```
本地控制 → 上位机记录 → 无线监控 → 双向控制
```

1. 先完成 STM32 本地控制闭环
2. 再让 STM32 输出标准化状态行
3. 用 Ubuntu Linux 完成串口日志采集
4. 用 ESP32-S3 完成 WiFi 联网和网页显示
5. 用 UART 把 STM32 与 ESP32 连接起来
6. 最后增加网页按钮，实现 ESP32 反向控制 STM32

---

## 9. 后续规划

| 优先级 | 方向 | 说明 |
|--------|------|------|
| 高 | 文档整理、截图、接线图、README | GitHub 展示和面试讲解 |
| 中 | ESP32 网页设置 target | 从 RESET 控制扩展到参数控制 |
| 低 | INMP441 / MAX98357A 音频 | 音频方向后续再做 |
| 低 | PCB V2 | 等功能路线稳定后再画 |

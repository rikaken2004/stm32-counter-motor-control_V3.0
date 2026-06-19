# STM32-ESP32 HIL 控制器测试板 V2 IO 分配草案

> 以 `2-1/hardware/` 和 `2-1/user/` 实际代码扫描结果为准。部分 hardware/ 目录下的驱动（Buzzer、Encoder、NEW_Encoder、AD、New_AD、IC、LightSensor、NEW_LED）未在当前 app.c 中调用，本文档仅列出当前项目实际使用的 IO。

## 1. 当前已验证 IO（HIL 核心链路）

| 信号 | STM32 引脚 | ESP32 引脚 | 方向 | 当前状态 |
|------|-----------|-----------|------|----------|
| 虚拟传感器脉冲注入 | PB14 (IPU, EXTI14) | GPIO4 (Out_PP) | ESP32 → STM32 | 已验证 |
| STM32 状态反馈 | PB10 / USART3_TX (AF_PP) | GPIO18 / UART1 RX | STM32 → ESP32 | 已验证 |
| ESP32 命令下发 | PB11 / USART3_RX (IN_FLOATING) | GPIO17 / UART1 TX | ESP32 → STM32 | 已验证 (RESET) |
| GND | GND | GND | 共地 | 已验证 |

> ESP32 GPIO4 到 STM32 PB14 串联 1kΩ 电阻（已验证稳定）。

## 2. STM32 当前项目实际使用 IO（以代码为准）

| 引脚 | 功能 | 配置 | 来源文件 | 备注 |
|------|------|------|----------|------|
| PA6 | 电机 PWM | TIM3_CH1, AF_PP | PWM.c / Motor.c | TB6612 PWMA |
| PA9 | LED9（计数闪烁） | Out_PP, LOW=ON | LED.c | |
| PA10 | LED10（电机指示） | Out_PP, LOW=ON | LED.c | |
| PB1 | ADD 按键 | IPU | Key.c / app_key.c | 低电平有效 |
| PB8 | OLED SCL | Out_OD | OLED.c | 软件 I2C |
| PB9 | OLED SDA | Out_OD | OLED.c | 软件 I2C |
| PB10 | USART3_TX | AF_PP | uart_debug.c | 115200 8N1 |
| PB11 | USART3_RX | IN_FLOATING | uart_debug.c | RXNE 中断接收命令 |
| PB12 | 电机方向 AIN1 | Out_PP | Motor.c | TB6612 |
| PB13 | 电机方向 AIN2 | Out_PP | Motor.c | TB6612 |
| PB14 | 计数传感器输入 | IPU, EXTI14 | CountSensor.c | 上升沿触发 |
| PC13 | CONFIRM/RESET 按键 | IPU | Key.c / app_key.c | 短按/长按 >2s |

> 注意：Buzzer.c 使用 PB12（与 Motor 冲突）、Encoder.c 使用 PA0/PA1、NEW_Encoder.c 使用 PA6/PA7（与 PWM 冲突）、AD.c/New_AD.c 使用 PA0~PA3、IC.c 使用 PA0、LightSensor.c 使用 PB14（与 CountSensor 冲突）、NEW_LED.c 使用 PA6（与 PWM 冲突）。以上驱动均未在当前 main.c → App_Task() 调用链中使用。

## 3. STM32F103C8T6 已占用 / 可用 IO 汇总

| 端口 | 已用 | 可用（未在当前项目使用） |
|------|------|------------------------|
| PA0 | — | 可用（AD/Encoder/IC 未用） |
| PA1 | — | 可用 |
| PA2 | — | 可用 |
| PA3 | — | 可用 |
| PA4 | — | 可用 |
| PA5 | — | 可用 |
| PA6 | PWM 电机 | 已用 |
| PA7 | — | 可用 |
| PA8 | — | 可用 |
| PA9 | LED9 | 已用 |
| PA10 | LED10 | 已用 |
| PA11 | — | 可用（CAN_RX 可选） |
| PA12 | — | 可用（CAN_TX 可选） |
| PA13 | SWDIO | 保留（SWD 下载） |
| PA14 | SWCLK | 保留（SWD 下载） |
| PA15 | — | 可用 |
| PB0 | — | 可用 |
| PB1 | ADD 按键 | 已用 |
| PB2 | BOOT1 | 保留 |
| PB3 | — | 可用（JTDO，SWD 模式下可作 GPIO） |
| PB4 | — | 可用（JNTRST，SWD 模式下可作 GPIO） |
| PB5 | — | 可用 |
| PB6 | — | 可用 |
| PB7 | — | 可用 |
| PB8 | OLED SCL | 已用（软件 I2C） |
| PB9 | OLED SDA | 已用（软件 I2C） |
| PB10 | USART3_TX | 已用 |
| PB11 | USART3_RX | 已用 |
| PB12 | 电机 AIN1 | 已用 |
| PB13 | 电机 AIN2 | 已用 |
| PB14 | 计数输入 | 已用 |
| PB15 | — | 可用 |
| PC13 | CONFIRM/RESET | 已用（注意：Blue Pill 板载 LED） |
| PC14 | — | 可用 |
| PC15 | — | 可用 |

## 4. V2 新增接口建议

### 4.1 IMU1 I2C 接口

建议复用 PB8/PB9（当前 OLED 软件 I2C 总线），在同一组 I2C 上挂载 OLED + IMU。OLED 地址通常为 0x3C，MPU6050 地址通常为 0x68，不冲突。

| 引脚 | 信号 |
|------|------|
| PB8 | SCL |
| PB9 | SDA |
| 3.3V | 电源 |
| GND | 地 |
| PA0 (可选) | INT 中断引脚 |
| GND (可选) | AD0 地址选择 |

### 4.2 IMU2 I2C 接口

如果需要第二组独立的 I2C（避免地址冲突或用于冗余传感器对比测试），建议使用软件 I2C 模拟在空闲 GPIO 上。推荐 PB6/PB7 或 PB3/PB4。

| 方案 A | 方案 B |
|--------|--------|
| PB6 = SCL, PB7 = SDA | PB3 = SCL, PB4 = SDA |

### 4.3 舵机 PWM1 / PWM2

使用 STM32 空闲定时器 + GPIO 输出 PWM。STM32F103C8T6 有 TIM2/TIM3/TIM4 多个定时器，TIM3_CH1 已被电机占用（PA6），剩余通道可选。

| 舵机 | 推荐引脚 | 备选 |
|------|---------|------|
| PWM1 | PA0 (TIM2_CH1) | PA1 (TIM2_CH2) |
| PWM2 | PA2 (TIM2_CH3) | PA3 (TIM2_CH4) |

每路配 3Pin 排针：PWM / 5V / GND。5V 从外部供电端子取，不经过 STM32。

### 4.4 RS485 预留

使用一组空闲 UART（STM32F103 有 USART1/USART2/USART3，USART3 已用）。

- 推荐 USART1：PA9/TX（当前被 LED9 占用，需调整）或 PA2/TX
- 备选 USART2：PA2/TX, PA3/RX
- 需外接 MAX3485 模块（RE/DE 控制用空闲 GPIO）

先预留 4Pin 排针 + 两路 GPIO（RE/DE），RS485 收发器用外部模块。

### 4.5 CAN 预留

STM32F103C8T6 内置 bxCAN 控制器，仅需外部 CAN 收发器（如 TJA1050）。

| 引脚 | 信号 |
|------|------|
| PA11 | CAN_RX |
| PA12 | CAN_TX |

当前 PA11/PA12 空闲。V2 先预留 4Pin 排针（PA11 / PA12 / 3.3V / GND），CAN 收发器用外部模块。

### 4.6 逻辑分析仪测试排针

| 序号 | 信号 | 引脚 |
|------|------|------|
| 1 | GND | GND |
| 2 | HIL_PULSE (ESP32 GPIO4) | 测试点 |
| 3 | PB14 (STM32 Count) | 测试点 |
| 4 | USART3_TX (PB10) | 测试点 |
| 5 | USART3_RX (PB11) | 测试点 |
| 6 | PWM (PA6) | 测试点 |
| 7 | I2C SCL (PB8) | 测试点 |
| 8 | I2C SDA (PB9) | 测试点 |

排列为 1×8 或 2×4 排针，间距 2.54mm。

### 4.7 电源测试点

| 测试点 | 丝印 |
|--------|------|
| 3.3V | `+3V3` |
| 5V | `+5V` |
| GND (×2) | `GND` |

使用通孔焊盘（直径 ≥ 1.5mm），方便万用表表笔接触。

### 4.8 SWD 下载接口

| 引脚 | 信号 |
|------|------|
| 1 | 3.3V |
| 2 | PA13 / SWDIO |
| 3 | PA14 / SWCLK |
| 4 | GND |
| 5 (可选) | NRST |

标准 2.54mm 间距排针，兼容 ST-Link V2。

## 5. ESP32-S3 侧引脚

ESP32-S3 以开发板形式插入 V2 载板（使用排母插座），仅连接以下信号：

| ESP32 引脚 | 连接目标 | 功能 |
|-----------|---------|------|
| GPIO4 | → 1kΩ → STM32 PB14 | 虚拟传感器脉冲输出 |
| GPIO18 (U1RX) | ← STM32 PB10 | 状态反馈接收 |
| GPIO17 (U1TX) | → STM32 PB11 | 命令下发 |
| GND | ↔ STM32 GND | 共地 |

ESP32 的 USB 口独立供电，3.3V 不与 STM32 连接。

ESP32 关键启动脚（GPIO0/IO0、EN）保留给开发板自身，不在 V2 载板上额外连接。

## 6. 风险点

| 风险 | 说明 | 缓解措施 |
|------|------|----------|
| PB14 双驱动冲突 | 真实传感器和 ESP32 同时输出到 PB14 | 3Pin 跳线三选一，物理上避免同时接入 |
| I2C 地址冲突 | 多个 I2C 设备共用 PB8/PB9 | 记录设备地址，IMU2 走独立软件 I2C |
| 舵机 5V 反灌 | 舵机 5V 电源可能影响 3.3V 系统 | 5V 独立供电端子，不经过 3.3V 稳压 |
| 引脚复用冲突 | USART1 重映射可能与当前 IO 重叠 | 选择 USART2 的 PA2/PA3 方案优先 |
| CAN 与 USB 共存 | STM32F103 的 CAN 和 USB 共享 SRAM 缓冲区，不能同时使用 | 本项目不使用 USB，无冲突 |
| ESP32 启动脚误用 | GPIO0/IO2/IO12 等涉及启动模式 | 不使用这些引脚做 HIL 信号 |
| OLED + IMU 共用 I2C 总线负载 | 软件 I2C 时序可能被 IMU 中断影响 | 预留独立 IMU2 软件 I2C 接口作为备选 |

## 7. 下一步待确认问题

以下问题在开始 PCB 设计前需确认答案：

1. IMU1 是否继续使用 PB8/PB9（与 OLED 共用 I2C），还是单独分配？
2. 是否需要独立的第二组软件 I2C 给 IMU2？如需要，用哪个 GPIO 对？
3. 舵机 PWM1/PWM2 使用哪个定时器的哪个通道？（建议 TIM2_CH1~CH4 = PA0~PA3）
4. RS485 使用 USART1 还是 USART2？RE/DE 控制用哪个 GPIO？
5. CAN 是否仅预留排针模块接口，还是需要板载 TJA1050？
6. ESP32 命令下发（GPIO17 → STM32 PB11）是否已稳定，是否纳入正式走线？
7. PCB 是否保留真实红外传感器 3Pin 接口（VCC/GND/OUT）？
8. LED9/PA9 是否调整以释放 USART1_TX 给 RS485？如不调整，RS485 走 USART2。
9. 尺寸限制：保持 10cm×10cm 以内，还是接受稍大的板子？

---

*文档状态：草案  |  2026-06  |  IO 配置以 `2-1/` 代码扫描结果为准  |  部分引脚可用性需对照 STM32F103C8T6 数据手册确认*

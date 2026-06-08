# STM32 智能计数与电机控制系统

基于 STM32F103C8T6 的工业计数控制项目，是从 51 双 MCU 方案到 STM32 单芯片的工程化升级。
<img width="1706" height="1279" alt="整体情况" src="https://github.com/user-attachments/assets/1f9d2fa8-1aec-4fb2-aba0-6b3e7ca17a61" />
<img width="1706" height="1279" alt="整体预览图" src="https://github.com/user-attachments/assets/6f718d48-c66b-4ba7-bd2b-7b798305e75c" />
<img width="1706" height="1279" alt="背面焊板情况" src="https://github.com/user-attachments/assets/408649e2-4548-4daa-bf89-d05795100da1" />
演示视频链接https://t.bilibili.com/1211556445441491012?share_source=pc_native

## 功能特性

- **红外脉冲计数**：EXTI 中断 + 差值法 delta 读取 + 应用层消抖
- **4 状态非阻塞状态机**：IDLE → COUNTING → MOTOR_RUN → DONE
- **PWM 电机控制**：TB6612FNG 驱动，软启动 ramp，固定时长运行
- **OLED 实时显示**：目标值 / 当前计数 / 累计值 / 状态 / 电机状态
- **非阻塞按键**：PB1 设置目标，PC13 短按确认 / 长按复位
- **LED 指示**：计数闪烁 + 电机运行指示
- **USART3 调试日志**：状态转换 / IR 计数 / 电机事件 / PB14 电平诊断

## 硬件组成

| 模块 | 型号/说明 | 接口 |
|------|-----------|------|
| MCU | STM32F103C8T6 (Blue Pill) | 核心板插座 |
| 显示 | 0.96" OLED 128x64 I2C | PB8(SCL) / PB9(SDA) |
| 电机驱动 | TB6612FNG | PA6(PWM) / PB12/PB13(方向) |
| 红外传感器 | 4Pin 模块 (DO 数字输出) | PB14 (EXTI14) |
| 按键 ADD | 微动开关 | PB1 (IPU) |
| 按键 CONFIRM/RESET | 微动开关 | PC13 (IPU) |
| LED9 (计数) | 5mm LED | PA9 |
| LED10 (电机指示) | 5mm LED | PA10 |
| 调试串口 | USART3 115200 8N1 | PB10(TX) / PB11(RX) |

## 软件状态机

```
    ┌─────────┐  CONFIRM   ┌──────────┐  C>=T   ┌───────────┐  3000ms  ┌────────┐
    │  IDLE   │ ─────────→ │ COUNTING │ ──────→ │ MOTOR_RUN │ ───────→ │  DONE  │
    │ 电机OFF │  (T>0)     │ 电机OFF   │         │ 电机ON     │          │ 电机OFF │
    │ ADD设T  │            │ IR计数     │         │ 软启动     │          │ 等待确认 │
    └─────────┘            └──────────┘         └───────────┘          └────────┘
         ▲                      │                      │                    │
         │                      │ CONFIRM/RESET        │ CONFIRM/RESET      │ CONFIRM/RESET
         │                      ▼                      ▼                    ▼
         │                 ┌────────────────────────────────────────────────┘
         │                 │  停电机, T=0, C=0, → IDLE
         └─────────────────┘
```

- **IDLE**：设置目标次数 (0~99)，CONFIRM 开始计数
- **COUNTING**：红外触发 current_count++，电机不转，达到目标 → MOTOR_RUN
- **MOTOR_RUN**：软启动 (25%→35%)，LED10 亮，3000ms 后自动停止 → DONE
- **DONE**：本轮完成，CONFIRM/RESET 开始下一轮

任意状态长按 PC13 (>2s) 或短按 CONFIRM → RESET 回 IDLE。

## 项目结构

```
STM32_COUNTER_PROJECT/
├── 2-1/                          # Keil MDK 工程
│   ├── user/                     # 应用层 (app / app_key / app_time / uart_debug)
│   ├── hardware/                 # 外设驱动 (Motor / PWM / CountSensor / OLED / LED / Key)
│   ├── library/                  # STM32 标准外设库
│   ├── start/                    # 启动文件 + CMSIS
│   └── system/                   # Delay
├── docs/                         # 项目文档
├── hardware/PCB_V1/              # PCB 设计文件
├── media/                        # 照片 & 视频
├── PROJECT_CONTEXT.md            # 完整开发记录
└── README.md
```

## PCB V1

- **方案**：模块化载板（STM32 核心板 + 各模块通过排针/插座接入）
- **EDA**：嘉立创 EDA
- **尺寸**：10cm × 10cm 以内
- **状态**：已完成打样、焊接、主要功能验证 ✅
- 详见 [PROJECT_CONTEXT.md](PROJECT_CONTEXT.md)

## 当前验证状态

| 功能 | 状态 |
|------|------|
| OLED 显示 | ✅ |
| ADD 按键 (PB1) | ✅ |
| CONFIRM/RESET 按键 (PC13) | ✅ |
| 红外计数 (PB14 EXTI) | ✅ |
| 状态机 (IDLE→COUNTING→MOTOR_RUN→DONE) | ✅ |
| 电机 PWM 控制 (软启动) | ✅ |
| LED 指示 (计数/电机) | ✅ |
| USART3 调试日志 | ✅ |
| PCB 逻辑部分验证 | ✅ |
| 电机全系统联合 (规范供电) | 🔲 待接线端子 |

## 调试问题记录摘要

| 问题 | 根因 | 解决 |
|------|------|------|
| PC13 按键无响应 | PC13 虚焊 | 补焊 |
| 红外计数无反应 | R2 不当上拉将 PB14 低电平抬至 ~1.6V (VIH/VIL 不确定区) | R2 空焊，保留 R1+C6 |
| 电机接口焊盘损伤 | 未装接线端子直接焊线 | 等 KF301 到货，第二片 PCB 正式焊接 |
| TB6612 发热异常 | 供电接线不规范 | 暂停电机测试，等规范供电 |

## 后续改进方向

- **PCB V1.1**：LED 改直插、电机接口丝印加大、IR 输入电路优化 (R1/R2/C6 标注)、测试点更明显
- **V2 FreeRTOS**：多任务化 (KeyTask / SensorTask / MotorTask / DisplayTask / DebugTask)
- **V1.5 数码管**：TM1637 / MAX7219 显示扩展
- **参数持久化**：Flash 保存 target/motor_speed 等配置
- **编码器反馈**：速度/位置闭环控制

## 开发环境

- **IDE**：Keil MDK v5
- **库**：STM32F10x Standard Peripheral Library
- **编译器**：ARMCC (C89 mode)
- **调试工具**：ST-Link V2, USB-TTL (USART3 115200)

## 快速开始

1. 克隆仓库
2. 用 Keil MDK 打开 `2-1/project.uvprojx`
3. 编译下载到 STM32F103C8T6
4. 串口助手连接 USART3 (PB10/PB11), 115200 8N1
5. 上电 → OLED 显示 IDLE → ADD 设置目标 → CONFIRM → 遮挡红外 → 电机自动运行

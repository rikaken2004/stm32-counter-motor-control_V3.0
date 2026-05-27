# PROJECT CONTEXT — STM32 计数器项目

## 项目概述

基于 STM32F103 的计数/定长控制系统。传感器脉冲计数，达到目标值后电机自动停止，冷却后自动重启，循环运行。

## 目录结构

```
STM32_COUNTER_PROJECT/
└── 2-1/                          # Keil MDK 工程目录
    ├── project.uvprojx           # Keil 工程文件
    ├── user/                     # 应用层
    │   ├── main.c                # 入口
    │   ├── app.h / app.c         # 状态机 & 任务调度
    │   ├── app_time.h / app_time.c   # 非阻塞时基 (TIM4)
    │   ├── app_key.h / app_key.c     # 非阻塞按键
    │   ├── stm32f10x_conf.h
    │   └── stm32f10x_it.c/h
    ├── hardware/                 # 外设驱动（不可修改）
    │   ├── OLED.c/h, OLED_Font.h
    │   ├── Motor.c/h
    │   ├── PWM.c/h
    │   ├── LED.c/h, NEW_LED.c/h
    │   ├── Key.c/h
    │   ├── CountSensor.c/h
    │   ├── Encoder.c/h, NEW_Encoder.c/h
    │   ├── IC.c/h
    │   ├── AD.c/h, New_AD.c/h
    │   ├── Buzzer.c/h
    │   ├── LightSensor.c/h
    │   └── Servo.c/h
    ├── system/                   # 系统工具（已存在，应用中未使用）
    │   ├── Delay.c/h
    │   ├── Timer.c/h
    │   ├── InternalTimer.c/h
    │   └── ExternalTimer.c/h
    ├── start/                    # 启动文件 & CMSIS
    └── library/                  # 标准外设库
```

## 当前实现功能

| 功能 | 说明 |
|------|------|
| 非阻塞时基 | TIM4 1ms 中断，`App_Millis()` 提供毫秒时间戳 |
| 非阻塞按键 | PB1 加值, PC13 短按确认/长按复位(>2s), 10ms 采样 + 3 次消抖 |
| 状态机 | IDLE → RUNNING → COOLDOWN → RUNNING 自动循环 |
| OLED 显示 | 100ms 刷新, 4 行: 目标/当前/累计/状态/电机 |
| 电机控制 | PWM 调速 60%, PA6(TIM3_CH1), 方向 PB12/PB13 |
| 计数传感器 | PB14 下降沿中断, delta 差值法消除累积误差 |
| LED 指示 | PA9 每检测到一次计数闪 50ms, PA10 COOLDOWN 时常亮 |

## 新增文件

| 文件 | 作用 |
|------|------|
| `user/app_time.h` | `App_Time_Init()`, `App_Millis()` |
| `user/app_time.c` | TIM4 配置 (PSC=71, ARR=999, 1kHz), `TIM4_IRQHandler` tick 自增 |
| `user/app_key.h` | `AppKeyEvent` 枚举, `App_Key_Init/Update/GetEvent` |
| `user/app_key.c` | 非阻塞状态机: PB1 单次触发, PC13 短/长按, 事件缓存 |
| `user/app.h` | `App_Init()`, `App_Task()` |
| `user/app.c` | 三层状态机 + delta 计数 + OLED 刷新 + LED 指示 |

## 修改文件

| 文件 | 变更 |
|------|------|
| `user/main.c` | 旧 AD 演示替换为 `App_Init()` + `while(1) App_Task()` |
| `project.uvprojx` | 补加 `Motor.c/h` 到 hardware 组; `app_time/app_key/app` 到 user 组 |

## 状态机

```
         ┌──────────────────────────────┐
         │   CONFIRM / RESET            │
         │   (清零, 停电机)              │
         ▼                              │
      ┌──────┐  CONFIRM(target>0)  ┌──────────┐
      │ IDLE │ ──────────────────→ │ RUNNING  │
      │      │                     │ 电机 ON   │
      └──────┘                     └──────────┘
         ▲                              │
         │                              │ current ≥ target
         │                              ▼
         │                        ┌───────────┐
         │   CONFIRM / RESET      │ COOLDOWN  │
         └─────────────────────── │ 电机 OFF   │
                                  │ LED10 ON  │
                                  └───────────┘
                                       │
                                       │ 5s 后
                                       │ 自动重启电机
                                       ▼
                                  ┌──────────┐
                                  │ RUNNING  │  (下一轮)
                                  └──────────┘
```

- **IDLE**: PB1(ADD) 调整 target_count (0~99 循环)，PC13 短按启动 (target>0 时)，PC13 长按清零
- **RUNNING**: 电机 60% 占空比运行，CountSensor delta 累加 current_count，每脉冲 PA9 闪 50ms，达标→COOLDOWN。CONFIRM/RESET 清零回 IDLE
- **COOLDOWN**: 电机停止，LED10 亮，等 5s → 自动清零 current_count 回 RUNNING。CONFIRM/RESET 清零回 IDLE

## 硬件引脚分配

| 模块 | 引脚 | 备注 |
|------|------|------|
| OLED I2C | PB8(SCL), PB9(SDA) | 软件 I2C |
| 电机 PWM | PA6 (TIM3_CH1) | `Motor_Init` 内部调用 `PWM_Init` |
| 电机方向 | PB12, PB13 | 推挽输出, `Motor_SetSpeed` 控制 |
| 计数传感器 | PB14 (EXTI14 下降沿) | 上拉输入, `CountSensor_Init/Get` |
| 按键 ADD | PB1 | 上拉输入, 低电平有效 |
| 按键 CONFIRM | PC13 | 上拉输入, 低电平有效 |
| LED9 | PA9 | 推挽输出, 低电平亮 |
| LED10 | PA10 | 推挽输出, 低电平亮 |
| 时基 | TIM4 | PSC=71, ARR=999, 1ms 中断 |

## 当前限制

1. **CountSensor 需要外部脉冲源** — PB14 无脉冲时计数不增长
2. **App_Task 中 GetEvent 在 Update 之前调用** — 按键事件有 1 帧延迟（非致命）
3. **LED ON/OFF 函数每次重新初始化 GPIO** — 功能正确但低效
4. **system/ 和部分 hardware/ 旧驱动未清理** — 可编译但未使用
5. **无 UART/DMA** — 无串口调试输出
6. **OLED_ShowNum 前导零显示** — `T:01` 而非 `T: 1`

## 下一阶段计划（勿执行）

1. 修复 App_Task 中 Update/GetEvent 调用顺序
2. UART 调试输出（状态、计数、事件日志）
3. 数码管显示方案
4. DMA + ADC 非阻塞模拟量采集
5. 清理未使用的旧驱动文件
6. 嘉立创 EDA 原理图/PCB 设计
7. 参数可配置（电机速度、冷却时间、目标上限）

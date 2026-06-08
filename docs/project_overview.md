# 项目概览

## 项目定位

**STM32 智能计数与电机控制系统** 是基于 STM32F103C8T6 的工业计数/定长控制项目。

原项目为 51 双 MCU 架构（一片管显示+按键，一片管计数+电机），本项目的目标是：

1. 用单颗 STM32F103C8T6 替代双 51 MCU
2. 全部代码非阻塞，基于 TIM4 1ms 时基驱动
3. 模块化载板 PCB 设计，可复现
4. 完整的调试日志系统，方便诊断

## 系统架构

```
┌─────────────────────────────────────────────────┐
│                    app.c                        │
│        4 状态非阻塞状态机 + 任务调度              │
├──────────┬──────────┬──────────┬────────────────┤
│ app_key  │app_time  │uart_debug│   hardware/    │
│ 非阻塞    │ TIM4     │ USART3   │  Motor/OLED/   │
│ 按键      │ 1ms 时基  │ 调试日志  │  CountSensor/  │
│          │          │          │  LED/Key/PWM   │
├──────────┴──────────┴──────────┴────────────────┤
│           STM32F103C8T6 (72MHz)                 │
└─────────────────────────────────────────────────┘
```

## 引脚分配

| 引脚 | 功能 | 配置 | 说明 |
|------|------|------|------|
| PA6 | 电机 PWM | TIM3_CH1, AF_PP | TB6612 PWMA |
| PA9 | LED9 | Out_PP, LOW=ON | 计数闪烁指示 |
| PA10 | LED10 | Out_PP, LOW=ON | 电机运行指示 |
| PB1 | ADD 按键 | IPU | 设置目标值 |
| PB8 | OLED SCL | Out_OD | 软件 I2C |
| PB9 | OLED SDA | Out_OD | 软件 I2C |
| PB10 | USART3 TX | AF_PP | 调试日志 115200 |
| PB11 | USART3 RX | IN_FLOATING | 未使用接收 |
| PB12 | 电机 AIN1 | Out_PP | 方向控制 |
| PB13 | 电机 AIN2 | Out_PP | 方向控制 |
| PB14 | 红外计数 | IPU, EXTI14 | 红外 DO 输入 |
| PC13 | CONFIRM/RESET | IPU | 短按确认/长按复位 |

## 状态机

```
IDLE ──CONFIRM(T>0)──→ COUNTING ──C>=T──→ MOTOR_RUN ──3000ms──→ DONE
  ▲                        │                   │                  │
  │                        │ CONFIRM/RESET     │ CONFIRM/RESET    │ CONFIRM/RESET
  └────────────────────────┴───────────────────┴──────────────────┘
                         (任意状态可 RESET 回 IDLE)
```

## 关键技术点

### 非阻塞时基
- TIM4: PSC=71, ARR=999 → 1kHz (1ms)
- `App_Millis()` 返回 `volatile uint32_t sys_tick_ms`
- 所有延时用差值法：`now - last_xxx >= INTERVAL`

### 按键消抖
- 10ms 采样周期
- 3 次确认消抖（30ms）
- 长按 >2s 产生 RESET 事件
- 单事件 pending 缓存

### 红外计数
- EXTI14 上升沿中断（遮挡=HIGH）
- ISR 中二次读取 GPIO 确认电平
- App 层 120ms 消抖窗口
- 差值法读取 delta（免疫 uint16 溢出）

### 电机控制
- TIM3_CH1 PWM: PSC=719, ARR=99 → ~1kHz
- 软启动: 25% → +5%/100ms → 35%
- 固定运行 3000ms
- Motor_Stop() 统一停机入口

### 调试日志
- USART3 115200 8N1, 阻塞式 TX
- 事件日志：状态转换/按键/IR计数/电机/PB14电平
- PB14 周期性电平输出 (500ms) 用于硬件诊断

## 相关文档

- [bringup_log.md](bringup_log.md) — PCB V1 上电调试记录
- [debug_notes.md](debug_notes.md) — 调试问题详细记录
- [test_record.md](test_record.md) — 功能测试记录
- [interview_summary.md](interview_summary.md) — 面试复习材料

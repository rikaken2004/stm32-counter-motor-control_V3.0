# 面试复习材料 — STM32 计数与电机控制项目

> 适合面试前快速回顾，所有描述与 [2-1/user/app.c](../2-1/user/app.c) 实际代码一致。

## 项目一句话

基于 STM32F103C8T6 的非阻塞状态机计数控制系统，替代原 51 双 MCU 方案。EXTI 红外计数 → PWM 电机控制 → OLED 显示，零 Delay_ms 裸机架构。

## 代码结构

```
user/
├── app.c / app.h           # 4 状态机 + App_Task() 超级循环
├── app_key.c / app_key.h   # 非阻塞按键 (10ms采样, 3次消抖, 长按>2s)
├── app_time.c / app_time.h # TIM4 1ms 时基
└── uart_debug.c / .h       # USART3 阻塞式调试日志
hardware/                   # 外设驱动 (未修改)
├── CountSensor.c/h         # PB14 EXTI 中断计数
├── Motor.c/h + PWM.c/h     # TB6612 PWM 电机控制
├── OLED.c/h                # I2C 128x64 OLED
├── LED.c/h                 # PA9/PA10 LED
└── Key.c/h                 # PB1/PC13 硬件初始化
```

## 状态机

```
IDLE ──CONFIRM──→ COUNTING ──C>=T──→ MOTOR_RUN ──3000ms──→ DONE
  ▲                   │                  │                   │
  └───────────────────┴──────────────────┴───────────────────┘
                   任意状态 CONFIRM/RESET → IDLE
```

## 面试 10 问速答

### Q1: 为什么没有 Delay_ms？
全部用 `App_Millis()` 差值法。TIM4 1ms 中断自增 `sys_tick_ms`。10ms/50ms/100ms/120ms/3s 全部非阻塞。

### Q2: CountSensor uint16 溢出怎么办？
差值法 `raw_delta = now - last`（处理 uint16 回绕），天然免疫溢出。

### Q3: 为什么每次只 `current_count += 1` 而不是 `+= raw_delta`？
电机 EMI 可能导致 raw_delta=20~30 的异常跳变。`+=1` 是保守策略，每 120ms 窗口最多计 1 个。

### Q4: 按键长按怎么防止和短按冲突？
`cf_long_fired` 标志。长按 2s 后立即产生 RESET（不等松手），松手时检测到 `cf_long_fired=1` 则抑制 CONFIRM。

### Q5: PC13 和板载 LED 共存有问题吗？
PC13 是 Blue Pill 板载 LED。配置为 IPU 时 LED 不影响按键——按下到 GND 时 LED 反而会亮，等于自带指示灯。

### Q6: LED ON/OFF 为什么每次重新 Init GPIO？
课程代码的设计问题（未修改 hardware 层）。面试时主动指出：实际项目应直接操作 ODR/BSRR。

### Q7: COOLDOWN→RUNNING 为什么要重新 `CountSensor_Get()`？
冷却期间 ISR 仍在计数，重新快照避免冷却期间的噪声被算入下一轮第一个 delta。

### Q8: 单槽 pending 怎么处理同时多键？
`if (pending == APP_KEY_NONE)` 先到先得。实际使用中不会同时按两键。

### Q9: UART 阻塞发送不会拖慢主循环吗？
115200 波特率下 1 字符约 87μs，一条日志约 1.7ms。对于按键+红外计数场景完全可接受。

### Q10: 移植到 FreeRTOS 怎么拆？
KeyTask(10ms) → Queue → AppTask(状态机) → 发命令给 MotorTask/DisplayTask/DebugTask。各任务独立，事件驱动。

## 主动亮出的 "我知道但没改"

| 问题 | 位置 |
|------|------|
| Motor.c 注释写 PA4/PA5，实际用 PB12/PB13 | hardware/Motor.c |
| Key.c 注释写 PB11，实际是 PC13 | hardware/Key.c |
| LED.c 注释写 PA1/PA2，实际用 PA9/PA10 | hardware/LED.c |
| LED.c ON/OFF 每次重新 Init GPIO | hardware/LED.c |
| NVIC_PriorityGroupConfig 重复调用 | CountSensor.c + app_time.c |

**面试话术**："这些是课程提供的 hardware 层代码。我注意到但不修改——遵循分层原则，改动集中在 user/ 层。"

## 硬件调试亮点

- 1.6V 中间电压导致 STM32 读取错误（V_IL<1.155V, V_IH>2.145V）
- 通过分压计算和实测定位 R2 不当上拉
- 软件 `CountSensor_ReadRawLevel()` + 500ms 周期性串口日志辅助诊断
- 展示了软硬结合排查能力

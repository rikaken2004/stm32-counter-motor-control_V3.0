# 功能测试记录

## 测试环境

- MCU: STM32F103C8T6 (Blue Pill)
- 底板: PCB V1 模块化载板
- 显示: 0.96" OLED I2C
- 红外: 4Pin 模块 (DO 输出)
- 电机驱动: TB6612FNG
- 调试: USART3 115200 8N1

## 基础功能测试

| # | 测试项 | 结果 | 说明 |
|---|--------|------|------|
| 1 | 上电 OLED 显示 | ✅ PASS | 4 行内容清晰: T/C/State/Motor |
| 2 | PB1 ADD 加值 | ✅ PASS | 0→1→...→99→0 循环 |
| 3 | PC13 CONFIRM (短按) | ✅ PASS | IDLE 中 target>0 → COUNTING |
| 4 | PC13 RESET (长按 >2s) | ✅ PASS | 任意状态 → IDLE |
| 5 | 红外计数显示 | ✅ PASS | C 值随遮挡递增 |
| 6 | 计数达标 → 电机启动 | ✅ PASS | C≥T → MOTOR_RUN, 软启动 |
| 7 | 电机 3000ms 后自动停 | ✅ PASS | MOTOR_RUN → DONE |
| 8 | COUNTING 中 RESET | ✅ PASS | 电机不转，回 IDLE |
| 9 | MOTOR_RUN 中 RESET | ✅ PASS | 电机立即停，回 IDLE |
| 10 | DONE 中 CONFIRM | ✅ PASS | 回 IDLE, T/C 清零 |
| 11 | USART3 启动日志 | ✅ PASS | `STM32 counter system boot` |
| 12 | USART3 事件日志 | ✅ PASS | 7 种事件日志正常 |
| 13 | LED9 计数闪烁 | ✅ PASS | 每有效脉冲闪 50ms |
| 14 | LED10 电机指示 | ✅ PASS | MOTOR_RUN 亮, 其余灭 |
| 15 | OLED 状态显示 | ✅ PASS | IDLE/COUNT/MOTOR/DONE |

## 红外计数测试

| # | 测试项 | 结果 | 说明 |
|---|--------|------|------|
| 1 | 未遮挡 PB14 lvl | ✅ 0 | 电平稳定为 0 |
| 2 | 遮挡 PB14 lvl | ✅ 1 | 电平稳定为 1 |
| 3 | target=1 单次计数 | ✅ PASS | 遮挡 1 次 → C=1 → 电机转 |
| 4 | target=3 连续计数 | ✅ PASS | 遮挡 3 次 → C=3 → 电机转 |
| 5 | target=5 连续计数 | ✅ PASS | 遮挡 5 次 → C=5 → 电机转 |
| 6 | 快速遮挡 (消抖测试) | ✅ PASS | 120ms 消抖正常 |

## 电机测试

| # | 测试项 | 结果 | 备注 |
|---|--------|------|------|
| 1 | 软启动 ramp 日志 | ✅ PASS | speed ramp=30, ramp=35 |
| 2 | 电机方向 | ✅ PASS | 正转 |
| 3 | 电机 PWM 频率 | ✅ PASS | ~1kHz |
| 4 | 电机 3000ms 定时 | ✅ PASS | 到时间自动停 |
| 5 | 紧急停止 | ✅ PASS | MOTOR_RUN 中 CONFIRM → 立即停 |
| 6 | 规范供电全系统测试 | 🔲 | 待接线端子 |

## 串口日志样本

```
STM32 counter system boot
PB14 init level=0
PB14 lvl=0 state=0
KEY ADD target=5
STATE IDLE->COUNTING target=5
IR count current=1 target=5 lvl=1
IR count current=2 target=5 lvl=1
IR count current=3 target=5 lvl=1
IR count current=4 target=5 lvl=1
IR count current=5 target=5 lvl=1
STATE COUNTING->MOTOR_RUN
MOTOR RUN START speed=35
MOTOR speed ramp=30
MOTOR speed ramp=35
PB14 lvl=0 state=2
MOTOR RUN STOP
STATE MOTOR_RUN->DONE
RESET to IDLE
```

## 已知未测项

- [ ] 连续 10 轮无复位
- [ ] 连续运行 30 分钟稳定性
- [ ] 电机全系统带载测试
- [ ] 不同 target 值 (1/3/5/10/50/99) 全测

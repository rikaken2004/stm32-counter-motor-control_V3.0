#include "stm32f10x.h"
#include "app_time.h"
#include "app_key.h"
#include "OLED.h"
#include "Motor.h"
#include "LED.h"
#include "CountSensor.h"
#include "uart_debug.h"

typedef enum {
    APP_IDLE = 0,
    APP_COUNTING,
    APP_MOTOR_RUN,
    APP_DONE
} AppState;

/*
 * PWM 调速说明：
 * 当前方案是 PWM 开环调速，本质是改变电机平均驱动电压，
 * 不是电流闭环控制，也不是速度闭环控制。
 * 如需精确电流控制，需要电流采样电阻 + 电流检测电路 + 闭环算法。
 * 如需精确速度控制，需要编码器反馈 + PID 等闭环控制。
 */

// 电机 PWM 占空比 0~100（平均电压控制，非电流/速度闭环）
#define APP_MOTOR_SPEED        35
// 电机固定运行时长
#define APP_MOTOR_RUN_MS     3000
// 红外计数消抖窗口
#define APP_IR_DEBOUNCE_MS    120
// OLED 刷新间隔
#define APP_OLED_REFRESH_MS   100
// LED9 计数闪烁时长
#define APP_LED9_FLASH_MS      50
// 目标值上限
#define APP_TARGET_MAX         99

// 软启动：从 25% 开始，每 100ms +5%，直到 APP_MOTOR_SPEED
#define APP_RAMP_START         25
#define APP_RAMP_STEP           5
#define APP_RAMP_INTERVAL_MS  100

static AppState  state;

static uint8_t   target_count;
static uint16_t  current_count;
static uint32_t  total_count;
static uint16_t  last_sensor_count;
static uint32_t  motor_start_ms;
static uint32_t  oled_last_update_ms;
static uint32_t  led9_on_time_ms;
static uint8_t   led9_is_on;
static uint32_t  last_ir_accept_ms;
static uint32_t  last_status_print_ms;

// 软启动变量
static uint8_t   motor_ramp_speed;
static uint32_t  last_ramp_ms;
static uint8_t   ramp_done;

/*
 * 停止电机并复位软启动状态。
 * 所有需要停机的路径（RESET、取消、超时、回 IDLE/DONE）必须调用此函数。
 */
static void Motor_Stop(void)
{
    Motor_SetSpeed(0);
    motor_ramp_speed = 0;
    ramp_done = 0;
}

/*
 * 统一复位到 IDLE：停电机 / 灭灯 / 清零 / 状态回 IDLE。
 * 本地按键 RESET 和串口 RESET 命令均通过此函数执行核心复位动作。
 */
static void App_ResetToIdle(void)
{
    Motor_Stop();
    LED10_OFF();
    target_count = 0;
    current_count = 0;
    state = APP_IDLE;
}

static void update_display(void)
{
    OLED_ShowString(1, 1, "T:");
    OLED_ShowNum(1, 3, target_count, 2);
    OLED_ShowString(1, 6, "C:");
    OLED_ShowNum(1, 8, current_count, 2);

    OLED_ShowString(2, 1, "Total:");
    OLED_ShowNum(2, 7, total_count, 5);

    OLED_ShowString(3, 1, "State:");
    switch (state) {
    case APP_IDLE:      OLED_ShowString(3, 7, "IDLE "); break;
    case APP_COUNTING:  OLED_ShowString(3, 7, "COUNT"); break;
    case APP_MOTOR_RUN: OLED_ShowString(3, 7, "MOTOR"); break;
    case APP_DONE:      OLED_ShowString(3, 7, "DONE "); break;
    }

    OLED_ShowString(4, 1, "Motor:");
    if (state == APP_MOTOR_RUN)
        OLED_ShowString(4, 7, "ON ");
    else
        OLED_ShowString(4, 7, "OFF");
}

/*
 * 周期性状态行输出（每 500ms），供 Linux/树莓派串口采集解析。
 * 格式: STATE=<状态>,TARGET=<T>,CURRENT=<C>,TOTAL=<总>,MOTOR=<ON|OFF>\r\n
 */
static void print_status_line(void)
{
    UART_Debug_SendString("STATE=");
    switch (state) {
    case APP_IDLE:      UART_Debug_SendString("IDLE");      break;
    case APP_COUNTING:  UART_Debug_SendString("COUNTING");  break;
    case APP_MOTOR_RUN: UART_Debug_SendString("MOTOR_RUN"); break;
    case APP_DONE:      UART_Debug_SendString("DONE");      break;
    }
    UART_Debug_SendString(",TARGET=");
    UART_Debug_SendNum(target_count);
    UART_Debug_SendString(",CURRENT=");
    UART_Debug_SendNum(current_count);
    UART_Debug_SendString(",TOTAL=");
    UART_Debug_SendNum(total_count);
    UART_Debug_SendString(",MOTOR=");
    if (state == APP_MOTOR_RUN)
        UART_Debug_SendString("ON");
    else
        UART_Debug_SendString("OFF");
    UART_Debug_SendString("\r\n");
}

void App_Init(void)
{
    App_Time_Init();
    UART_Debug_Init(115200);
    UART_Debug_SendString("STM32 counter system boot\r\n");
    OLED_Init();
    Motor_Init();
    LED_Init();
    CountSensor_Init();
    App_Key_Init();

    state = APP_IDLE;
    target_count = 0;
    current_count = 0;
    total_count = 0;
    last_sensor_count = 0;
    motor_start_ms = 0;
    oled_last_update_ms = 0;
    led9_on_time_ms = 0;
    led9_is_on = 0;
    last_ir_accept_ms = 0;
    last_status_print_ms = 0;
    motor_ramp_speed = 0;
    last_ramp_ms = 0;
    ramp_done = 0;

    UART_Debug_SendString("PB14 init level=");
    UART_Debug_SendNum(CountSensor_ReadRawLevel());
    UART_Debug_SendString("\r\n");

    Motor_Stop();
    LED9_OFF();
    LED10_OFF();
    OLED_Clear();

    update_display();
}

void App_Task(void)
{
    AppKeyEvent key;
    uint32_t now;
    uint8_t  refresh;

    App_Key_Update();
    key = App_Key_GetEvent();
    now = App_Millis();
    refresh = 0;

    // LED9: 计数指示，50ms 自动灭
    if (led9_is_on && (now - led9_on_time_ms >= APP_LED9_FLASH_MS)) {
        LED9_OFF();
        led9_is_on = 0;
    }

    // 周期性状态行输出（每 500ms），供 Linux/树莓派串口采集
    if (now - last_status_print_ms >= 500) {
        print_status_line();
        last_status_print_ms = now;
    }

    // 处理 ESP32 串口 RESET 命令（由 USART3 RX 中断置位）
    if (UART_Debug_GetResetRequest()) {
        UART_Debug_ClearResetRequest();
        App_ResetToIdle();
        UART_Debug_SendString("CMD RESET OK\r\n");
        refresh = 1;
    }

    switch (state)
    {

    // ============================================================
    // IDLE: 电机停止，等待 ADD 设置目标 + CONFIRM 开始计数
    // ============================================================
    case APP_IDLE:
        if (key == APP_KEY_ADD) {
            target_count++;
            if (target_count > APP_TARGET_MAX)
                target_count = 0;
            UART_Debug_SendString("KEY ADD target=");
            UART_Debug_SendNum(target_count);
            UART_Debug_SendString("\r\n");
            refresh = 1;
        }
        else if (key == APP_KEY_CONFIRM) {
            if (target_count > 0) {
                current_count = 0;
                last_sensor_count = CountSensor_Get();
                last_ir_accept_ms = now;
                state = APP_COUNTING;
                UART_Debug_SendString("STATE IDLE->COUNTING target=");
                UART_Debug_SendNum(target_count);
                UART_Debug_SendString("\r\n");
                refresh = 1;
            }
        }
        else if (key == APP_KEY_RESET) {
            target_count = 0;
            current_count = 0;
            refresh = 1;
        }
        break;

    // ============================================================
    // COUNTING: 电机保持停止，红外触发 current_count++，
    //           达到 target 后进入 MOTOR_RUN
    // ============================================================
    case APP_COUNTING:
        if (key == APP_KEY_CONFIRM || key == APP_KEY_RESET) {
            App_ResetToIdle();
            UART_Debug_SendString("RESET to IDLE\r\n");
            refresh = 1;
            break;
        }
        {
            uint16_t now_sensor;
            uint16_t raw_delta;

            now_sensor = CountSensor_Get();
            if (now_sensor >= last_sensor_count)
                raw_delta = now_sensor - last_sensor_count;
            else
                raw_delta = (0xFFFF - last_sensor_count) + now_sensor + 1;
            last_sensor_count = now_sensor;

            if (raw_delta > 0) {
                if (now - last_ir_accept_ms >= APP_IR_DEBOUNCE_MS) {
                    current_count += 1;
                    last_ir_accept_ms = now;
                    UART_Debug_SendString("IR count current=");
                    UART_Debug_SendNum(current_count);
                    UART_Debug_SendString(" target=");
                    UART_Debug_SendNum(target_count);
                    UART_Debug_SendString(" lvl=");
                    UART_Debug_SendNum(CountSensor_ReadRawLevel());
                    UART_Debug_SendString("\r\n");
                    LED9_ON();
                    led9_is_on = 1;
                    led9_on_time_ms = now;
                    refresh = 1;

                    if (current_count >= target_count) {
                        total_count += current_count;
                        motor_start_ms = now;
                        // 软启动：从 APP_RAMP_START 开始
                        motor_ramp_speed = APP_RAMP_START;
                        if (motor_ramp_speed > APP_MOTOR_SPEED)
                            motor_ramp_speed = APP_MOTOR_SPEED;
                        Motor_SetSpeed((int8_t)motor_ramp_speed);
                        last_ramp_ms = now;
                        ramp_done = (motor_ramp_speed >= APP_MOTOR_SPEED);
                        LED10_ON();
                        state = APP_MOTOR_RUN;
                        UART_Debug_SendString("STATE COUNTING->MOTOR_RUN\r\n");
                        UART_Debug_SendString("MOTOR RUN START speed=");
                        UART_Debug_SendNum(APP_MOTOR_SPEED);
                        UART_Debug_SendString("\r\n");
                        refresh = 1;
                    }
                }
            }
        }
        break;

    // ============================================================
    // MOTOR_RUN: 电机运行（软启动 + 固定时长），LED10 亮
    //            RESET/CONFIRM 紧急停止回 IDLE
    // ============================================================
    case APP_MOTOR_RUN:
        if (key == APP_KEY_CONFIRM || key == APP_KEY_RESET) {
            App_ResetToIdle();
            UART_Debug_SendString("RESET to IDLE\r\n");
            refresh = 1;
            break;
        }

        // 软启动 ramp: 每 100ms +5%，直到 APP_MOTOR_SPEED
        if (!ramp_done && (now - last_ramp_ms >= APP_RAMP_INTERVAL_MS)) {
            motor_ramp_speed += APP_RAMP_STEP;
            if (motor_ramp_speed >= APP_MOTOR_SPEED) {
                motor_ramp_speed = APP_MOTOR_SPEED;
                ramp_done = 1;
            }
            Motor_SetSpeed((int8_t)motor_ramp_speed);
            last_ramp_ms = now;
            UART_Debug_SendString("MOTOR speed ramp=");
            UART_Debug_SendNum(motor_ramp_speed);
            UART_Debug_SendString("\r\n");
        }

        // 运行时间到 → 停机 → DONE
        if (now - motor_start_ms >= APP_MOTOR_RUN_MS) {
            Motor_Stop();
            LED10_OFF();
            state = APP_DONE;
            UART_Debug_SendString("MOTOR RUN STOP\r\n");
            UART_Debug_SendString("STATE MOTOR_RUN->DONE\r\n");
            refresh = 1;
        }
        break;

    // ============================================================
    // DONE: 本轮完成，电机已停，等待下一轮
    // ============================================================
    case APP_DONE:
        if (key == APP_KEY_CONFIRM || key == APP_KEY_RESET) {
            App_ResetToIdle();
            UART_Debug_SendString("RESET to IDLE\r\n");
            refresh = 1;
        }
        break;
    }

    // OLED 刷新：事件触发 或 100ms 定时
    if (refresh || (now - oled_last_update_ms >= APP_OLED_REFRESH_MS)) {
        update_display();
        oled_last_update_ms = now;
    }
}

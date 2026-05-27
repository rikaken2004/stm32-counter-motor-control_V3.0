#include "stm32f10x.h"
#include "app_time.h"
#include "app_key.h"
#include "OLED.h"
#include "Motor.h"
#include "LED.h"
#include "CountSensor.h"

typedef enum {
    APP_IDLE = 0,
    APP_RUNNING,
    APP_COOLDOWN
} AppState;

#define APP_MOTOR_SPEED      60
#define APP_COOLDOWN_MS    5000
#define APP_OLED_REFRESH_MS  100
#define APP_LED9_FLASH_MS    50
#define APP_TARGET_MAX       99

static AppState  state;

static uint8_t   target_count;
static uint16_t  current_count;
static uint32_t  total_count;
static uint16_t  last_sensor_count;
static uint32_t  cooldown_start_ms;
static uint32_t  oled_last_update_ms;
static uint32_t  led9_on_time_ms;
static uint8_t   led9_is_on;

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
    case APP_IDLE:     OLED_ShowString(3, 7, "IDLE "); break;
    case APP_RUNNING:  OLED_ShowString(3, 7, "RUN  "); break;
    case APP_COOLDOWN: OLED_ShowString(3, 7, "COOL "); break;
    }

    OLED_ShowString(4, 1, "Motor:");
    if (state == APP_RUNNING)
        OLED_ShowString(4, 7, "ON ");
    else
        OLED_ShowString(4, 7, "OFF");
}

void App_Init(void)
{
    App_Time_Init();
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
    cooldown_start_ms = 0;
    oled_last_update_ms = 0;
    led9_on_time_ms = 0;
    led9_is_on = 0;

    Motor_SetSpeed(0);
    LED9_OFF();
    LED10_OFF();
    OLED_Clear();

    update_display();
}

void App_Task(void)
{
	AppKeyEvent key = App_Key_GetEvent();
    uint32_t now = App_Millis();
    uint8_t  refresh = 0;

    App_Key_Update();
   

    // LED9 auto-off after flash duration
    if (led9_is_on && (now - led9_on_time_ms >= APP_LED9_FLASH_MS)) {
        LED9_OFF();
        led9_is_on = 0;
    }

    switch (state)
    {
    case APP_IDLE:
        if (key == APP_KEY_ADD) {
            target_count++;
            if (target_count > APP_TARGET_MAX)
                target_count = 0;
            refresh = 1;
        }
        else if (key == APP_KEY_CONFIRM) {
            if (target_count > 0) {
                current_count = 0;
                last_sensor_count = CountSensor_Get();
                LED10_OFF();
                Motor_SetSpeed(APP_MOTOR_SPEED);
                state = APP_RUNNING;
                refresh = 1;
            }
        }
        else if (key == APP_KEY_RESET) {
            target_count = 0;
            current_count = 0;
            refresh = 1;
        }
        break;

    case APP_RUNNING:
        if (key == APP_KEY_CONFIRM || key == APP_KEY_RESET) {
            Motor_SetSpeed(0);
            LED10_OFF();
            target_count = 0;
            current_count = 0;
            state = APP_IDLE;
            refresh = 1;
            break;
        }
        {
            uint16_t now_sensor = CountSensor_Get();
            uint16_t delta;
            if (now_sensor >= last_sensor_count)
                delta = now_sensor - last_sensor_count;
            else
                delta = (0xFFFF - last_sensor_count) + now_sensor + 1;
            last_sensor_count = now_sensor;

            if (delta > 0) {
                current_count += delta;
                LED9_ON();
                led9_is_on = 1;
                led9_on_time_ms = now;
                refresh = 1;

                if (current_count >= target_count) {
                    Motor_SetSpeed(0);
                    LED10_ON();
                    total_count += current_count;
                    cooldown_start_ms = now;
                    state = APP_COOLDOWN;
                }
            }
        }
        break;

    case APP_COOLDOWN:
        if (key == APP_KEY_CONFIRM || key == APP_KEY_RESET) {
            Motor_SetSpeed(0);
            LED10_OFF();
            target_count = 0;
            current_count = 0;
            state = APP_IDLE;
            refresh = 1;
            break;
        }
        if (now - cooldown_start_ms >= APP_COOLDOWN_MS) {
            current_count = 0;
            last_sensor_count = CountSensor_Get();
            LED10_OFF();
            Motor_SetSpeed(APP_MOTOR_SPEED);
            state = APP_RUNNING;
            refresh = 1;
        }
        break;
    }

    if (refresh || (now - oled_last_update_ms >= APP_OLED_REFRESH_MS)) {
        update_display();
        oled_last_update_ms = now;
    }
}

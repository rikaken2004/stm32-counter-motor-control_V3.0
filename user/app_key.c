#include "app_key.h"
#include "app_time.h"
#include "Key.h"

// Key states
typedef enum {
    KS_IDLE = 0,
    KS_DEBOUNCE_DOWN,
    KS_HOLD,
    KS_DEBOUNCE_UP
} KeyState;

#define SAMPLE_MS       10
#define DEBOUNCE_CNT     3
#define LONG_PRESS_MS 2000

// ADD key (PB1)
static KeyState  add_state;
static uint8_t   add_cnt;

// CONFIRM/RESET key (PC13)
static KeyState  cf_state;
static uint8_t   cf_cnt;
static uint32_t  cf_press_start;
static uint8_t   cf_long_fired;

static uint32_t  last_tick;
static AppKeyEvent pending;

void App_Key_Init(void)
{
    Key_Init();

    add_state = KS_IDLE;
    add_cnt = 0;

    cf_state = KS_IDLE;
    cf_cnt = 0;
    cf_press_start = 0;
    cf_long_fired = 0;

    last_tick = 0;
    pending = APP_KEY_NONE;
}

void App_Key_Update(void)
{
	uint8_t add_lvl = (GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1) == Bit_RESET) ? 0 : 1;
    uint8_t cf_lvl  = (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_13) == Bit_RESET) ? 0 : 1;
    uint32_t now = App_Millis();
    if (now - last_tick < SAMPLE_MS)
        return;
    last_tick = now;

   

    // ---- ADD key (PB1): single-shot on press ----
    switch (add_state)
    {
    case KS_IDLE:
        if (add_lvl == 0) { add_cnt = 1; add_state = KS_DEBOUNCE_DOWN; }
        break;

    case KS_DEBOUNCE_DOWN:
        if (add_lvl == 0) {
            if (++add_cnt >= DEBOUNCE_CNT) {
                add_state = KS_HOLD;
                if (pending == APP_KEY_NONE)
                    pending = APP_KEY_ADD;
            }
        } else {
            add_state = KS_IDLE;
        }
        break;

    case KS_HOLD:
        if (add_lvl == 1) { add_cnt = 1; add_state = KS_DEBOUNCE_UP; }
        break;

    case KS_DEBOUNCE_UP:
        if (add_lvl == 1) {
            if (++add_cnt >= DEBOUNCE_CNT)
                add_state = KS_IDLE;
        } else {
            add_state = KS_HOLD;
        }
        break;
    }

    // ---- CONFIRM/RESET key (PC13): short / long press ----
    switch (cf_state)
    {
    case KS_IDLE:
        if (cf_lvl == 0) { cf_cnt = 1; cf_state = KS_DEBOUNCE_DOWN; }
        break;

    case KS_DEBOUNCE_DOWN:
        if (cf_lvl == 0) {
            if (++cf_cnt >= DEBOUNCE_CNT) {
                cf_state = KS_HOLD;
                cf_press_start = now;
                cf_long_fired = 0;
            }
        } else {
            cf_state = KS_IDLE;
        }
        break;

    case KS_HOLD:
        if (!cf_long_fired && (now - cf_press_start >= LONG_PRESS_MS)) {
            cf_long_fired = 1;
            if (pending == APP_KEY_NONE)
                pending = APP_KEY_RESET;
        }
        if (cf_lvl == 1) {
            if (!cf_long_fired) {
                if (pending == APP_KEY_NONE)
                    pending = APP_KEY_CONFIRM;
            }
            cf_cnt = 1;
            cf_state = KS_DEBOUNCE_UP;
        }
        break;

    case KS_DEBOUNCE_UP:
        if (cf_lvl == 1) {
            if (++cf_cnt >= DEBOUNCE_CNT)
                cf_state = KS_IDLE;
        } else {
            cf_state = KS_HOLD;
        }
        break;
    }
}

AppKeyEvent App_Key_GetEvent(void)
{
    AppKeyEvent e = pending;
    pending = APP_KEY_NONE;
    return e;
}

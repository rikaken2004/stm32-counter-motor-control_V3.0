#ifndef __APP_KEY_H
#define __APP_KEY_H

#include "stm32f10x.h"

typedef enum
{
APP_KEY_NONE = 0,
APP_KEY_ADD,
APP_KEY_CONFIRM,
APP_KEY_RESET
} AppKeyEvent;

void App_Key_Init(void);
void App_Key_Update(void);
AppKeyEvent App_Key_GetEvent(void);

#endif

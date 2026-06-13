#ifndef __UART_DEBUG_H
#define __UART_DEBUG_H

#include "stm32f10x.h"

void UART_Debug_Init(uint32_t baudrate);
void UART_Debug_SendChar(char c);
void UART_Debug_SendString(const char *str);
void UART_Debug_SendNum(uint32_t num);

uint8_t UART_Debug_GetResetRequest(void);
void    UART_Debug_ClearResetRequest(void);

#endif

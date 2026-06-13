#include "uart_debug.h"

/* ── UART RX 命令接收缓冲区 ── */
#define RX_BUF_SIZE  32
static char     rx_buf[RX_BUF_SIZE];
static uint8_t  rx_idx;
static volatile uint8_t uart_reset_request;

/* 返回 1 表示有 RESET 命令待处理，由 App_Task 主循环消费 */
uint8_t UART_Debug_GetResetRequest(void)
{
    return uart_reset_request;
}

void UART_Debug_ClearResetRequest(void)
{
    uart_reset_request = 0;
}

void UART_Debug_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    // PB10 = USART3_TX, alternate function push-pull
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // PB11 = USART3_RX, input floating
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_Init(USART3, &USART_InitStructure);

    /* ── 使能 RXNE 中断 ── */
    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    /* ── NVIC 配置 USART3 中断 ── */
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_Init(&NVIC_InitStructure);

    /* ── 缓冲区 & 标志初始化 ── */
    rx_idx = 0;
    uart_reset_request = 0;

    USART_Cmd(USART3, ENABLE);
}

void UART_Debug_SendChar(char c)
{
    while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET);
    USART_SendData(USART3, c);
}

void UART_Debug_SendString(const char *str)
{
    while (*str) {
        UART_Debug_SendChar(*str++);
    }
}

void UART_Debug_SendNum(uint32_t num)
{
    char buf[11];
    uint8_t i = 0;

    if (num == 0) {
        UART_Debug_SendChar('0');
        return;
    }

    while (num > 0 && i < 10) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }

    while (i > 0) {
        UART_Debug_SendChar(buf[--i]);
    }
}

/* ── USART3 接收中断 ── */
void USART3_IRQHandler(void)
{
    char c;

    if (USART_GetITStatus(USART3, USART_IT_RXNE) == SET)
    {
        c = (char)USART_ReceiveData(USART3);

        /* 忽略 '\r'，只在遇到 '\n' 时判断命令 */
        if (c == '\r') {
            return;
        }

        if (c == '\n') {
            rx_buf[rx_idx] = '\0';          // 封成 C 字符串

            /* 判断命令（仅支持 RESET） */
            if (rx_buf[0] == 'R' && rx_buf[1] == 'E' &&
                rx_buf[2] == 'S' && rx_buf[3] == 'E' &&
                rx_buf[4] == 'T' && rx_buf[5] == '\0') {
                uart_reset_request = 1;     // 只设标志，不做复杂操作
            }

            rx_idx = 0;                     // 准备收下一行
            return;
        }

        /* 普通字符存入缓冲区（防溢出） */
        if (rx_idx < RX_BUF_SIZE - 1) {
            rx_buf[rx_idx++] = c;
        }
        /* 溢出则丢弃当前行，等 '\n' 重置 */
    }
}

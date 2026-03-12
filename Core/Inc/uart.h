#ifndef UART_H_
#define UART_H_

#include "stm32f446xx.h"
#include <stdbool.h>
#include <stdint.h>

extern volatile bool needs_log;
void USART2_init(void);
void USART2_DMA_init(void);
static void USART2_send_byte(const char byte);
void USART2_log(const char *str);
void USART2_Printf(const char *format, ...);
static void USART2_Send_DMA(uint8_t *data, uint16_t size);
void USART2_DMA_Printf(const char *format, ...);

#endif
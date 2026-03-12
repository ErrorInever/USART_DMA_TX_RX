#ifndef DMA_H_
#define DMA_H_
/*
DMA2 connected to the fast buses (APB2) and has direct access to memory via the bus matrix. 
Only DMA2 can do Memory-to-Memory transfers.
*/
#include <stdint.h>

#define RX_BUF_SIZE 10
extern uint8_t rx_buffer_0[RX_BUF_SIZE];    // DMA RX buffer 1
extern uint8_t rx_buffer_1[RX_BUF_SIZE];    // DMA RX buffer 2

extern uint32_t buf0_count;
extern uint32_t buf1_count;
extern uint32_t idle_count;

void DMA2_init(const uint32_t srcAddr, const uint32_t dstAddr, const uint16_t size);
void DMA1_UART_TX_init(void);
void DMA1_UART_RX_init(uint8_t *buffer_0, uint8_t *buffer_1, const uint16_t size);

#endif

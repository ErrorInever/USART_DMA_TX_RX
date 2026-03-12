#include "dma.h"
#include "main.h"
#include "stm32f446xx.h"
#include <stdint.h>

// for test double buffer
uint32_t buf0_count = 0;
uint32_t buf1_count = 0;
uint32_t idle_count = 0;


uint8_t rx_buffer_0[RX_BUF_SIZE] = {0};
uint8_t rx_buffer_1[RX_BUF_SIZE] = {0};

void DMA2_init(const uint32_t srcAddr, const uint32_t dstAddr, const uint16_t size) {
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;
    (void)RCC->AHB1ENR;
    // Reset
    DMA2_Stream0->CR = 0;                           // reset all
    while(DMA2_Stream0->CR & DMA_SxCR_EN);          // wait reset
    // Addresses
    DMA2_Stream0->PAR = srcAddr;                    // source (in M2M mode it's Peripheral)
    DMA2_Stream0->M0AR = dstAddr;                   // receiver 
    DMA2_Stream0->NDTR = size;                      // number of data to copy
    // CR setup
    DMA2_Stream0->CR |= (2 << DMA_SxCR_DIR_Pos);    // mode Memory-to-Memory
    DMA2_Stream0->CR |= (2 << DMA_SxCR_MSIZE_Pos);  // Memory 32-bit for speed
    DMA2_Stream0->CR |= (2 << DMA_SxCR_PSIZE_Pos);  // Peripheral 32-bit 
    // enable increment
    DMA2_Stream0->CR |= DMA_SxCR_PINC;               // fixed peripheral increment
    DMA2_Stream0->CR |= DMA_SxCR_MINC;               // fixed memory increment
 
    DMA2_Stream0->CR |= (3 << DMA_SxCR_PL_Pos);     // priority very high
}

void DMA1_UART_TX_init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    (void)RCC->AHB1ENR;
    // Reset
    DMA1_Stream6->CR = 0;
    while(DMA1_Stream6->CR & DMA_SxCR_EN);
    // Addresses
    DMA1_Stream6->PAR = (uint32_t)&(USART2->DR);    // set the address of the peripheral (where we are connecting)
    // CR setup
    DMA1_Stream6->CR |= (4 << DMA_SxCR_CHSEL_Pos);  // select 4 channel (USART2_TX)
    DMA1_Stream6->CR |= (1 << DMA_SxCR_DIR_Pos);    // Direction: Memory-to-peripheral (01)
    // Increment: memory yes, peripheral no.
    DMA1_Stream6->CR |= DMA_SxCR_MINC;
    DMA1_Stream6->CR &= ~DMA_SxCR_PINC;
    // Data size: 8 bit for both sides, because UART 1 byte
    DMA1_Stream6->CR &= ~(DMA_SxCR_MSIZE_Msk | DMA_SxCR_PSIZE_Msk);
    // priority: middle
    DMA1_Stream6->CR |= (1 << DMA_SxCR_PL_Pos);
    // FIFO direct mode (optional)
    DMA1_Stream6->FCR &= ~DMA_SxFCR_DMDIS;
}

void DMA1_UART_RX_init(uint8_t *buffer_0, uint8_t *buffer_1, const uint16_t size) {
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    (void)RCC->AHB1ENR;
    
    DMA1_Stream5->CR = 0;
    while(DMA1_Stream5->CR & DMA_SxCR_EN);
    // Addresses
    DMA1_Stream5->CR |= DMA_SxCR_DBM;               // enable double buffer mode
    DMA1_Stream5->PAR = (uint32_t)&(USART2->DR);    // peripheral addr (from)
    DMA1_Stream5->M0AR = (uint32_t)buffer_0;        // RAM addr 1 (to)
    DMA1_Stream5->M1AR = (uint32_t)buffer_1;        // RAM addr 2 (to)
    DMA1_Stream5->NDTR = size;
    // CR setup: DIR default, PINC default
    DMA1_Stream5->CR |= (4 << DMA_SxCR_CHSEL_Pos);  // select 4 channel (USART2_RX)
    DMA1_Stream5->CR |= DMA_SxCR_MINC;              // addr array increase
    DMA1_Stream5->CR |= DMA_SxCR_CIRC;              // enable circular mode

    DMA1_Stream5->CR |= (1 << DMA_SxCR_PL_Pos);     // set priority

    DMA1_Stream5->CR |= DMA_SxCR_EN;                // enable
    DMA1_Stream5->CR |= DMA_SxCR_TCIE;              // enable interrupt TC

    NVIC_SetPriority(DMA1_Stream5_IRQn, 1); 
    NVIC_EnableIRQ(DMA1_Stream5_IRQn);
}
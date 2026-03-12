#include "uart.h"
#include "stm32f446xx.h"
#include "ringbuffer.h"
#include "dma.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>

volatile bool needs_log = false;

#define UART_TX_BUF_SIZE 128
static uint8_t tx_storage[UART_TX_BUF_SIZE];
static GenericCB  uart_tx_cb; // ring buffer

#define APB1_SPEED 0x16D  // bus speed 42Mhz
#define AF7 0x07


void USART2_init(void) {
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN; // RCC usart2
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN; // RCC port A
  (void)RCC->APB1ENR;
  // init ring buffer
  cb_init(&uart_tx_cb, tx_storage, sizeof(uint8_t), 
  UART_TX_BUF_SIZE, true);
  // Set mode AF for pa2 and pa3
  GPIOA->MODER &= ~GPIO_MODER_MODER2_Msk;
  GPIOA->MODER &= ~GPIO_MODER_MODER3_Msk;
  GPIOA->MODER |= GPIO_MODER_MODER2_1;
  GPIOA->MODER |= GPIO_MODER_MODER3_1;
  // Set AF USART2 for pa2 and pa3
  GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL2_Msk;
  GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL3_Msk;
  GPIOA->AFR[0] |= (AF7 << GPIO_AFRL_AFSEL2_Pos);
  GPIOA->AFR[0] |= (AF7 << GPIO_AFRL_AFSEL3_Pos);

  USART2->BRR = APB1_SPEED; // Default UART mode, OVER8=0
  USART2->CR1 |= USART_CR1_TE |         // Enable transmitter
                  USART_CR1_RE |        // Enable receiver
                  USART_CR1_RXNEIE |    // Enable interrupt on receive
                  USART_CR1_UE;         // Enable USART
  
  // add usart2 interrupt and set priority to NVIC
  NVIC_SetPriority(USART2_IRQn, 2);
  NVIC_EnableIRQ(USART2_IRQn);
}

void USART2_DMA_init(void) {
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;     // RCC usart2
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;      // RCC port A
  (void)RCC->APB1ENR;
  // init ring buffer
  cb_init(&uart_tx_cb, tx_storage, sizeof(uint8_t), 
  UART_TX_BUF_SIZE, true);
  // Set mode AF for pa2 and pa3
  GPIOA->MODER &= ~GPIO_MODER_MODER2_Msk;
  GPIOA->MODER &= ~GPIO_MODER_MODER3_Msk;
  GPIOA->MODER |= GPIO_MODER_MODER2_1;
  GPIOA->MODER |= GPIO_MODER_MODER3_1;
  // Set AF USART2 for pa2 and pa3
  GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL2_Msk;
  GPIOA->AFR[0] &= ~GPIO_AFRL_AFSEL3_Msk;
  GPIOA->AFR[0] |= (AF7 << GPIO_AFRL_AFSEL2_Pos);
  GPIOA->AFR[0] |= (AF7 << GPIO_AFRL_AFSEL3_Pos);

  USART2->BRR = APB1_SPEED;               // Default UART mode, OVER8=0
  USART2->CR1 |=  USART_CR1_TE |          // Enable transmitter
                  USART_CR1_RE |          // Enable receiver
                  USART_CR1_IDLEIE |      // Enable IDLE line interrupt
                  USART_CR1_UE;           // Enable USART

  
  USART2->CR3 |= USART_CR3_DMAR;          // Enable DMA receiver
  USART2->CR3 |= USART_CR3_DMAT;          // enable DMA

  NVIC_SetPriority(USART2_IRQn, 0);
  NVIC_EnableIRQ(USART2_IRQn);
}


void USART2_IRQHandler(void) {
  // DMA IDLE line detection
  if(USART2->SR & USART_SR_IDLE) {
    // For reset flag IDLE need read SR and DR
    volatile uint32_t temp;
    temp = USART2->SR;
    temp = USART2->DR;
    (void)temp;

    idle_count++; // test interrupt

    uint8_t *current_buffer;
    // If CT == 0, then the DMA is currently writing to rx_buffer_0.
    // The data for the current (short) packet is also located there.
    if((DMA1_Stream5->CR & DMA_SxCR_CT) == 0) {
      current_buffer = rx_buffer_0;
    } else {
      current_buffer = rx_buffer_1;
    }
    uint16_t recieved_count = RX_BUF_SIZE - DMA1_Stream5->NDTR; // count byte received

    if(recieved_count > 0) {
      USART2_DMA_Printf("USART2 DMA RX received %d byte: %.*s\r\n", recieved_count, recieved_count, current_buffer);
    }

    // Need reset DMA if circular mode disable
    // If use DMA will automatically reset the NDTR to RX_BUF_SIZE, but only when it reaches the very end (to 0).
    // To ensure that each new package starts from scratch, it's best to restart it manually:
    
    // disable for double buffer
    // DMA1_Stream5->CR &= ~DMA_SxCR_EN;
    // while(DMA1_Stream5->CR & DMA_SxCR_EN);
    // DMA1_Stream5->NDTR = RX_BUF_SIZE;
    // DMA1_Stream5->CR |= DMA_SxCR_EN;
  }
}

void DMA1_Stream5_IRQHandler(void) {
    // Rule for DMA interrupt: Process the buffer that the CT bit does NOT point to.
    // DMA TC (Transfer Complete) interrupt
    if (DMA1->HISR & DMA_HISR_TCIF5) {
        // reset interrupt flag
        DMA1->HIFCR |= DMA_HIFCR_CTCIF5;
        // If CT = 1 now, then DMA is writing to buf1. This means buf0 has JUST filled up!
        if ((DMA1_Stream5->CR & DMA_SxCR_CT) == 0) {
            // DMA is now writing to buf0. This means buf1 just became full. Process buf1.
            buf1_count++;
        } 
        else {
            buf0_count++;
        }
    }
}

// send raw byte
static void USART2_send_byte(const char byte) {
  while(!(USART2->SR & USART_SR_TXE)); // wait TXE becomes 1 i.e. buffer becomes empty
  USART2->DR = byte;
}

// send string
void USART2_log(const char *str) {
  while(*str) {
    cb_push_safe(&uart_tx_cb, (void*)str++);
  }
  // "Kick" the UART TX interrupt to start transmission
  USART2->CR1 |= USART_CR1_TXEIE;
}

/**
 * @brief Formatted print to UART using the circular buffer.
 * @param format String format (like printf)
 * @param ... Arguments
 */
void USART2_Printf(const char *format, ...) {
    char temp_str[128]; // Temporary buffer for formatted string
    va_list args;
    
    va_start(args, format);
    // Formatting the string into our temporary buffer
    int len = vsnprintf(temp_str, sizeof(temp_str), format, args);
    va_end(args);

    if (len > 0) {
        // Push each character into the ring buffer safely
        for (int i = 0; i < len; i++) {
            cb_push_safe(&uart_tx_cb, &temp_str[i]);
        }
        
        // "Kick" the UART TX interrupt to start transmission
        USART2->CR1 |= USART_CR1_TXEIE;
    }
}

static void USART2_Send_DMA(uint8_t *data, uint16_t size) {
  while (DMA1_Stream6->CR & DMA_SxCR_EN); // Is DMA busy?
  // Reset all flags from stream6
  DMA1->HIFCR |= (DMA_HIFCR_CTCIF6 | DMA_HIFCR_CTEIF6 | DMA_HIFCR_CDMEIF6 | DMA_HIFCR_CFEIF6);
  // set new address and size
  DMA1_Stream6->M0AR = (uint32_t)data; 
  DMA1_Stream6->NDTR = size;
  // enable DMA
  DMA1_Stream6->CR |= DMA_SxCR_EN;
}

void USART2_DMA_Printf(const char *format, ...) {
  static char dma_tx_buffer[256]; // for DMA need static buffer
  while (DMA1_Stream6->CR & DMA_SxCR_EN); // We wait until the previous seal hasn't flown into the wires yet.
  // parsing args
  va_list args;
  va_start(args, format);
  // format into DMA
  int len = vsnprintf(dma_tx_buffer, sizeof(dma_tx_buffer), format, args);
  va_end(args);
  // send DMA
  if (len > 0) {
    USART2_Send_DMA((uint8_t *)dma_tx_buffer, (uint16_t)len);
  }
}
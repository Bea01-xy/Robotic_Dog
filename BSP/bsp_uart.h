/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : bsp_uart.h
  * @brief          : The header file of bsp_can.h 
  * @author         : GrassFan Wang
  * @date           : 2025/01/22
  * @version        : v1.0
  ******************************************************************************
  * @attention      : Pay attention to extern the functions and structure
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef BSP_UART_H
#define BSP_UART_H

#ifdef __cplusplus
extern "C" {
#endif

/* Include------------------------------------------------------------------*/
#include "stm32h7xx.h"
#include <stdbool.h>

/* Exported defines ---------------------------------------------------------*/
#define VOFA_MAX_FLOAT_COUNT    10      /* 单次最大发送float数量 */
#define VOFA_FRAME_TAIL         0x7f800000  /* Justfloat协议帧尾 (float: +inf) */

/* Exported types ------------------------------------------------------------*/
typedef struct {
    uint8_t *tx_buf;        /* 发送缓冲区指针 */
    uint16_t buf_size;      /* 缓冲区大小 */
    bool is_busy;           /* DMA发送忙标志 */
} Vofa_HandleTypeDef;

/* Exported functions -------------------------------------------------------*/
extern void BSP_USART_Init(void);

/* 基础VOFA发送函数 (原有，向后兼容) - 使用UART7 */
extern void USART_Vofa_Justfloat_Transmit(float SendValue1, float SendValue2, float SendValue3);

/* 扩展VOFA发送函数 - UART7 (高速921600) */
extern void USART_Vofa_Init(void);
extern bool USART_Vofa_SendFloat(float *data, uint8_t count);
extern bool USART_Vofa_SendFloat_Block(float *data, uint8_t count);
extern bool USART_Vofa_IsBusy(void);

/* UART1 VOFA发送函数 (115200波特率，用于无需裁判系统时的调试) */
extern bool USART1_Vofa_SendFloat(float *data, uint8_t count);
extern bool USART1_Vofa_SendFloat_Block(float *data, uint8_t count);
extern bool USART1_Vofa_IsBusy(void);

/* USART10 VOFA发送函数 (115200波特率，IT中断方式发送，无需DMA) */
extern void USART10_Vofa_Init(void);
extern bool USART10_Vofa_SendFloat(float *data, uint8_t count);
extern bool USART10_Vofa_SendFloat_Block(float *data, uint8_t count);
extern bool USART10_Vofa_IsBusy(void);

#ifdef __cplusplus
}
#endif

#endif
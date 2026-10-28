/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : bsp_uart.c
  * @brief          : bsp uart functions 
  * @author         : GrassFan Wang
  * @date           : 2025/04/27
  * @version        : v1.0
  ******************************************************************************
  * @attention      : Pay attention to init the  BSP_USART_Init functions
  ******************************************************************************
  */
/* USER CODE END Header */

#include "bsp_uart.h"
#include "usart.h"
#include "Remote_Control.h"

static void USER_USART5_RxHandler(UART_HandleTypeDef *huart,uint16_t Size);

static void USART_RxDMA_MultiBuffer_Init(UART_HandleTypeDef *, uint32_t *, uint32_t *, uint32_t );

/* 已废弃: PLL2 时钟不再用于 USART1，USART1 使用 D2PCLK2 (80MHz) */
/* PLL2_ClocksTypeDef PLL2_ClockFreq; */

/**
  * @brief  Configures the USART.
  * @param  None
  * @retval None
  */
void BSP_USART_Init(void){

	/* USART5 is used for the SBUS remote control. */
	USART_RxDMA_MultiBuffer_Init(&huart5,(uint32_t *)SBUS_MultiRx_Buf[0],(uint32_t *)SBUS_MultiRx_Buf[1],SBUS_RX_BUF_NUM);

	/* 初始化VOFA通讯 (UART7用于调试输出) */
	USART_Vofa_Init();

	/* 初始化第二路VOFA输出 (USART10) */
	USART10_Vofa_Init();
}

/**
  * @brief  Init the multi_buffer DMA Transfer with interrupt enabled.
  * @param  huart       pointer to a UART_HandleTypeDef structure that contains
  *                     the configuration information for the specified USART Stream.  
  * @param  SrcAddress pointer to The source memory Buffer address
  * @param  DstAddress pointer to The destination memory Buffer address
  * @param  SecondMemAddress pointer to The second memory Buffer address in case of multi buffer Transfer  
  * @param  DataLength The length of data to be transferred from source to destination
  * @retval none
  */
static void USART_RxDMA_MultiBuffer_Init(UART_HandleTypeDef *huart, uint32_t *DstAddress, uint32_t *SecondMemAddress, uint32_t DataLength){

 huart->ReceptionType = HAL_UART_RECEPTION_TOIDLE;

 huart->RxXferSize    = DataLength * 2;

 SET_BIT(huart->Instance->CR3,USART_CR3_DMAR);

 __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE); 
		
  do{
      __HAL_DMA_DISABLE(huart->hdmarx);
  }while(((DMA_Stream_TypeDef  *)huart->hdmarx->Instance)->CR & DMA_SxCR_EN);

  /* Configure the source memory Buffer address  */
  ((DMA_Stream_TypeDef  *)huart->hdmarx->Instance)->PAR = (uint32_t)&huart->Instance->RDR;

  /* Configure the destination memory Buffer address */
  ((DMA_Stream_TypeDef  *)huart->hdmarx->Instance)->M0AR = (uint32_t)DstAddress;

  /* Configure DMA Stream destination address */
  ((DMA_Stream_TypeDef  *)huart->hdmarx->Instance)->M1AR = (uint32_t)SecondMemAddress;

  /* Configure the length of data to be transferred from source to destination */
  ((DMA_Stream_TypeDef  *)huart->hdmarx->Instance)->NDTR = DataLength;

  /* Enable double memory buffer */
  SET_BIT(((DMA_Stream_TypeDef  *)huart->hdmarx->Instance)->CR, DMA_SxCR_DBM);

  /* Enable DMA */
  __HAL_DMA_ENABLE(huart->hdmarx);	
	
}



/**
  * @brief  USER USART5 Reception Event Callback.(SBUS remote_ctrl)
  * @param  huart UART handle
  * @param  Size  Number of data available in application reception buffer (indicates a position in
  *               reception buffer until which, data are available)
  * @retval None
  */
static void USER_USART5_RxHandler(UART_HandleTypeDef *huart,uint16_t Size){

    /* Current memory buffer used is Memory 0 */
  if(((((DMA_Stream_TypeDef  *)huart->hdmarx->Instance)->CR) & DMA_SxCR_CT ) == RESET){
		
					/* Disable DMA */
					__HAL_DMA_DISABLE(huart->hdmarx);
          
				  /* Switch Memory 0 to Memory 1*/
					((DMA_Stream_TypeDef  *)huart->hdmarx->Instance)->CR |= DMA_SxCR_CT;
					
				  /* Reset the receive count */
					__HAL_DMA_SET_COUNTER(huart->hdmarx,SBUS_RX_BUF_NUM*2);

				  /* Juge whether size is equal to the length of the received data */
					if(Size == SBUS_RX_BUF_NUM)
					{
					
						/* Memory 0 data update to remote_ctrl*/
						SBUS_TO_RC(SBUS_MultiRx_Buf[0],&remote_ctrl);
					
					}
					
			}
			/* Current memory buffer used is Memory 1 */
			else{
					/* Disable DMA */
					__HAL_DMA_DISABLE(huart->hdmarx);
				 
				  /* Switch Memory 1 to Memory 0*/
					((DMA_Stream_TypeDef  *)huart->hdmarx->Instance)->CR &= ~(DMA_SxCR_CT);
				
					/* Reset the receive count */
					__HAL_DMA_SET_COUNTER(huart->hdmarx,SBUS_RX_BUF_NUM*2);

					if(Size == SBUS_RX_BUF_NUM)
					{
						/* Memory 1 to data update to remote_ctrl*/
						SBUS_TO_RC(SBUS_MultiRx_Buf[1],&remote_ctrl);
					}
					
			}
			
}

/**
  * @brief  Reception Event Callback (Rx event notification called after use of advanced reception service).
  * @param  huart UART handle
  * @param  Size  Number of data available in application reception buffer (indicates a position in
  *               reception buffer until which, data are available)
  * @retval None
  */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart,uint16_t Size)
{
    if (huart == &huart5) {
        USER_USART5_RxHandler(huart, Size);

        huart->ReceptionType = HAL_UART_RECEPTION_TOIDLE;
        __HAL_UART_ENABLE_IT(huart, UART_IT_IDLE);
        SET_BIT(huart->Instance->CR3, USART_CR3_DMAR);
        __HAL_DMA_ENABLE(huart->hdmarx);
    }
}

/*============================ VOFA 通讯扩展 =============================*/

/* UART7 VOFA 发送缓冲区 (115200，用于调试输出) */
__attribute__((section(".AXI_SRAM"))) static uint8_t Vofa_TxBuffer[VOFA_MAX_FLOAT_COUNT * 4 + 4];

/* UART1 VOFA 发送缓冲区 (115200，用于裁判系统替代调试) */
__attribute__((section(".AXI_SRAM"))) static uint8_t USART1_Vofa_TxBuffer[VOFA_MAX_FLOAT_COUNT * 4 + 4];

/* USART10 VOFA 发送缓冲区 (115200，第二路调试输出，中断方式) */
__attribute__((section(".AXI_SRAM"))) static uint8_t USART10_Vofa_TxBuffer[VOFA_MAX_FLOAT_COUNT * 4 + 4];

/* VOFA 句柄 */
static Vofa_HandleTypeDef Vofa_Handle = {
    .tx_buf = Vofa_TxBuffer,
    .buf_size = sizeof(Vofa_TxBuffer),
    .is_busy = false
};

static Vofa_HandleTypeDef USART1_Vofa_Handle = {
    .tx_buf = USART1_Vofa_TxBuffer,
    .buf_size = sizeof(USART1_Vofa_TxBuffer),
    .is_busy = false
};

static Vofa_HandleTypeDef USART10_Vofa_Handle = {
    .tx_buf = USART10_Vofa_TxBuffer,
    .buf_size = sizeof(USART10_Vofa_TxBuffer),
    .is_busy = false
};

/**
  * @brief  初始化VOFA通讯 (UART7)
  * @param  None
  * @retval None
  */
void USART_Vofa_Init(void)
{
    Vofa_Handle.is_busy = false;
    USART1_Vofa_Handle.is_busy = false;
    USART10_Vofa_Handle.is_busy = false;
}

/**
  * @brief  非阻塞方式发送float数组到VOFA (使用DMA)
  * @param  data: float数据指针
  * @param  count: float数据数量 (最大VOFA_MAX_FLOAT_COUNT)
  * @retval true: 发送成功启动, false: 发送忙或参数错误
  */
bool USART_Vofa_SendFloat(float *data, uint8_t count)
{
    if (count == 0 || count > VOFA_MAX_FLOAT_COUNT) {
        return false;
    }
    
    if (Vofa_Handle.is_busy) {
        return false;  // DMA正在发送
    }
    
    uint8_t *buf = Vofa_Handle.tx_buf;
    uint8_t *float_ptr;
    
    /* 拷贝float数据 (小端格式) */
    for (uint8_t i = 0; i < count; i++) {
        float_ptr = (uint8_t *)&data[i];
        buf[i * 4 + 0] = float_ptr[0];
        buf[i * 4 + 1] = float_ptr[1];
        buf[i * 4 + 2] = float_ptr[2];
        buf[i * 4 + 3] = float_ptr[3];
    }
    
    /* 添加帧尾 (0x00 0x00 0x80 0x7F = float +inf) */
    buf[count * 4 + 0] = 0x00;
    buf[count * 4 + 1] = 0x00;
    buf[count * 4 + 2] = 0x80;
    buf[count * 4 + 3] = 0x7F;
    
    Vofa_Handle.is_busy = true;
    HAL_UART_Transmit_DMA(&huart7, buf, count * 4 + 4);
    
    return true;
}

/**
  * @brief  阻塞方式发送float数组到VOFA
  * @param  data: float数据指针
  * @param  count: float数据数量 (最大VOFA_MAX_FLOAT_COUNT)
  * @retval true: 发送成功, false: 参数错误
  */
bool USART_Vofa_SendFloat_Block(float *data, uint8_t count)
{
    if (count == 0 || count > VOFA_MAX_FLOAT_COUNT) {
        return false;
    }
    
    uint8_t *buf = Vofa_Handle.tx_buf;
    uint8_t *float_ptr;
    
    /* 拷贝float数据 */
    for (uint8_t i = 0; i < count; i++) {
        float_ptr = (uint8_t *)&data[i];
        buf[i * 4 + 0] = float_ptr[0];
        buf[i * 4 + 1] = float_ptr[1];
        buf[i * 4 + 2] = float_ptr[2];
        buf[i * 4 + 3] = float_ptr[3];
    }
    
    /* 添加帧尾 */
    buf[count * 4 + 0] = 0x00;
    buf[count * 4 + 1] = 0x00;
    buf[count * 4 + 2] = 0x80;
    buf[count * 4 + 3] = 0x7F;
    
    HAL_UART_Transmit(&huart7, buf, count * 4 + 4, 100);
    
    return true;
}

/**
  * @brief  检查VOFA DMA发送是否忙
  * @param  None
  * @retval true: 正在发送, false: 空闲
  */
bool USART_Vofa_IsBusy(void)
{
    return Vofa_Handle.is_busy;
}

/**
  * @brief  UART DMA发送完成回调 (用于清除VOFA忙标志)
  * @param  huart: UART句柄
  * @retval None
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart7) {
        Vofa_Handle.is_busy = false;
    }
    if (huart == &huart1) {
        USART1_Vofa_Handle.is_busy = false;
    }
    if (huart == &huart10) {
        USART10_Vofa_Handle.is_busy = false;
    }
}

/**
  * @brief  原始VOFA发送函数 (向后兼容)
  */
void USART_Vofa_Justfloat_Transmit(float SendValue1, float SendValue2, float SendValue3)
{
    float data[3] = {SendValue1, SendValue2, SendValue3};
    USART_Vofa_SendFloat(data, 3);
}

/*============================ UART1 VOFA 支持 =============================*/

/**
  * @brief  UART1非阻塞方式发送float数组到VOFA (115200波特率)
  * @param  data: float数据指针
  * @param  count: float数据数量 (最大VOFA_MAX_FLOAT_COUNT)
  * @retval true: 发送成功启动, false: 发送忙或参数错误
  */
bool USART1_Vofa_SendFloat(float *data, uint8_t count)
{
    if (count == 0 || count > VOFA_MAX_FLOAT_COUNT) {
        return false;
    }
    
    if (USART1_Vofa_Handle.is_busy) {
        return false;  // DMA正在发送
    }
    
    uint8_t *buf = USART1_Vofa_Handle.tx_buf;
    uint8_t *float_ptr;
    
    /* 拷贝float数据 (小端格式) */
    for (uint8_t i = 0; i < count; i++) {
        float_ptr = (uint8_t *)&data[i];
        buf[i * 4 + 0] = float_ptr[0];
        buf[i * 4 + 1] = float_ptr[1];
        buf[i * 4 + 2] = float_ptr[2];
        buf[i * 4 + 3] = float_ptr[3];
    }
    
    /* 添加帧尾 (0x00 0x00 0x80 0x7F = float +inf) */
    buf[count * 4 + 0] = 0x00;
    buf[count * 4 + 1] = 0x00;
    buf[count * 4 + 2] = 0x80;
    buf[count * 4 + 3] = 0x7F;
    
    USART1_Vofa_Handle.is_busy = true;
    HAL_UART_Transmit_DMA(&huart1, buf, count * 4 + 4);
    
    return true;
}

/**
  * @brief  UART1阻塞方式发送float数组到VOFA (115200波特率)
  * @param  data: float数据指针
  * @param  count: float数据数量 (最大VOFA_MAX_FLOAT_COUNT)
  * @retval true: 发送成功, false: 参数错误
  */
bool USART1_Vofa_SendFloat_Block(float *data, uint8_t count)
{
    if (count == 0 || count > VOFA_MAX_FLOAT_COUNT) {
        return false;
    }
    
    uint8_t *buf = USART1_Vofa_Handle.tx_buf;
    uint8_t *float_ptr;
    
    /* 拷贝float数据 */
    for (uint8_t i = 0; i < count; i++) {
        float_ptr = (uint8_t *)&data[i];
        buf[i * 4 + 0] = float_ptr[0];
        buf[i * 4 + 1] = float_ptr[1];
        buf[i * 4 + 2] = float_ptr[2];
        buf[i * 4 + 3] = float_ptr[3];
    }
    
    /* 添加帧尾 */
    buf[count * 4 + 0] = 0x00;
    buf[count * 4 + 1] = 0x00;
    buf[count * 4 + 2] = 0x80;
    buf[count * 4 + 3] = 0x7F;
    
    HAL_UART_Transmit(&huart1, buf, count * 4 + 4, 100);
    
    return true;
}

/*============================ USART10 VOFA 支持 =============================*/

/**
  * @brief  初始化 USART10 VOFA 通讯 (115200, IT中断发送, 无需 DMA)
  * @param  None
  * @retval None
  */
void USART10_Vofa_Init(void)
{
    USART10_Vofa_Handle.is_busy = false;
}

/**
  * @brief  非阻塞方式发送 float 数组到 VOFA (USART10, 中断发送)
  * @param  data: float数据指针
  * @param  count: float数据数量 (最大VOFA_MAX_FLOAT_COUNT)
  * @retval true: 发送成功启动, false: 发送忙或参数错误
  */
bool USART10_Vofa_SendFloat(float *data, uint8_t count)
{
    if (count == 0 || count > VOFA_MAX_FLOAT_COUNT) {
        return false;
    }

    if (USART10_Vofa_Handle.is_busy) {
        return false;  // 上次中断发送未完成
    }

    uint8_t *buf = USART10_Vofa_Handle.tx_buf;
    uint8_t *float_ptr;

    /* 拷贝float数据 (小端格式) */
    for (uint8_t i = 0; i < count; i++) {
        float_ptr = (uint8_t *)&data[i];
        buf[i * 4 + 0] = float_ptr[0];
        buf[i * 4 + 1] = float_ptr[1];
        buf[i * 4 + 2] = float_ptr[2];
        buf[i * 4 + 3] = float_ptr[3];
    }

    /* 添加帧尾 (0x00 0x00 0x80 0x7F = float +inf) */
    buf[count * 4 + 0] = 0x00;
    buf[count * 4 + 1] = 0x00;
    buf[count * 4 + 2] = 0x80;
    buf[count * 4 + 3] = 0x7F;

    USART10_Vofa_Handle.is_busy = true;
    HAL_UART_Transmit_IT(&huart10, buf, count * 4 + 4);

    return true;
}

/**
  * @brief  阻塞方式发送 float 数组到 VOFA (USART10)
  * @param  data: float数据指针
  * @param  count: float数据数量 (最大VOFA_MAX_FLOAT_COUNT)
  * @retval true: 发送成功, false: 参数错误
  */
bool USART10_Vofa_SendFloat_Block(float *data, uint8_t count)
{
    if (count == 0 || count > VOFA_MAX_FLOAT_COUNT) {
        return false;
    }

    uint8_t *buf = USART10_Vofa_Handle.tx_buf;
    uint8_t *float_ptr;

    /* 拷贝float数据 */
    for (uint8_t i = 0; i < count; i++) {
        float_ptr = (uint8_t *)&data[i];
        buf[i * 4 + 0] = float_ptr[0];
        buf[i * 4 + 1] = float_ptr[1];
        buf[i * 4 + 2] = float_ptr[2];
        buf[i * 4 + 3] = float_ptr[3];
    }

    /* 添加帧尾 */
    buf[count * 4 + 0] = 0x00;
    buf[count * 4 + 1] = 0x00;
    buf[count * 4 + 2] = 0x80;
    buf[count * 4 + 3] = 0x7F;

    HAL_UART_Transmit(&huart10, buf, count * 4 + 4, 100);

    return true;
}

/**
  * @brief  检查 USART10 VOFA 中断发送是否忙
  * @param  None
  * @retval true: 正在发送, false: 空闲
  */
bool USART10_Vofa_IsBusy(void)
{
    return USART10_Vofa_Handle.is_busy;
}


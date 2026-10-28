/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : Buzzer_Task.c
  * @brief          : Buzzer demo task — plays melody once at startup, then idles
  * @author         : Ported from CtrBoard-H7_BUZZER
  ******************************************************************************
  * @attention      : None
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "cmsis_os.h"
#include "Buzzer.h"

/**
  * @brief  Buzzer task: initialise hardware, play demo melody, then idle.
  * @param  argument: Not used
  * @retval None
  */
void Buzzer_Task(void const * argument)
{
    (void)argument;
    Buzzer_Init();

    osDelay(100);  /* 等待 TIM12 稳定输出 */
    /* 播放开机旋律 (阻塞, 内部使用 osDelay — 让出 CPU) */
    //Buzzer_Demo();

    /* 旋律播完后进入空闲循环 */
    for (;;)
    {
        osDelay(1000);
    }
}

/**
  ******************************************************************************
  * @file           : Control_Task.c
  * @brief          : Control task
  ******************************************************************************
  * @attention      : Dog control logic will live here. RS04 motion is currently
  *                   driven from CAN_Task.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "cmsis_os.h"
#include "Control_Task.h"

void Control_Task(void const * argument)
{
    (void)argument;

    TickType_t tick = osKernelSysTick();
    for (;;)
    {
        osDelayUntil(&tick, 1U);
    }
}

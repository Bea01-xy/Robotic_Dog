/**
  ******************************************************************************
  * @file           : Detect_Task.c
  * @brief          : Input and communication monitoring task
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "cmsis_os.h"
#include "Detect_Task.h"
#include "Remote_Control.h"
#include "Minipc.h"

void Detect_Task(void const * argument)
{
    (void)argument;

    TickType_t tick = osKernelSysTick();
    for (;;)
    {
        Remote_Message_Moniter(&remote_ctrl);
        MiniPC_Offline_Monitor();
        MiniPC_Receive_Info();
        MiniPC_Data_Update_Last();

        osDelayUntil(&tick, 1U);
    }
}

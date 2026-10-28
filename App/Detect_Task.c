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
#include "INS_Task.h"
#include "bsp_uart.h"
#include "Motor_RS.h"

/* 9 floats at 115200 take about 3.5 ms; 10 ms period leaves margin. */
#define VOFA_SEND_PERIOD_MS 10U

void Detect_Task(void const * argument)
{
    (void)argument;

    TickType_t tick = osKernelSysTick();
    TickType_t vofa_tick = tick;
    for (;;)
    {
        Remote_Message_Moniter(&remote_ctrl);
        MiniPC_Offline_Monitor();
        MiniPC_Receive_Info();
        MiniPC_Data_Update_Last();

        const TickType_t now = osKernelSysTick();
        if ((now - vofa_tick) >= VOFA_SEND_PERIOD_MS)
        {
            vofa_tick = now;
            float vofa_data[10] = {
                #if 0
                (float)remote_ctrl.rc.ch[RC_RX],
                (float)remote_ctrl.rc.ch[RC_RY],
                (float)remote_ctrl.rc.ch[RC_LX],
                (float)remote_ctrl.rc.ch[RC_LY],
                (float)remote_ctrl.rc.ch[RC_VA],
                (float)remote_ctrl.rc.ch[RC_VB],
                (float)remote_ctrl.rc.sw[RC_R],
                (float)remote_ctrl.rc.s[RC_R],
                (float)remote_ctrl.rc.sw[RC_L],
                (float)remote_ctrl.rc.s[RC_L],
                #endif
                Motor_RS04.Data.Position,
                Motor_RS04.Data.Velocity,
            };
            (void)USART10_Vofa_SendFloat(vofa_data, 2U);
        }

        osDelayUntil(&tick, 1U);
    }
}

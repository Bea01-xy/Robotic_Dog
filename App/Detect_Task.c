/**
  ******************************************************************************
  * @file           : Detect_Task.c
  * @brief          : Input and communication monitoring task
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "Detect_Task.h"
#include "Remote_Control.h"
#include "Minipc.h"
#include "INS_Task.h"
#include "bsp_uart.h"
#include "Motor_RS.h"

/* Five floats plus the JustFloat tail take about 2.1 ms at 115200 baud. */
#define VOFA_SEND_PERIOD_MS 10U
#define VOFA_SCAN_ID_PERIOD_MS 500U

void Detect_Task(void const * argument)
{
    (void)argument;

    TickType_t tick = osKernelSysTick();
    TickType_t vofa_tick = tick;
    TickType_t scan_id_tick = tick;
    uint8_t scan_id_index = 0U;
    for (;;)
    {
        Remote_Message_Moniter(&remote_ctrl);
        MiniPC_Offline_Monitor();
        MiniPC_Receive_Info();
        MiniPC_Data_Update_Last();

        if (remote_ctrl.rc.ch[RC_VB] <= -630)
        {
            HAL_NVIC_SystemReset();
        }
        const TickType_t now = osKernelSysTick();
        if ((now - vofa_tick) >= VOFA_SEND_PERIOD_MS)
        {
            vofa_tick = now;
            const uint8_t scan_count = Motor_RS_ScanResult.Count;
            float scan_id = 0.0f;
            if (scan_count != 0U)
            {
                if (scan_id_index >= scan_count)
                {
                    scan_id_index = 0U;
                }
                if ((now - scan_id_tick) >= VOFA_SCAN_ID_PERIOD_MS)
                {
                    scan_id_index = (scan_id_index + 1U) % scan_count;
                    scan_id_tick = now;
                }
                scan_id = (float)Motor_RS_ScanResult.Ids[scan_id_index];
            }

            float vofa_data[] = {
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
                scan_id,
                (float)scan_count,
                Motor_RS_ScanResult.Complete ? 1.0f : 0.0f,
            };
            (void)USART10_Vofa_SendFloat(vofa_data,
                                        sizeof(vofa_data) / sizeof(vofa_data[0]));
        }

        osDelayUntil(&tick, 1U);
    }
}

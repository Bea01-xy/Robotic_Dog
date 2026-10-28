/**
  ******************************************************************************
  * @file           : CAN_Task.c
  * @brief          : CAN task for the RS04 motor
  * @author         : GrassFam Wang
  * @date           : 2025/1/22
  * @version        : v1.1
  ******************************************************************************
  * @attention      : None
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "cmsis_os.h"
#include "CAN_Task.h"
#include "bsp_can.h"
#include "Motor_RS.h"
#include "Remote_Control.h"

/* RS04 test motion: constant 1 rad/s in speed mode, motor CAN ID = 2. */
#define RS04_TARGET_SPEED_RAD_S       4.0f
#define RS04_SPEED_CURRENT_LIMIT_A    5.0f
#define RS04_STARTUP_DELAY_MS         20U

/* Send the private-protocol type-25 switch frame once per boot.
 * The motor applies the new protocol only after it is power-cycled. */
#define RS04_SEND_PROTOCOL_SWITCH     1

/* USER CODE BEGIN Header_CAN_Task */
/**
* @brief Function implementing the StartCANTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_CAN_Task */
void CAN_Task(void const * argument)
{
    (void)argument;

#if RS04_SEND_PROTOCOL_SWITCH
    /* Switch to MIT protocol. Power-cycle the motor after this frame. */
    RS_Motor_ProtocolSwitch(&FDCAN1_TxFrame, &Motor_RS04[0], MOTOR_RS_PROTOCOL_MIT);
    RS_Motor_ProtocolSwitch(&FDCAN1_TxFrame, &Motor_RS04[1], MOTOR_RS_PROTOCOL_MIT);
    osDelay(RS04_STARTUP_DELAY_MS);
#endif

    /* Stop first: the control mode may only be changed while disabled. */
    RS_Motor_Command(&FDCAN1_TxFrame, &Motor_RS04[0], MOTOR_RS_DISABLE);
    RS_Motor_Command(&FDCAN1_TxFrame, &Motor_RS04[1], MOTOR_RS_DISABLE);
    osDelay(RS04_STARTUP_DELAY_MS);

    RS_Motor_Command(&FDCAN1_TxFrame, &Motor_RS04[0], MOTOR_RS_SET_SPEED_MODE);
    RS_Motor_Command(&FDCAN1_TxFrame, &Motor_RS04[1], MOTOR_RS_SET_SPEED_MODE);
    osDelay(RS04_STARTUP_DELAY_MS);

    bool running = false;

    for(;;)
    {
        const bool stop_requested =
            remote_ctrl.rc_lost || switch_is_up(remote_ctrl.rc.sw[RC_R]);
        const bool run_requested =
            !remote_ctrl.rc_lost && switch_is_down(remote_ctrl.rc.sw[RC_R]);

        if (stop_requested)
        {
            if (running)
            {
                RS_Motor_Command(&FDCAN1_TxFrame, &Motor_RS04[0], MOTOR_RS_DISABLE);
                RS_Motor_Command(&FDCAN1_TxFrame, &Motor_RS04[1], MOTOR_RS_DISABLE);
                running = false;
            }
        }
        else if (run_requested)
        {
            if (!running)
            {
                RS_Motor_Command(&FDCAN1_TxFrame, &Motor_RS04[0], MOTOR_RS_ENABLE);
                RS_Motor_Command(&FDCAN1_TxFrame, &Motor_RS04[1], MOTOR_RS_ENABLE);
                running = true;
            }

            RS_Motor_Speed_CAN_TxMessage(&FDCAN1_TxFrame, &Motor_RS04[0],
                                        -RS04_TARGET_SPEED_RAD_S,
                                         RS04_SPEED_CURRENT_LIMIT_A);
            RS_Motor_Speed_CAN_TxMessage(&FDCAN1_TxFrame, &Motor_RS04[1],
                                         RS04_TARGET_SPEED_RAD_S,
                                         RS04_SPEED_CURRENT_LIMIT_A);
        }
        else
        {
            /* SW0 is in the middle position: keep the previous state. */
            if (running)
            {
                RS_Motor_Speed_CAN_TxMessage(&FDCAN1_TxFrame, &Motor_RS04[0],
                                            -RS04_TARGET_SPEED_RAD_S,
                                             RS04_SPEED_CURRENT_LIMIT_A);
                RS_Motor_Speed_CAN_TxMessage(&FDCAN1_TxFrame, &Motor_RS04[1],
                                             RS04_TARGET_SPEED_RAD_S,
                                             RS04_SPEED_CURRENT_LIMIT_A);
            }
        }

        osDelay(1);
    }
}


/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : Buzzer_Task.c
  * @brief          : Buzzer task controlled by remote switch SW1
  * @author         : Ported from CtrBoard-H7_BUZZER
  ******************************************************************************
  * @attention      :
  *   Remote switch sw[1] down -> play demo melody and loop after completion.
  *   Remote switch sw[1] up   -> pause playback.
  *   Remote lost              -> pause playback.
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "cmsis_os.h"
#include "Buzzer.h"
#include "Remote_Control.h"

/**
  * @brief  Buzzer task.
  * @param  argument: Not used
  * @retval None
  */
void Buzzer_Task(void const * argument)
{
    (void)argument;

    Buzzer_Init();
    osDelay(100);  /* Wait for TIM12 output to settle. */

    Buzzer_Music_Start();

    bool playing = false;

    for (;;)
    {
        const bool play_requested =
            !remote_ctrl.rc_lost && switch_is_down(remote_ctrl.rc.sw[RC_L]);

        if (!play_requested)
        {
            if (playing)
            {
                Buzzer_Music_Pause();
                playing = false;
            }
        }
        else
        {
            playing = true;

            if (Buzzer_Music_Step(1U))
            {
                /* Song finished: restart from the first note. */
                Buzzer_Music_Start();
            }
        }

        osDelay(1);
    }
}

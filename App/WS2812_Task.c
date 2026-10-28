/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : WS2812_Task.c
  * @brief          : WS2812 LED task
  *                   整条灯带红/蓝交替闪烁
  * @author         : Ported from CtrBoard-H7_WS2812
  ******************************************************************************
  * @attention      : None
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "cmsis_os.h"
#include "WS2812.h"

/* 每种颜色的保持时间 (ms) */
#define WS2812_BLINK_INTERVAL_MS    500U

/**
  * @brief  WS2812 task: 红/蓝交替闪烁
  * @param  argument: Not used
  * @retval None
  */
void WS2812_Task(void const * argument)
{
    (void)argument;

    static const uint8_t colors[2][3] = {
        { 255U,   0U,   0U },   /* 红 */
        {   0U,   0U, 255U },   /* 蓝 */
    };
    uint8_t phase = 0U;
    uint16_t i;

    WS2812_Init();

    for (;;)
    {
        for (i = 0U; i < WS2812_LED_NUM; i++)
        {
            WS2812_SetLED(i, colors[phase][0], colors[phase][1], colors[phase][2]);
        }
        WS2812_Update();

        phase ^= 1U;
        osDelay(WS2812_BLINK_INTERVAL_MS);
    }
}

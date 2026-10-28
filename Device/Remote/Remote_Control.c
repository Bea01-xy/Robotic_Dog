/* Includes ------------------------------------------------------------------*/
#include "Remote_Control.h"
#include "usart.h"

/* Exported variables ---------------------------------------------------------*/
/**
 * @brief remote control structure variable
 */
 Remote_Info_Typedef remote_ctrl={
	.online_cnt = 0xFAU,
	.rc_lost = true,
};

/**
 * @brief remote control usart RxDMA MultiBuffer
 */
__attribute__((section (".AXI_SRAM")))uint8_t SBUS_MultiRx_Buf[2][SBUS_RX_BUF_NUM];

/* Private variables ---------------------------------------------------------*/
/**
 * @brief structure that contains the information of keyboard
 */
KeyBoard_Info_Typedef KeyBoard_Info;

/* Private function prototypes -----------------------------------------------*/
/**
  * @brief  Convert SBUS frame to remote control data.
  */
static void fs_sbus_to_rc(volatile const uint8_t *sbus_buf, Remote_Info_Typedef  *remote_ctrl);

/**
  * @brief  convert the remote control received message
  * @param  sbus_buf: pointer to a array that contains the information of the received message.
  * @param  remote_ctrl: pointer to a Remote_Info_Typedef structure that
  *         contains the information  for the remote control.
  * @retval none
  */


void SBUS_TO_RC(volatile const uint8_t *sbus_buf, Remote_Info_Typedef  *remote_ctrl)
{
#if defined(DBUS)
    dji_dbus_to_rc(sbus_buf, remote_ctrl);
    /* DBUS receiver: assume frame is valid (stops sending when offline) */
    remote_ctrl->online_cnt = 0xFAU;
    remote_ctrl->rc_lost = false;
#elif defined(SBUS)
    fs_sbus_to_rc(sbus_buf, remote_ctrl);
    /* SBUS: only mark online if the frame doesn't report failsafe/lost */
    if (!(sbus_buf[23] & 0x0CU))
    {
        remote_ctrl->online_cnt = 0xFAU;
        remote_ctrl->rc_lost = false;
    }
#endif

}
//------------------------------------------------------------------------------
static void __attribute__((unused)) dji_dbus_to_rc(volatile const uint8_t *sbus_buf, Remote_Info_Typedef  *remote_ctrl)
{
        if (sbus_buf == NULL || remote_ctrl == NULL) return;

    /* Channel 0, 1, 2, 3 */
    remote_ctrl->rc.ch[0] = (  sbus_buf[0]       | (sbus_buf[1] << 8 ) ) & 0x07ff;                            //!< Channel 0
    remote_ctrl->rc.ch[1] = ( (sbus_buf[1] >> 3) | (sbus_buf[2] << 5 ) ) & 0x07ff;                            //!< Channel 1
    remote_ctrl->rc.ch[2] = ( (sbus_buf[2] >> 6) | (sbus_buf[3] << 2 ) | (sbus_buf[4] << 10) ) & 0x07ff;      //!< Channel 2
    remote_ctrl->rc.ch[3] = ( (sbus_buf[4] >> 1) | (sbus_buf[5] << 7 ) ) & 0x07ff;                            //!< Channel 3
    remote_ctrl->rc.ch[4] = (  sbus_buf[16] 	   | (sbus_buf[17] << 8) ) & 0x07ff;                 			      //!< Channel 4

    /* Switch left, right */
    remote_ctrl->rc.s[0] = ((sbus_buf[5] >> 4) & 0x0003);                  //!< Switch left
    remote_ctrl->rc.s[1] = ((sbus_buf[5] >> 4) & 0x000C) >> 2;             //!< Switch right

    /* Mouse axis: X, Y, Z */
    remote_ctrl->mouse.x = sbus_buf[6]  | (sbus_buf[7] << 8);                    //!< Mouse X axis
    remote_ctrl->mouse.y = sbus_buf[8]  | (sbus_buf[9] << 8);                    //!< Mouse Y axis
    remote_ctrl->mouse.z = sbus_buf[10] | (sbus_buf[11] << 8);                  //!< Mouse Z axis

    /* Mouse Left, Right Is Press  */
    remote_ctrl->mouse.press_l = sbus_buf[12];                                  //!< Mouse Left Is Press
    remote_ctrl->mouse.press_r = sbus_buf[13];                                  //!< Mouse Right Is Press

    /* KeyBoard value */
    remote_ctrl->key.v = sbus_buf[14] | (sbus_buf[15] << 8);                    //!< KeyBoard value

    remote_ctrl->rc.ch[0] -= RC_CH_VALUE_OFFSET;
    remote_ctrl->rc.ch[1] -= RC_CH_VALUE_OFFSET;
    remote_ctrl->rc.ch[2] -= RC_CH_VALUE_OFFSET;
    remote_ctrl->rc.ch[3] -= RC_CH_VALUE_OFFSET;
    remote_ctrl->rc.ch[4] -= RC_CH_VALUE_OFFSET;
}

//富斯遥控器
//富斯摇杆范围为240-1807，中间值为1024
int16_t map_joystick_rounded(int16_t adc_value)
{
    const int FROM_LOW  = 240;
    const int FROM_HIGH = 1807;
    const int TO_LOW    = 364;
    const int TO_HIGH   = 1684;
    const int DENOM     = FROM_HIGH - FROM_LOW; // 1567

    if (DENOM == 0) return TO_LOW;

    long num = (long)(adc_value - FROM_LOW) * (TO_HIGH - TO_LOW);
    long result = (num + DENOM / 2) / DENOM + TO_LOW;

    if (result < 364) return 364;
    if (result > 1684) return 1684;
    return (int16_t)result;
}
//取正函数
static int16_t RC_abs(int16_t value)
{
    if (value > 0)
    {
        return value;
    }
    else
    {
        return -value;
    }
}
//拨杆返回值上为240，中为1024，下为1807
uint8_t map_toggle_switch(uint16_t adc_value)
{
    if (RC_abs(adc_value - 240)<= 2) {
        return RC_SW_UP;
    }else if (RC_abs(adc_value - 1807)<= 2) {
        return RC_SW_DOWN;
    } else if (RC_abs(adc_value - 1024)<= 2) {
        return RC_SW_MID;
    }

    return RC_SW_MID;
}

uint16_t temp_s[2] = { 0, 0 };
uint16_t temp_sw[2] = { 0, 0 };

static void fs_sbus_to_rc(volatile const uint8_t *sbus_buf, Remote_Info_Typedef  *remote_ctrl)
{

    if (sbus_buf == NULL || remote_ctrl == NULL){
        return;
    }
    //无键鼠
    remote_ctrl->mouse.x = 0;
    remote_ctrl->mouse.y = 0;
    remote_ctrl->mouse.z = 0;
    remote_ctrl->mouse.press_l = 0;
    remote_ctrl->mouse.press_r = 0;
    remote_ctrl->key.v = 0;

    remote_ctrl->rc.ch[0] = (sbus_buf[1] | (sbus_buf[2] << 8)) & 0x07ff;                            //!< Channel 0
    remote_ctrl->rc.ch[1] = ((sbus_buf[2] >> 3) | (sbus_buf[3] << 5)) & 0x07ff;                           //!< Channel 1
    remote_ctrl->rc.ch[3] = ((sbus_buf[3] >> 6) | (sbus_buf[4] << 2) |          //!< Channel 2
                         (sbus_buf[5] << 10)) &0x07ff;      //!< Channel 2
    remote_ctrl->rc.ch[2] = ((sbus_buf[5] >> 1) | (sbus_buf[6] << 7)) & 0x07ff;  //!< Channel 3
    remote_ctrl->rc.ch[4] = ((sbus_buf[6] >> 4) | (sbus_buf[7] << 4)) & 0x07FF; //!< Channel 4
    remote_ctrl->rc.ch[5] = ((sbus_buf[7] >> 7 | (sbus_buf[8] << 1) | (sbus_buf[9] << 9)) & 0x07FF);
    temp_sw[1] = ((sbus_buf[9] >> 2 )| (sbus_buf[10] << 6))              & 0x07FF;
    temp_s[1]  = ((sbus_buf[10] >> 5) | (sbus_buf[11] << 3)) & 0x07FF;
    temp_s[0]  = ((sbus_buf[12])   | (sbus_buf[13] << 8))  & 0x07FF;
    temp_sw[0] = ((sbus_buf[13] >> 3)| (sbus_buf[14] << 5))& 0x07FF;

    for (int i=0;i<=5;i++){
        remote_ctrl->rc.ch[i] = map_joystick_rounded(remote_ctrl->rc.ch[i]);
    }

    remote_ctrl->rc.ch[0] -= RC_CH_VALUE_OFFSET;
    remote_ctrl->rc.ch[1] -= RC_CH_VALUE_OFFSET;
    remote_ctrl->rc.ch[2] -= RC_CH_VALUE_OFFSET;
    remote_ctrl->rc.ch[3] -= RC_CH_VALUE_OFFSET;
    remote_ctrl->rc.ch[4] -= RC_CH_VALUE_OFFSET;
    remote_ctrl->rc.ch[5] -= RC_CH_VALUE_OFFSET;

    remote_ctrl->rc.s[0] = map_toggle_switch(temp_s[0]);
    remote_ctrl->rc.s[1] = map_toggle_switch(temp_s[1]);
    remote_ctrl->rc.sw[0] = map_toggle_switch(temp_sw[0]);
    remote_ctrl->rc.sw[1] = map_toggle_switch(temp_sw[1]);
}

/**
  * @brief  clear the remote control data while the device offline
  * @param  remote_ctrl: pointer to a Remote_Info_Typedef structure that
  *         contains the information  for the remote control.
  * @retval none
  */
void Remote_Message_Moniter(Remote_Info_Typedef  *remote_ctrl)
{
  /* Juege the device status */
  if(remote_ctrl->online_cnt <= 0x32U)
  {
    /* clear the data */
    memset(remote_ctrl,0,sizeof(Remote_Info_Typedef));

    /* reset the online count */

    /* set the lost flag */
		remote_ctrl->rc_lost = true;

  }
  else if(remote_ctrl->online_cnt > 0)
  {
    /* online count decrements which reseted in received interrupt  */
    remote_ctrl->online_cnt--;
  }
}
//------------------------------------------------------------------------------


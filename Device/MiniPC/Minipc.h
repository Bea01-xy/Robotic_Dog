/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : MiniPC.h
  * @brief          : MiniPC interfaces functions
  * @author         : GrassFan Wang
  * @date           : 2025/02/10
  * @version        : v1.0
  ******************************************************************************
  * @attention      : None
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef DEVICE_MINIPC_H
#define DEVICE_MINIPC_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stdint.h"
#include "stdbool.h"

/* Exported types ------------------------------------------------------------*/
/* MiniPC 数据结构定义 - 用于接收来自上位机的数据 */
typedef struct {
    /* 关节数据 - 6个float = 24字节 */
    float joint_pos_data[6];
    uint16_t controller_buttons;
    uint16_t valid_joint_mask;
    uint16_t command_sequence;
    uint32_t controller_time_ms;

    /* 自定义控制器数据 - 6字节*/
    uint8_t main_buttons;
    uint8_t handle_buttons;             /*!< 位域: bit0=btn1(1), bit1=btn2(2), bit2=btn3(4), bit3=btn4(8) */
    uint8_t btn1;                       /*!< handle_buttons bit0 拆出的独立标志 (0/1), 配合 MINIPC_KEY_RISING_EDGE 使用 */
    uint8_t btn2;                       /*!< handle_buttons bit1 */
    uint8_t btn3;                       /*!< handle_buttons bit2 */
    uint8_t btn4;                       /*!< handle_buttons bit3 */
    uint16_t joystick_x;
    uint16_t joystick_y;

    /* 鼠标数据 - 3个int16_t = 6字节 */
    int16_t mouse_x;
    int16_t mouse_y;
    int16_t mouse_z;

    /* 鼠标按键 - 3个uint8_t = 3字节 */
    uint8_t mouse_left;
    uint8_t mouse_right;
    uint8_t mouse_mid;

    /* 键盘按键 - 18个uint8_t = 18字节 */
    uint8_t key_w;
    uint8_t key_s;
    uint8_t key_a;
    uint8_t key_d;
    uint8_t key_shift;
    uint8_t key_ctrl;
    uint8_t key_q;
    uint8_t key_e;
    uint8_t key_r;
    uint8_t key_f;
    uint8_t key_g;
    uint8_t key_z;
    uint8_t key_x;
    uint8_t key_c;
    uint8_t key_v;
    uint8_t key_b;
    uint8_t key_1;
    uint8_t key_2;

    /* 在线状态 - 用法同 Remote_Info_Typedef / DM_Motor_Info_Typedef */
    uint8_t online_cnt;     /*!< 在线计数, 收到有效帧时复位 0xFA, 主循环递减 */
    bool    lost;           /*!< 离线标志, online_cnt ≤ 0x32 时置 true */
} MiniPC_DataTypeDef;

/* Exported variables --------------------------------------------------------*/
/* 全局实例声明 - 在 MiniPC.c 中定义 */
extern MiniPC_DataTypeDef MiniPC_Data;
/* 上一循环状态镜像 - 用于按键/鼠标信号的边沿检测（跳变检测） */
extern MiniPC_DataTypeDef MiniPC_Data_Last;

/* Exported macros -----------------------------------------------------------*/
/* 边沿检测辅助宏 - 配合 MiniPC_Data_Update_Last() 在循环末尾的更新使用
 * 用法示例： if (MINIPC_KEY_RISING_EDGE(key_q)) { ... } */
#define MINIPC_KEY_RISING_EDGE(key)   ( MiniPC_Data.key && !MiniPC_Data_Last.key)
#define MINIPC_KEY_FALLING_EDGE(key)  (!MiniPC_Data.key &&  MiniPC_Data_Last.key)
#define MINIPC_KEY_CHANGED(key)       ( MiniPC_Data.key !=  MiniPC_Data_Last.key)

/* Exported functions prototypes ---------------------------------------------*/
uint8_t MiniPC_Transmit_Info(float* Buf, uint16_t Len);
void    MiniPC_Receive_Info(void);
void    MiniPC_Offline_Monitor(void);
/* 将当前 MiniPC_Data 拷贝到 MiniPC_Data_Last，应在每个循环末尾调用一次 */
void    MiniPC_Data_Update_Last(void);

#ifdef __cplusplus
}
#endif

#endif /* DEVICE_MINIPC_H */

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : Motor_RS.h
  * @brief          : RobStride RS04 motor driver (MIT protocol)
  * @author         : Generated from RS04 user manual
  * @date           : 2026/10/27
  * @version        : v1.0
  ******************************************************************************
  * @attention
  *
  * Protocol: RobStride MIT protocol, CAN 2.0 classic standard frame, 1 Mbps.
  *   Control ID  = (mode << 8) | motor_can_id
  *                 mode 0 = MIT, 1 = position, 2 = speed
  *   Feedback ID = host_can_id (default 0xFD)
  *                 Data[0] = motor CAN ID
  *
  * RS04 MIT ranges:
  *   position : -12.57 ... +12.57 rad
  *   velocity : -15.0  ... +15.0  rad/s
  *   torque   : -120.0 ... +120.0 Nm
  *   kp       : 0 ... 5000
  *   kd       : 0 ... 100
  *
  * This module is standalone and does not depend on Motor_DM/Motor_DJI.
  *
  * Typical use on the selected CAN bus:
  *   RS_Motor_Command(&FDCAN1_TxFrame, &Motor_RS04[0], MOTOR_RS_ENABLE);
  *   RS_Motor_CAN_TxMessage(&FDCAN1_TxFrame, &Motor_RS04[0],
  *                           pos_rad, vel_rad_s, kp, kd, torque_nm);
  *
  * Feedback: call RS_Motor_Info_Update() from the matching RX handler:
  *   RS_Motor_Info_Update(Identifier, Data, &Motor_RS04[0]);
  *   RS_Motor_Info_Update(Identifier, Data, &Motor_RS04[1]);
  *
  * To change control mode, stop the motor first, then:
  *   MOTOR_RS_DISABLE -> MOTOR_RS_SET_*_MODE -> MOTOR_RS_ENABLE
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef DEVICE_MOTOR_RS_H
#define DEVICE_MOTOR_RS_H

/* Includes ------------------------------------------------------------------*/
#include "robot_config.h"
#include "stdbool.h"
#include "stm32h723xx.h"
#include "bsp_can.h"

/* RS04 model parameters -----------------------------------------------------*/
#define MOTOR_RS04_P_MAX     12.57f
#define MOTOR_RS04_V_MAX     15.0f
#define MOTOR_RS04_T_MAX     120.0f
#define MOTOR_RS04_KP_MAX    5000.0f
#define MOTOR_RS04_KD_MAX    100.0f

/* Motor CAN ID used by this project. The motor must also be configured to ID 2.
 * Host CAN ID remains the RobStride default 0xFD. */
#ifndef MOTOR_RS04_1_CAN_ID
#define MOTOR_RS04_1_CAN_ID    0x01U
#endif
#ifndef MOTOR_RS04_2_CAN_ID
#define MOTOR_RS04_2_CAN_ID    0x02U
#endif

#ifndef MOTOR_RS04_HOST_CAN_ID
#define MOTOR_RS04_HOST_CAN_ID 0xFDU
#endif

#define MOTOR_RS04_PARAM_RANGE_INIT { \
    .P_MAX  = MOTOR_RS04_P_MAX,  \
    .V_MAX  = MOTOR_RS04_V_MAX,  \
    .T_MAX  = MOTOR_RS04_T_MAX,  \
    .KP_MAX = MOTOR_RS04_KP_MAX, \
    .KD_MAX = MOTOR_RS04_KD_MAX, \
}

/**
 * @brief Control mode encoded in bits 10:8 of the standard CAN identifier.
 */
typedef enum
{
    MOTOR_RS_CTRL_MIT      = 0,
    MOTOR_RS_CTRL_POSITION = 1,
    MOTOR_RS_CTRL_SPEED    = 2,
} Motor_RS_Control_Mode_e;

/**
 * @brief Motor state reported in the feedback frame (Byte6 bits 7:6).
 */
typedef enum
{
    MOTOR_RS_STATE_RESET = 0,
    MOTOR_RS_STATE_CALI  = 1,
    MOTOR_RS_STATE_MOTOR = 2,
} Motor_RS_State_e;

/**
 * @brief MIT protocol command types.
 */
typedef enum
{
    MOTOR_RS_ENABLE = 0,
    MOTOR_RS_DISABLE,
    MOTOR_RS_SAVE_ZERO,
    MOTOR_RS_CLEAR_ERROR,
    MOTOR_RS_SET_MIT_MODE,
    MOTOR_RS_SET_POSITION_MODE,
    MOTOR_RS_SET_SPEED_MODE,
    MOTOR_RS_SAVE_PARAMS,
} Motor_RS_CMD_Type_e;

/**
 * @brief CAN protocol selected by the private-protocol switch frame.
 */
typedef enum
{
    MOTOR_RS_PROTOCOL_PRIVATE = 0,
    MOTOR_RS_PROTOCOL_CANOPEN = 1,
    MOTOR_RS_PROTOCOL_MIT     = 2,
} Motor_RS_Protocol_e;

/**
 * @brief CAN identifiers for one RS motor.
 * @note  TxIdentifier stores the motor CAN ID (0x00~0xFF), not the full frame ID.
 *        RxIdentifier stores the host CAN ID used by feedback frames.
 */
typedef struct
{
    uint32_t TxIdentifier;
    uint32_t RxIdentifier;
} Motor_RS_CANFrameInfo_Typedef;

/**
 * @brief Protocol conversion ranges.
 */
typedef struct
{
    float P_MAX;
    float V_MAX;
    float T_MAX;
    float KP_MAX;
    float KD_MAX;
} Motor_RS_Param_Range_Typedef;

/**
 * @brief Decoded motor data and state.
 */
typedef struct
{
    bool     Initialized;
    uint8_t  Mode_State;          /*!< Motor_RS_State_e */
    bool     Fault;               /*!< feedback fault flag */
    bool     Warning;             /*!< feedback warning flag */

    uint16_t P_int;               /*!< raw 16-bit position */
    uint16_t V_int;               /*!< raw 12-bit velocity */
    uint16_t T_int;               /*!< raw 12-bit torque */
    uint16_t Temperature_int;     /*!< raw 12-bit temperature, 0.1 degC */

    float    Position;            /*!< rad */
    float    Velocity;            /*!< rad/s */
    float    Torque;              /*!< Nm */
    float    Temperature;         /*!< degC */

    uint8_t  online_cnt;          /*!< same convention as DM driver */
    bool     offline;

    float    Target_Position;     /*!< rad */
    float    Target_Velocity;     /*!< rad/s */
    float    Target_Torque;       /*!< Nm */
    float    Target_KP;
    float    Target_KD;
} Motor_RS_Data_Typedef;

/**
 * @brief Complete RS motor instance.
 */
typedef struct
{
    Motor_RS_Control_Mode_e       Control_Mode;
    Motor_RS_CANFrameInfo_Typedef FDCANFrame;
    Motor_RS_Param_Range_Typedef  Param_Range;
    Motor_RS_Data_Typedef         Data;
} Motor_RS_Info_Typedef;

/* Externs ------------------------------------------------------------------*/
#define MOTOR_RS04_NUM 2U
extern Motor_RS_Info_Typedef Motor_RS04[MOTOR_RS04_NUM];

/**
  * @brief  Switch the motor CAN protocol using private-protocol type 25.
  * @param  FDCAN_TxFrame transmit frame to prepare
  * @param  RS_Motor      motor instance (uses motor ID and host ID)
  * @param  Protocol      MOTOR_RS_PROTOCOL_PRIVATE / CANOPEN / MIT
  * @note   The frame is a 29-bit extended frame:
  *           ID   = (0x19 << 24) | (host_id << 8) | motor_id
  *           Data = 01 02 03 04 05 06 protocol 00
  *         The new protocol takes effect only after the motor is power-cycled.
  */
extern void RS_Motor_ProtocolSwitch(FDCAN_TxFrame_TypeDef *FDCAN_TxFrame,
                                    Motor_RS_Info_Typedef *RS_Motor,
                                    Motor_RS_Protocol_e Protocol);

/**
  * @brief  Send enable/disable/zero/mode command to an RS motor.
  * @note   Stop the motor before switching control mode.
  */
extern void RS_Motor_Command(FDCAN_TxFrame_TypeDef *FDCAN_TxFrame,
                             Motor_RS_Info_Typedef *RS_Motor,
                             Motor_RS_CMD_Type_e CMD);

/**
  * @brief  Send one MIT-mode control frame (position, velocity, kp, kd, torque).
  * @note   Motor must already be enabled and in MIT mode.
  */
extern void RS_Motor_CAN_TxMessage(FDCAN_TxFrame_TypeDef *FDCAN_TxFrame,
                                   Motor_RS_Info_Typedef *RS_Motor,
                                   float Position, float Velocity,
                                   float KP, float KD, float Torque);

/**
  * @brief  Send position-mode control frame (command 10).
  * @param  Position target position, rad
  * @param  Velocity target velocity, rad/s
  * @note   Set position mode and enable the motor first.
  */
extern void RS_Motor_Position_CAN_TxMessage(FDCAN_TxFrame_TypeDef *FDCAN_TxFrame,
                                            Motor_RS_Info_Typedef *RS_Motor,
                                            float Position, float Velocity);

/**
  * @brief  Send speed-mode control frame (command 11).
  * @param  Velocity      target velocity, rad/s
  * @param  Current_Limit speed/position mode current limit, A
  * @note   Set speed mode and enable the motor first.
  */
extern void RS_Motor_Speed_CAN_TxMessage(FDCAN_TxFrame_TypeDef *FDCAN_TxFrame,
                                         Motor_RS_Info_Typedef *RS_Motor,
                                         float Velocity, float Current_Limit);

/**
  * @brief  Decode an RS MIT feedback frame.
  * @param  Identifier received standard identifier
  * @param  Rx_Buf     received 8-byte payload
  * @param  RS_Motor   motor instance to update
  */
extern void RS_Motor_Info_Update(uint32_t *Identifier, uint8_t *Rx_Buf,
                                 Motor_RS_Info_Typedef *RS_Motor);

#endif /* DEVICE_MOTOR_RS_H */

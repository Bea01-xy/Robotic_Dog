/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : Motor_RS.c
  * @brief          : RobStride RS04 motor driver (MIT protocol)
  * @author         : Generated from RS04 user manual
  * @date           : 2026/10/27
  * @version        : v1.0
  ******************************************************************************
  * @attention
  *
  * MIT protocol frame layout (standard 11-bit ID, classic CAN, 8 bytes):
  *
  *   ID = (mode << 8) | motor_can_id
  *     mode 0: MIT control (Byte0~7 below)
  *     mode 1: position control (float position, float velocity)
  *     mode 2: speed control    (float velocity, float current limit)
  *
  *   MIT control payload:
  *     Byte0~1   : target position, 16-bit, -P_MAX...+P_MAX
  *     Byte2     : target velocity high 8 bits
  *     Byte3[7:4]: target velocity low 4 bits
  *     Byte3[3:0]: kp high 4 bits
  *     Byte4     : kp low 8 bits
  *     Byte5     : kd high 8 bits
  *     Byte6[7:4]: kd low 4 bits
  *     Byte6[3:0]: target torque high 4 bits
  *     Byte7     : target torque low 8 bits
  *
  *   Feedback payload (ID = host CAN ID):
  *     Byte0     : motor CAN ID
  *     Byte1~2   : position, 16-bit, -P_MAX...+P_MAX
  *     Byte3     : velocity high 8 bits
  *     Byte4[7:4]: velocity low 4 bits
  *     Byte4[3:0]: torque high 4 bits
  *     Byte5     : torque low 8 bits
  *     Byte6[7:6]: state (0 reset, 1 cali, 2 motor)
  *     Byte6[5]  : fault flag
  *     Byte6[4]  : warning flag
  *     Byte6[3:0]: temperature high 4 bits
  *     Byte7     : temperature low 8 bits (0.1 degC)
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "Motor_RS.h"

/* MIT protocol identifiers and command bytes ---------------------------------*/
#define MOTOR_RS_ID_MOTOR_MASK      0x0FFU
#define MOTOR_RS_MODE_MIT_ID        0x000U
#define MOTOR_RS_MODE_POSITION_ID   0x100U
#define MOTOR_RS_MODE_SPEED_ID      0x200U

#define MOTOR_RS_CMD_ENABLE_BYTE    0xFCU
#define MOTOR_RS_CMD_DISABLE_BYTE   0xFDU
#define MOTOR_RS_CMD_ZERO_BYTE      0xFEU
#define MOTOR_RS_CMD_CLEAR_BYTE     0xFBU
#define MOTOR_RS_CMD_SAVE_BYTE      0xF8U

/* Private-protocol type 25: switch CAN protocol (29-bit extended frame). */
#define MOTOR_RS_PROTOCOL_TYPE      0x19U

/**
 * @brief Default RS04 instances. Change the CAN IDs to match the motors.
 */
Motor_RS_Info_Typedef Motor_RS04[MOTOR_RS04_NUM] = {
    [0] = {
        .Control_Mode = MOTOR_RS_CTRL_MIT,
        .Param_Range  = MOTOR_RS04_PARAM_RANGE_INIT,
        .FDCANFrame = {
            .TxIdentifier = MOTOR_RS04_1_CAN_ID,
            .RxIdentifier = MOTOR_RS04_HOST_CAN_ID,
        },
    },
    [1] = {
        .Control_Mode = MOTOR_RS_CTRL_MIT,
        .Param_Range  = MOTOR_RS04_PARAM_RANGE_INIT,
        .FDCANFrame = {
            .TxIdentifier = MOTOR_RS04_2_CAN_ID,
            .RxIdentifier = MOTOR_RS04_HOST_CAN_ID,
        },
    },
};

static uint16_t RS_Float_To_Uint(float x, float x_min, float x_max, uint8_t bits);
static float    RS_Uint_To_Float(uint16_t x, float x_min, float x_max, uint8_t bits);
static void     RS_WriteFloatLE(uint8_t *buf, float value);
static void     RS_PrepareFrameType(FDCAN_TxFrame_TypeDef *frame, uint32_t identifier, uint32_t id_type);
static void     RS_PrepareFrame(FDCAN_TxFrame_TypeDef *frame, uint32_t identifier);

/*------------------------------------------------------------------------------*/
/* Public API                                                                   */
/*------------------------------------------------------------------------------*/

void RS_Motor_ProtocolSwitch(FDCAN_TxFrame_TypeDef *FDCAN_TxFrame,
                             Motor_RS_Info_Typedef *RS_Motor,
                             Motor_RS_Protocol_e Protocol)
{
    uint32_t motor_id;
    uint32_t host_id;

    if (FDCAN_TxFrame == NULL || RS_Motor == NULL) {
        return;
    }

    motor_id = RS_Motor->FDCANFrame.TxIdentifier & MOTOR_RS_ID_MOTOR_MASK;
    host_id  = RS_Motor->FDCANFrame.RxIdentifier & MOTOR_RS_ID_MOTOR_MASK;

    /* Private protocol type 25 uses a 29-bit extended identifier. */
    RS_PrepareFrameType(FDCAN_TxFrame,
                        (MOTOR_RS_PROTOCOL_TYPE << 24) |
                        (host_id << 8) |
                        motor_id,
                        FDCAN_EXTENDED_ID);

    FDCAN_TxFrame->Data[0] = 0x01U;
    FDCAN_TxFrame->Data[1] = 0x02U;
    FDCAN_TxFrame->Data[2] = 0x03U;
    FDCAN_TxFrame->Data[3] = 0x04U;
    FDCAN_TxFrame->Data[4] = 0x05U;
    FDCAN_TxFrame->Data[5] = 0x06U;
    FDCAN_TxFrame->Data[6] = (uint8_t)Protocol;
    FDCAN_TxFrame->Data[7] = 0x00U;

    USER_FDCAN_AddMessageToTxFifoQ(FDCAN_TxFrame);
}

void RS_Motor_Command(FDCAN_TxFrame_TypeDef *FDCAN_TxFrame,
                      Motor_RS_Info_Typedef *RS_Motor,
                      Motor_RS_CMD_Type_e CMD)
{
    uint32_t motor_id;
    uint8_t i;

    if (FDCAN_TxFrame == NULL || RS_Motor == NULL) {
        return;
    }

    motor_id = RS_Motor->FDCANFrame.TxIdentifier & MOTOR_RS_ID_MOTOR_MASK;

    /* Command frames use the motor ID directly (MIT mode type 0). */
    RS_PrepareFrame(FDCAN_TxFrame, motor_id);

    for (i = 0U; i < 7U; i++) {
        FDCAN_TxFrame->Data[i] = 0xFFU;
    }

    switch (CMD) {
        case MOTOR_RS_ENABLE:
            FDCAN_TxFrame->Data[7] = MOTOR_RS_CMD_ENABLE_BYTE;
            break;

        case MOTOR_RS_DISABLE:
            FDCAN_TxFrame->Data[7] = MOTOR_RS_CMD_DISABLE_BYTE;
            break;

        case MOTOR_RS_SAVE_ZERO:
            FDCAN_TxFrame->Data[7] = MOTOR_RS_CMD_ZERO_BYTE;
            break;

        case MOTOR_RS_CLEAR_ERROR:
            /* Byte6 = 0xFF clears the current fault. */
            FDCAN_TxFrame->Data[6] = 0xFFU;
            FDCAN_TxFrame->Data[7] = MOTOR_RS_CMD_CLEAR_BYTE;
            break;

        case MOTOR_RS_SET_MIT_MODE:
            FDCAN_TxFrame->Data[6] = (uint8_t)MOTOR_RS_CTRL_MIT;
            FDCAN_TxFrame->Data[7] = MOTOR_RS_CMD_ENABLE_BYTE;
            RS_Motor->Control_Mode = MOTOR_RS_CTRL_MIT;
            break;

        case MOTOR_RS_SET_POSITION_MODE:
            FDCAN_TxFrame->Data[6] = (uint8_t)MOTOR_RS_CTRL_POSITION;
            FDCAN_TxFrame->Data[7] = MOTOR_RS_CMD_ENABLE_BYTE;
            RS_Motor->Control_Mode = MOTOR_RS_CTRL_POSITION;
            break;

        case MOTOR_RS_SET_SPEED_MODE:
            FDCAN_TxFrame->Data[6] = (uint8_t)MOTOR_RS_CTRL_SPEED;
            FDCAN_TxFrame->Data[7] = MOTOR_RS_CMD_ENABLE_BYTE;
            RS_Motor->Control_Mode = MOTOR_RS_CTRL_SPEED;
            break;

        case MOTOR_RS_SAVE_PARAMS:
            FDCAN_TxFrame->Data[7] = MOTOR_RS_CMD_SAVE_BYTE;
            break;

        default:
            return;
    }

    USER_FDCAN_AddMessageToTxFifoQ(FDCAN_TxFrame);
}

void RS_Motor_CAN_TxMessage(FDCAN_TxFrame_TypeDef *FDCAN_TxFrame,
                            Motor_RS_Info_Typedef *RS_Motor,
                            float Position, float Velocity,
                            float KP, float KD, float Torque)
{
    uint32_t motor_id;
    uint16_t position_int;
    uint16_t velocity_int;
    uint16_t kp_int;
    uint16_t kd_int;
    uint16_t torque_int;

    if (FDCAN_TxFrame == NULL || RS_Motor == NULL) {
        return;
    }

    motor_id = RS_Motor->FDCANFrame.TxIdentifier & MOTOR_RS_ID_MOTOR_MASK;

    position_int = RS_Float_To_Uint(Position,
                                    -RS_Motor->Param_Range.P_MAX,
                                     RS_Motor->Param_Range.P_MAX, 16U);
    velocity_int = RS_Float_To_Uint(Velocity,
                                    -RS_Motor->Param_Range.V_MAX,
                                     RS_Motor->Param_Range.V_MAX, 12U);
    kp_int       = RS_Float_To_Uint(KP, 0.0f,
                                     RS_Motor->Param_Range.KP_MAX, 12U);
    kd_int       = RS_Float_To_Uint(KD, 0.0f,
                                     RS_Motor->Param_Range.KD_MAX, 12U);
    torque_int   = RS_Float_To_Uint(Torque,
                                    -RS_Motor->Param_Range.T_MAX,
                                     RS_Motor->Param_Range.T_MAX, 12U);

    RS_PrepareFrame(FDCAN_TxFrame, motor_id | MOTOR_RS_MODE_MIT_ID);

    FDCAN_TxFrame->Data[0] = (uint8_t)(position_int >> 8);
    FDCAN_TxFrame->Data[1] = (uint8_t)(position_int);
    FDCAN_TxFrame->Data[2] = (uint8_t)(velocity_int >> 4);
    FDCAN_TxFrame->Data[3] = (uint8_t)(((velocity_int & 0x0FU) << 4) |
                                       ((kp_int >> 8) & 0x0FU));
    FDCAN_TxFrame->Data[4] = (uint8_t)(kp_int);
    FDCAN_TxFrame->Data[5] = (uint8_t)(kd_int >> 4);
    FDCAN_TxFrame->Data[6] = (uint8_t)(((kd_int & 0x0FU) << 4) |
                                       ((torque_int >> 8) & 0x0FU));
    FDCAN_TxFrame->Data[7] = (uint8_t)(torque_int);

    USER_FDCAN_AddMessageToTxFifoQ(FDCAN_TxFrame);
}

void RS_Motor_Position_CAN_TxMessage(FDCAN_TxFrame_TypeDef *FDCAN_TxFrame,
                                     Motor_RS_Info_Typedef *RS_Motor,
                                     float Position, float Velocity)
{
    uint32_t motor_id;

    if (FDCAN_TxFrame == NULL || RS_Motor == NULL) {
        return;
    }

    motor_id = RS_Motor->FDCANFrame.TxIdentifier & MOTOR_RS_ID_MOTOR_MASK;
    RS_PrepareFrame(FDCAN_TxFrame, motor_id | MOTOR_RS_MODE_POSITION_ID);

    RS_WriteFloatLE(&FDCAN_TxFrame->Data[0], Position);
    RS_WriteFloatLE(&FDCAN_TxFrame->Data[4], Velocity);

    USER_FDCAN_AddMessageToTxFifoQ(FDCAN_TxFrame);
}

void RS_Motor_Speed_CAN_TxMessage(FDCAN_TxFrame_TypeDef *FDCAN_TxFrame,
                                  Motor_RS_Info_Typedef *RS_Motor,
                                  float Velocity, float Current_Limit)
{
    uint32_t motor_id;

    if (FDCAN_TxFrame == NULL || RS_Motor == NULL) {
        return;
    }

    motor_id = RS_Motor->FDCANFrame.TxIdentifier & MOTOR_RS_ID_MOTOR_MASK;
    RS_PrepareFrame(FDCAN_TxFrame, motor_id | MOTOR_RS_MODE_SPEED_ID);

    RS_WriteFloatLE(&FDCAN_TxFrame->Data[0], Velocity);
    RS_WriteFloatLE(&FDCAN_TxFrame->Data[4], Current_Limit);

    USER_FDCAN_AddMessageToTxFifoQ(FDCAN_TxFrame);
}

void RS_Motor_Info_Update(uint32_t *Identifier, uint8_t *Rx_Buf,
                          Motor_RS_Info_Typedef *RS_Motor)
{
    uint16_t position_int;
    uint16_t velocity_int;
    uint16_t torque_int;
    uint16_t temperature_int;
    uint32_t motor_id;

    if (Identifier == NULL || Rx_Buf == NULL || RS_Motor == NULL) {
        return;
    }

    motor_id = RS_Motor->FDCANFrame.TxIdentifier & MOTOR_RS_ID_MOTOR_MASK;

    /* Feedback uses the host CAN ID; Byte0 carries the motor CAN ID. */
    if (*Identifier != (RS_Motor->FDCANFrame.RxIdentifier & MOTOR_RS_ID_MOTOR_MASK)) {
        return;
    }
    if (Rx_Buf[0] != (uint8_t)motor_id) {
        return;
    }

    position_int    = ((uint16_t)Rx_Buf[1] << 8) | (uint16_t)Rx_Buf[2];
    velocity_int    = ((uint16_t)Rx_Buf[3] << 4) | ((uint16_t)Rx_Buf[4] >> 4);
    torque_int      = (((uint16_t)Rx_Buf[4] & 0x0FU) << 8) | (uint16_t)Rx_Buf[5];
    temperature_int = (((uint16_t)Rx_Buf[6] & 0x0FU) << 8) | (uint16_t)Rx_Buf[7];

    RS_Motor->Data.P_int = position_int;
    RS_Motor->Data.V_int = velocity_int;
    RS_Motor->Data.T_int = torque_int;
    RS_Motor->Data.Temperature_int = temperature_int;

    RS_Motor->Data.Position = RS_Uint_To_Float(position_int,
                                               -RS_Motor->Param_Range.P_MAX,
                                                RS_Motor->Param_Range.P_MAX, 16U);
    RS_Motor->Data.Velocity = RS_Uint_To_Float(velocity_int,
                                               -RS_Motor->Param_Range.V_MAX,
                                                RS_Motor->Param_Range.V_MAX, 12U);
    RS_Motor->Data.Torque = RS_Uint_To_Float(torque_int,
                                             -RS_Motor->Param_Range.T_MAX,
                                              RS_Motor->Param_Range.T_MAX, 12U);
    RS_Motor->Data.Temperature = (float)temperature_int / 10.0f;

    RS_Motor->Data.Mode_State = (uint8_t)(Rx_Buf[6] >> 6);
    RS_Motor->Data.Fault   = ((Rx_Buf[6] & 0x20U) != 0U);
    RS_Motor->Data.Warning = ((Rx_Buf[6] & 0x10U) != 0U);
    RS_Motor->Data.Initialized = true;

    /* Same online/offline convention as Motor_DM. */
    RS_Motor->Data.online_cnt = 0xFAU;
    RS_Motor->Data.offline    = false;
}

/*------------------------------------------------------------------------------*/
/* Private helpers                                                              */
/*------------------------------------------------------------------------------*/

/**
  * @brief  Map a float in [x_min, x_max] to an unsigned integer of `bits` bits.
  *         Values outside the range are clamped; NaN maps to 0.
  */
static uint16_t RS_Float_To_Uint(float x, float x_min, float x_max, uint8_t bits)
{
    uint32_t max_int;
    float span;

    if (bits == 0U || bits > 16U) {
        return 0U;
    }

    max_int = (1UL << bits) - 1UL;
    span = x_max - x_min;

    if (!(x > x_min)) {
        return 0U;
    }
    if (x >= x_max) {
        return (uint16_t)max_int;
    }

    return (uint16_t)((x - x_min) * (float)max_int / span);
}

/**
  * @brief  Map an unsigned integer of `bits` bits to [x_min, x_max].
  */
static float RS_Uint_To_Float(uint16_t x, float x_min, float x_max, uint8_t bits)
{
    uint32_t max_int;

    if (bits == 0U || bits > 16U) {
        return x_min;
    }

    max_int = (1UL << bits) - 1UL;
    return (float)x * (x_max - x_min) / (float)max_int + x_min;
}

/**
  * @brief  Write a float as little-endian IEEE-754 bytes (ARM native order).
  */
static void RS_WriteFloatLE(uint8_t *buf, float value)
{
    uint32_t raw;
    uint8_t i;

    memcpy(&raw, &value, sizeof(raw));
    for (i = 0U; i < 4U; i++) {
        buf[i] = (uint8_t)(raw >> (8U * i));
    }
}

/**
  * @brief  Configure a transmit frame for a RobStride classic CAN frame.
  */
static void RS_PrepareFrameType(FDCAN_TxFrame_TypeDef *frame, uint32_t identifier, uint32_t id_type)
{
    frame->Header.Identifier = identifier;
    frame->Header.IdType = id_type;
    frame->Header.TxFrameType = FDCAN_DATA_FRAME;
    frame->Header.DataLength = 8U;
    frame->Header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    frame->Header.BitRateSwitch = FDCAN_BRS_OFF;
    frame->Header.FDFormat = FDCAN_CLASSIC_CAN;
    frame->Header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    frame->Header.MessageMarker = 0U;
}

/**
  * @brief  Configure a transmit frame with a standard 11-bit identifier.
  */
static void RS_PrepareFrame(FDCAN_TxFrame_TypeDef *frame, uint32_t identifier)
{
    RS_PrepareFrameType(frame, identifier, FDCAN_STANDARD_ID);
}


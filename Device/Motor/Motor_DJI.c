//
// Created by asus on 26-1-20.
// From another project.
#include "Motor_DJI.h"

uint8_t fdcanx_send_data(FDCAN_HandleTypeDef *hfdcan, uint16_t id, uint8_t *data, uint32_t len)
{
    FDCAN_TxHeaderTypeDef pTxHeader;
    pTxHeader.Identifier=id;
    pTxHeader.IdType=FDCAN_STANDARD_ID;
    pTxHeader.TxFrameType=FDCAN_DATA_FRAME;

    if(len<=8)
        pTxHeader.DataLength = len;
    else if(len==12)
        pTxHeader.DataLength = FDCAN_DLC_BYTES_12;
    else if(len==16)
        pTxHeader.DataLength = FDCAN_DLC_BYTES_16;
    else if(len==20)
        pTxHeader.DataLength = FDCAN_DLC_BYTES_20;
    else if(len==24)
        pTxHeader.DataLength = FDCAN_DLC_BYTES_24;
    else if(len==32)
        pTxHeader.DataLength = FDCAN_DLC_BYTES_32;
    else if(len==48)
        pTxHeader.DataLength = FDCAN_DLC_BYTES_48;
    else if(len==64)
        pTxHeader.DataLength = FDCAN_DLC_BYTES_64;

    pTxHeader.ErrorStateIndicator=FDCAN_ESI_ACTIVE;
    pTxHeader.BitRateSwitch=FDCAN_BRS_ON;
    pTxHeader.FDFormat=FDCAN_FD_CAN;
    pTxHeader.TxEventFifoControl=FDCAN_NO_TX_EVENTS;
    pTxHeader.MessageMarker=0;

    if(HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &pTxHeader, data)!=HAL_OK)
        return 1;//发送
    return 0;
}

void GM6020_motor_vol_ctrl(FDCAN_HandleTypeDef* hcan, uint16_t dji_motor_identifier, int16_t id_1_vol, int16_t id_2_vol,int16_t id_3_vol, int16_t id_4_vol)
{
    uint8_t data[8];

    data[0] = (id_1_vol >> 8);
    data[1] = id_1_vol;
    data[2] = (id_2_vol >> 8);
    data[3] = id_2_vol;
    data[4] = (id_3_vol >> 8);
    data[5] = id_3_vol;
    data[6] = (id_4_vol >> 8);
    data[7] = id_4_vol;

    fdcanx_send_data(hcan, dji_motor_identifier, data, 8);
}

void M3508_motor_crt_ctrl(FDCAN_HandleTypeDef* hcan, uint16_t dji_motor_identifier, int16_t id_1_crt, int16_t id_2_crt,int16_t id_3_crt, int16_t id_4_crt)
{
    GM6020_motor_vol_ctrl(hcan,dji_motor_identifier,id_1_crt,id_2_crt,id_3_crt,id_4_crt);
}

void M2006_motor_crt_ctrl(FDCAN_HandleTypeDef* hcan, uint16_t dji_motor_identifier, int16_t id_1_crt, int16_t id_2_crt,int16_t id_3_crt, int16_t id_4_crt)
{
    GM6020_motor_vol_ctrl(hcan,dji_motor_identifier,id_1_crt,id_2_crt,id_3_crt,id_4_crt);
}

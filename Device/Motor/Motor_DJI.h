//
// Created by asus on 26-1-20.
//

#ifndef MOTOR_DJI_H
#define MOTOR_DJI_H
#include "bsp_can.h"
#endif //MOTOR_DJI_H

uint8_t fdcanx_send_data(FDCAN_HandleTypeDef *hfdcan, uint16_t id, uint8_t *data, uint32_t len);
void GM6020_motor_vol_ctrl(FDCAN_HandleTypeDef* hcan, uint16_t dji_motor_identifier, int16_t id_1_vol, int16_t id_2_vol,int16_t id_3_vol, int16_t id_4_vol);
void M3508_motor_crt_ctrl(FDCAN_HandleTypeDef* hcan, uint16_t dji_motor_identifier, int16_t id_1_crt, int16_t id_2_crt,int16_t id_3_crt, int16_t id_4_crt);
void M2006_motor_crt_ctrl(FDCAN_HandleTypeDef* hcan, uint16_t dji_motor_identifier, int16_t id_1_crt, int16_t id_2_crt,int16_t id_3_crt, int16_t id_4_crt);

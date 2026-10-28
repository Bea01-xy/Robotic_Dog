/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : MiniPC.c
  * @brief          : MiniPC USB communication
  * @author         : GarssFan Wang
  * @date           : 2025/01/22
  * @version        : v1.0
  ******************************************************************************
  * @attention      : None
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "Minipc.h"
#include "usbd_cdc_if.h"
#include "usb_comm.h"
#include "main.h"
#include <math.h>
#include <string.h>

extern USBD_HandleTypeDef hUsbDeviceHS;

/* Legacy MiniPC frame: 0xAA + 57 payload bytes + checksum + 0x55. */
#define MINIPC_LEGACY_FRAME_SIZE    60U
#define MINIPC_LEGACY_CHECKSUM_IDX  58U
#define MINIPC_LEGACY_TAIL_IDX      59U

typedef union {
    float f;
    uint8_t bytes[4];
} FloatConverter;

static float bytes_to_float_union(const uint8_t *buffer);
static void  float_to_bytes_union(float value, uint8_t *buffer);
static void  MiniPC_FeedBytes(const uint8_t *data, size_t length);
static void  MiniPC_ParseLegacyFrame(const uint8_t *frame);

/* Public variables ---------------------------------------------------------*/
MiniPC_DataTypeDef MiniPC_Data = {
    .joystick_x = 2048U,
    .joystick_y = 2048U,
    .lost = true
};

MiniPC_DataTypeDef MiniPC_Data_Last = {
    .joystick_x = 2048U,
    .joystick_y = 2048U,
    .lost = true
};

/* Private variables ---------------------------------------------------------*/
static uint8_t joint_data_transmit[27] = {0};
static uint8_t rx_frame[MINIPC_LEGACY_FRAME_SIZE];
static uint16_t rx_used = 0U;

/* Private functions ---------------------------------------------------------*/
static float bytes_to_float_union(const uint8_t *buffer)
{
    FloatConverter converter;
    for (int i = 0; i < 4; i++) {
        converter.bytes[i] = buffer[i];
    }
    return converter.f;
}

static void float_to_bytes_union(float value, uint8_t *buffer)
{
    FloatConverter converter;
    converter.f = value;
    for (int i = 0; i < 4; i++) {
        buffer[i] = converter.bytes[i];
    }
}

static void MiniPC_ParseLegacyFrame(const uint8_t *frame)
{
    MiniPC_DataTypeDef next = MiniPC_Data;
    const uint8_t *pbuf = &frame[1];
    uint32_t idx = 0U;

    for (uint32_t i = 0; i < 6U; i++) {
        const float position = bytes_to_float_union(&pbuf[idx]);
        if (!isfinite(position)) {
            return;
        }
        next.joint_pos_data[i] = position;
        idx += 4U;
    }

    next.main_buttons   = pbuf[idx++];
    next.handle_buttons = pbuf[idx++];
    next.joystick_x     = (uint16_t)(pbuf[idx] | (pbuf[idx + 1U] << 8));
    idx += 2U;
    next.joystick_y     = (uint16_t)(pbuf[idx] | (pbuf[idx + 1U] << 8));
    idx += 2U;
    next.mouse_x = (int16_t)(pbuf[idx] | (pbuf[idx + 1U] << 8));
    idx += 2U;
    next.mouse_y = (int16_t)(pbuf[idx] | (pbuf[idx + 1U] << 8));
    idx += 2U;
    next.mouse_z = (int16_t)(pbuf[idx] | (pbuf[idx + 1U] << 8));
    idx += 2U;
    next.mouse_left  = pbuf[idx++];
    next.mouse_right = pbuf[idx++];
    next.mouse_mid   = pbuf[idx++];
    next.key_w       = pbuf[idx++];
    next.key_s       = pbuf[idx++];
    next.key_a       = pbuf[idx++];
    next.key_d       = pbuf[idx++];
    next.key_shift   = pbuf[idx++];
    next.key_ctrl    = pbuf[idx++];
    next.key_q       = pbuf[idx++];
    next.key_e       = pbuf[idx++];
    next.key_r       = pbuf[idx++];
    next.key_f       = pbuf[idx++];
    next.key_g       = pbuf[idx++];
    next.key_z       = pbuf[idx++];
    next.key_x       = pbuf[idx++];
    next.key_c       = pbuf[idx++];
    next.key_v       = pbuf[idx++];
    next.key_b       = pbuf[idx++];
    next.key_1       = pbuf[idx++];
    next.key_2       = pbuf[idx++];

    next.controller_buttons = 0U;
    next.valid_joint_mask   = 0x3FU;
    next.command_sequence   = 0U;
    next.controller_time_ms = 0U;

    next.btn1 = (next.handle_buttons >> 0) & 0x01U;
    next.btn2 = (next.handle_buttons >> 1) & 0x01U;
    next.btn3 = (next.handle_buttons >> 2) & 0x01U;
    next.btn4 = (next.handle_buttons >> 3) & 0x01U;

    next.online_cnt = 0xFAU;
    next.lost = false;
    MiniPC_Data = next;
}

static void MiniPC_FeedBytes(const uint8_t *data, size_t length)
{
    for (size_t i = 0U; i < length; i++) {
        if (rx_used == 0U) {
            if (data[i] == 0xAAU) {
                rx_frame[rx_used++] = data[i];
            }
            continue;
        }

        rx_frame[rx_used++] = data[i];
        if (rx_used < MINIPC_LEGACY_FRAME_SIZE) {
            continue;
        }

        uint8_t checksum = 0U;
        for (uint16_t j = 1U; j < MINIPC_LEGACY_CHECKSUM_IDX; j++) {
            checksum = (uint8_t)(checksum + rx_frame[j]);
        }

        if (rx_frame[MINIPC_LEGACY_CHECKSUM_IDX] == checksum &&
            rx_frame[MINIPC_LEGACY_TAIL_IDX] == 0x55U) {
            MiniPC_ParseLegacyFrame(rx_frame);
            rx_used = 0U;
            continue;
        }

        /* Invalid frame: resynchronise on the next 0xAA. */
        uint16_t next = 1U;
        while (next < MINIPC_LEGACY_FRAME_SIZE && rx_frame[next] != 0xAAU) {
            next++;
        }
        if (next == MINIPC_LEGACY_FRAME_SIZE) {
            rx_used = 0U;
        } else {
            memmove(rx_frame, &rx_frame[next],
                    (size_t)(MINIPC_LEGACY_FRAME_SIZE - next));
            rx_used = (uint16_t)(MINIPC_LEGACY_FRAME_SIZE - next);
        }
    }
}

/* Public functions ----------------------------------------------------------*/
uint8_t MiniPC_Transmit_Info(float* Buf, uint16_t Len)
{
    if (Buf == NULL || Len != 6U) {
        return USBD_FAIL;
    }

    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (hUsbDeviceHS.dev_state != USBD_STATE_CONFIGURED ||
        hUsbDeviceHS.pClassData == NULL) {
        __set_PRIMASK(primask);
        return USBD_FAIL;
    }
    if (((USBD_CDC_HandleTypeDef *)hUsbDeviceHS.pClassData)->TxState != 0U) {
        __set_PRIMASK(primask);
        return USBD_BUSY;
    }

    joint_data_transmit[Len * 4U + 1U] = 0U;
    joint_data_transmit[0] = 0xAAU;
    for (uint8_t i = 0U; i < Len; i++) {
        float_to_bytes_union(Buf[i], &joint_data_transmit[1U + i * 4U]);
    }
    for (uint8_t i = 1U; i < Len * 4U + 1U; i++) {
        joint_data_transmit[Len * 4U + 1U] += joint_data_transmit[i];
    }
    joint_data_transmit[Len * 4U + 2U] = 0x55U;

    const uint8_t result = CDC_Transmit_HS(joint_data_transmit, Len * 4U + 3U);
    __set_PRIMASK(primask);
    return result;
}

void MiniPC_Receive_Info(void)
{
    uint8_t chunk[64];

    for (size_t i = 0U; i < 16U; ++i) {
        bool discontinuity;
        const size_t length = USBComm_Read(chunk, sizeof(chunk), &discontinuity);
        if (discontinuity) {
            rx_used = 0U;
        }
        if (length == 0U) {
            break;
        }
        MiniPC_FeedBytes(chunk, length);
    }
}

void MiniPC_Offline_Monitor(void)
{
    if (MiniPC_Data.online_cnt <= 0x32U) {
        MiniPC_Data.lost = true;
    } else if (MiniPC_Data.online_cnt > 0U) {
        MiniPC_Data.online_cnt--;
    }
}

void MiniPC_Data_Update_Last(void)
{
    MiniPC_Data_Last = MiniPC_Data;
}

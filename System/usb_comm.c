#include "usb_comm.h"
#include "main.h"

#define USB_COMM_CAPACITY 1024U

static uint8_t rx_buffer[USB_COMM_CAPACITY];
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;
static volatile bool stream_reset;
static volatile uint32_t dropped_bytes;

void USBComm_PushFromISR(const uint8_t *data, size_t length)
{
    if (data == NULL) {
        return;
    }
    for (size_t i = 0; i < length; ++i) {
        const uint16_t next = (uint16_t)((rx_head + 1U) % USB_COMM_CAPACITY);
        if (next == rx_tail) {
            ++dropped_bytes;
            /* Discard the broken stream before accepting subsequent bytes. */
            rx_tail = rx_head;
            stream_reset = true;
        } else {
            rx_buffer[rx_head] = data[i];
            __DMB();
            rx_head = next;
        }
    }
}

void USBComm_ResetFromISR(void)
{
    rx_head = 0U;
    rx_tail = 0U;
    stream_reset = true;
}

size_t USBComm_Read(uint8_t *data, size_t capacity, bool *discontinuity)
{
    if (data == NULL || discontinuity == NULL) {
        return 0U;
    }
    size_t count = 0U;
    const uint32_t primask = __get_PRIMASK();
    __disable_irq();
    *discontinuity = stream_reset;
    stream_reset = false;
    while (count < capacity && rx_tail != rx_head) {
        data[count++] = rx_buffer[rx_tail];
        rx_tail = (uint16_t)((rx_tail + 1U) % USB_COMM_CAPACITY);
    }
    __set_PRIMASK(primask);
    return count;
}

uint32_t USBComm_GetDroppedBytes(void)
{
    return dropped_bytes;
}

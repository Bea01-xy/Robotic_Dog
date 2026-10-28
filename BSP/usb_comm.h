#ifndef USB_COMM_H
#define USB_COMM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* One USB ISR producer and one task consumer. */
void USBComm_PushFromISR(const uint8_t *data, size_t length);
void USBComm_ResetFromISR(void);
size_t USBComm_Read(uint8_t *data, size_t capacity, bool *discontinuity);
uint32_t USBComm_GetDroppedBytes(void);

#endif /* USB_COMM_H */

/**
 * @file    can_bus.h
 * @brief   FDCAN1 / FDCAN2 in classic CAN mode, 500 kbit/s (set in CubeMX).
 *
 * Bus 1 = FDCAN1 (PD0/PD1, U3), bus 2 = FDCAN2 (PB5/PB6, U8).
 * All received frames go into one queue that the main loop drains with can_bus_pop().
 */
#ifndef CAN_BUS_H
#define CAN_BUS_H

#include <stdint.h>
#include "main.h"
#include "ddaq_config.h"

typedef struct {
    uint32_t t_ms;      /* HAL_GetTick() at reception */
    uint32_t id;
    uint8_t  bus;       /* DDAQ_CAN_BUS_1 or DDAQ_CAN_BUS_2 */
    uint8_t  extended;  /* 1 = 29-bit ID */
    uint8_t  dlc;       /* 0..8 */
    uint8_t  data[8];
} can_frame_t;

/** Accept every frame into RX FIFO 0, enable the RX interrupt and start both buses. */
HAL_StatusTypeDef can_bus_init(void);

/** Queue a classic data frame with an 11-bit ID. HAL_BUSY if the TX FIFO is full. */
HAL_StatusTypeDef can_bus_send(uint8_t bus, uint32_t id, const uint8_t *data, uint8_t len);

/** Take the oldest received frame. Returns 0 when the queue is empty. */
uint8_t can_bus_pop(can_frame_t *out);

/** Increments on every frame sent or received (for the CAN activity LED). */
uint32_t can_bus_activity(void);

/** Frames lost because the RX queue was full. */
uint32_t can_bus_rx_overflows(void);

#endif /* CAN_BUS_H */

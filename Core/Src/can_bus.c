/**
 * @file    can_bus.c
 * @brief   FDCAN start-up, RX interrupt -> queue, TX helper.
 */
#include "can_bus.h"
#include "fdcan.h"
#include "logger.h"

static can_frame_t       rxq[DDAQ_CAN_RX_QUEUE_LEN];
static volatile uint32_t rxq_head, rxq_tail;   /* head: ISR, tail: main loop */
static volatile uint32_t activity;
static volatile uint32_t overflows;

static FDCAN_HandleTypeDef *handle(uint8_t bus)
{
    return (bus == DDAQ_CAN_BUS_2) ? &hfdcan2 : &hfdcan1;
}

static HAL_StatusTypeDef start_one(FDCAN_HandleTypeDef *h)
{
    /* No ID filters: every standard and extended data frame goes to FIFO 0. */
    if (HAL_FDCAN_ConfigGlobalFilter(h, FDCAN_ACCEPT_IN_RX_FIFO0, FDCAN_ACCEPT_IN_RX_FIFO0,
                                     FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE) != HAL_OK) return HAL_ERROR;
    if (HAL_FDCAN_ActivateNotification(h, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK) return HAL_ERROR;
    return HAL_FDCAN_Start(h);
}

HAL_StatusTypeDef can_bus_init(void)
{
    rxq_head = rxq_tail = 0;
    HAL_StatusTypeDef a = start_one(&hfdcan1);
    HAL_StatusTypeDef b = start_one(&hfdcan2);
    return (a == HAL_OK && b == HAL_OK) ? HAL_OK : HAL_ERROR;
}

HAL_StatusTypeDef can_bus_send(uint8_t bus, uint32_t id, const uint8_t *data, uint8_t len)
{
    FDCAN_HandleTypeDef *h = handle(bus);
    if (len > 8u) len = 8u;
    if (HAL_FDCAN_GetTxFifoFreeLevel(h) == 0u) return HAL_BUSY;

    FDCAN_TxHeaderTypeDef hdr = {0};
    hdr.Identifier          = id & 0x7FFu;
    hdr.IdType              = FDCAN_STANDARD_ID;
    hdr.TxFrameType         = FDCAN_DATA_FRAME;
    hdr.DataLength          = len;              /* FDCAN_DLC_BYTES_0..8 are the values 0..8 on G4 */
    hdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    hdr.BitRateSwitch       = FDCAN_BRS_OFF;
    hdr.FDFormat            = FDCAN_CLASSIC_CAN;
    hdr.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    hdr.MessageMarker       = 0;

    HAL_StatusTypeDef st = HAL_FDCAN_AddMessageToTxFifoQ(h, &hdr, (uint8_t *)data);
    if (st == HAL_OK) activity++;
    return st;
}

/* Called by HAL from FDCANx_IT0_IRQHandler for both buses. */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) == 0u) return;

    FDCAN_RxHeaderTypeDef hdr;
    uint8_t data[8];

    while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, FDCAN_RX_FIFO0) > 0u) {
        if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &hdr, data) != HAL_OK) break;
        activity++;

        can_frame_t f;
        f.t_ms     = HAL_GetTick();
        f.id       = hdr.Identifier;
        f.bus      = (hfdcan->Instance == FDCAN2) ? DDAQ_CAN_BUS_2 : DDAQ_CAN_BUS_1;
        f.extended = (hdr.IdType == FDCAN_EXTENDED_ID) ? 1u : 0u;
        f.dlc      = (hdr.DataLength > 8u) ? 8u : (uint8_t)hdr.DataLength;
        for (uint8_t i = 0; i < 8u; i++) f.data[i] = (i < f.dlc) ? data[i] : 0u;

        /* TODO(T7): log every received frame to the SD card.
         *   - fill a log_can_t (see logger.h) from `f`
         *   - call logger_push(LOG_REC_CAN, &rec, sizeof rec)
         *   - then add LOG_REC_CAN decoding to Tools/decode_log.py (with T9)
         * Test: send frames from a USB-CAN adapter, check they appear in the decoded log. */

        if ((rxq_head - rxq_tail) < DDAQ_CAN_RX_QUEUE_LEN) {
            rxq[rxq_head % DDAQ_CAN_RX_QUEUE_LEN] = f;
            rxq_head++;
        } else {
            overflows++;
        }
    }
}

uint8_t can_bus_pop(can_frame_t *out)
{
    if (rxq_head == rxq_tail) return 0;
    *out = rxq[rxq_tail % DDAQ_CAN_RX_QUEUE_LEN];
    rxq_tail++;
    return 1;
}

uint32_t can_bus_activity(void)    { return activity; }
uint32_t can_bus_rx_overflows(void) { return overflows; }

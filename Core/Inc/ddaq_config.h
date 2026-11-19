/**
 * @file    ddaq_config.h
 * @brief   Board-wide constants for the DDAQ firmware.
 *
 * Everything that describes the board or the logging format lives here, so a
 * change (sample rate, buffer size, CAN IDs) is made in one place.
 *
 * Task list and owners: see the "DDAQ Firmware Task Board" doc.
 * Search the code for "TODO(T" to find every open task.
 */
#ifndef DDAQ_CONFIG_H
#define DDAQ_CONFIG_H

#include <stdint.h>

/* ---- Firmware identity -------------------------------------------------- */
#define DDAQ_FW_VERSION_MAJOR   0u
#define DDAQ_FW_VERSION_MINOR   1u

/* ---- Sampling ----------------------------------------------------------- */
#define DDAQ_SAMPLE_RATE_HZ     1000u   /* TIM6 update rate (170 MHz / 170 / 1000) */
#define DDAQ_NUM_AI             8u      /* ADS8688 channels AI0..AI7              */
#define DDAQ_NUM_AUX            2u      /* MCU ADC spare inputs AI8 (PC5), AI9 (PE8) */
#define DDAQ_NUM_FREQ           4u      /* TIM2 CH1..CH4 = FREQ1..FREQ4           */

/* TIM2 counts at 170 MHz / (16 + 1) = 10 MHz -> one tick = 0.1 us. */
#define DDAQ_FREQ_TICK_HZ       10000000u
/* No edge for this long -> the input is treated as stopped (period = 0). */
#define DDAQ_FREQ_TIMEOUT_MS    500u

/* ---- Logging ------------------------------------------------------------ */
#define DDAQ_LOG_BUF_SIZE       32768u  /* RAM ring buffer, must be a power of 2  */
#define DDAQ_LOG_CHUNK          4096u   /* bytes per f_write (multiple of 512)    */
#define DDAQ_LOG_SYNC_MS        1000u   /* f_sync period = max data lost on power cut */

/* ---- CAN ---------------------------------------------------------------- */
#define DDAQ_CAN_BUS_1          0u      /* FDCAN1, U3, CAN1H/CAN1L (pass-through) */
#define DDAQ_CAN_BUS_2          1u      /* FDCAN2, U8, CAN2H/CAN2L                */
#define DDAQ_CAN_RX_QUEUE_LEN   32u     /* frames buffered between ISR and main loop */

/* TODO(T6): agree the DDAQ CAN IDs with the rest of the car and put them here.
 * The base ID is offset by the node ID (0..3) so two DDAQs can share a bus. */
#define DDAQ_CAN_ID_BASE        0x500u

/* ---- Fault flags (shown on the FAULT LED, see leds.c) ------------------- */
#define DDAQ_FAULT_ADC          (1u << 0)   /* ADS8688 did not answer / verify failed */
#define DDAQ_FAULT_SD           (1u << 1)   /* no card, mount or write failed          */
#define DDAQ_FAULT_CAN          (1u << 2)   /* FDCAN start failed                       */
#define DDAQ_FAULT_LOG_OVERFLOW (1u << 3)   /* ring buffer full, samples were dropped   */

#endif /* DDAQ_CONFIG_H */

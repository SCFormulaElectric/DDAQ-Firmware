/**
 * @file    board_io.h
 * @brief   Small board functions: status LEDs (T1), node-ID jumpers (T2),
 *          spare analog inputs AI8/AI9 on the MCU ADC (T3).
 */
#ifndef BOARD_IO_H
#define BOARD_IO_H

#include <stdint.h>
#include "ddaq_config.h"

/* ---- LEDs (T1): D30 CAN (PE0, yellow), D31 FAULT (PE1, red), D32 STATUS (PE2, green) */
typedef enum { LED_CAN = 0, LED_FAULT, LED_STAT } led_t;

void leds_set(led_t led, uint8_t on);
/** Called every main-loop pass: heartbeat, fault and CAN-activity patterns. */
void leds_service(uint32_t now_ms, uint32_t fault_flags, uint32_t can_activity);

/* ---- Node ID (T2): jumpers JP3 (PC0, bit 0) and JP4 (PC1, bit 1) ------- */
/** 0..3. Open jumper = 1 (10k pull-up), bridged jumper = 0. */
uint8_t node_id_read(void);

/* ---- Spare analog AI8 (PC5, ADC2_IN11) and AI9 (PE8, ADC3_IN6) (T3) --- */
void    aux_adc_init(void);
/** Read both channels once (12-bit raw). Called at 1 kHz from the TIM6 interrupt, keep it short. */
void    aux_adc_read(uint16_t out[DDAQ_NUM_AUX]);
/** Raw 12-bit code -> volts at the connector pin (undoes the 10k/15k divider). */
float   aux_adc_to_volts(uint16_t raw);

#endif /* BOARD_IO_H */

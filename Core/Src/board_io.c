/**
 * @file    board_io.c
 * @brief   LEDs, node-ID jumpers and the MCU's own ADC inputs.
 */
#include "board_io.h"
#include "main.h"
#include "adc.h"

/* ========================================================================
 * LEDs
 * ======================================================================== */

void leds_set(led_t led, uint8_t on)
{
    GPIO_PinState s = on ? GPIO_PIN_SET : GPIO_PIN_RESET;   /* active high: pin -> resistor -> LED anode */
    switch (led) {
    case LED_CAN:   HAL_GPIO_WritePin(LED_CAN_GPIO_Port,   LED_CAN_Pin,   s); break;
    case LED_FAULT: HAL_GPIO_WritePin(LED_FAULT_GPIO_Port, LED_FAULT_Pin, s); break;
    case LED_STAT:  HAL_GPIO_WritePin(LED_STAT_GPIO_Port,  LED_STAT_Pin,  s); break;
    default: break;
    }
}

void leds_service(uint32_t now_ms, uint32_t fault_flags, uint32_t can_activity)
{
    enum {
        HEARTBEAT_PERIOD_MS = 1000,
        HEARTBEAT_ON_MS = 100,
        CAN_FLASH_MS = 20
    };
    static uint32_t heartbeat_start_ms;
    static uint32_t last_can_activity;
    static uint32_t can_flash_start_ms;
    static uint8_t can_flash_active;

    /* Unsigned elapsed time preserves the heartbeat phase across tick rollover. */
    uint32_t heartbeat_elapsed_ms = now_ms - heartbeat_start_ms;
    if (heartbeat_elapsed_ms >= HEARTBEAT_PERIOD_MS) {
        heartbeat_elapsed_ms %= HEARTBEAT_PERIOD_MS;
        heartbeat_start_ms = now_ms - heartbeat_elapsed_ms;
    }
    leds_set(LED_STAT, heartbeat_elapsed_ms < HEARTBEAT_ON_MS);
    leds_set(LED_FAULT, fault_flags != 0u);

    if (can_activity != last_can_activity) {
        last_can_activity = can_activity;
        can_flash_start_ms = now_ms;
        can_flash_active = 1u;
    }
    can_flash_active = can_flash_active && (now_ms - can_flash_start_ms < CAN_FLASH_MS);
    leds_set(LED_CAN, can_flash_active);
}

/* ========================================================================
 * Node ID
 * ======================================================================== */

uint8_t node_id_read(void)
{
    /* TODO(T2): read the two jumpers once at start-up.
     *   bit 0 = HAL_GPIO_ReadPin(NODE_ID0_GPIO_Port, NODE_ID0_Pin)
     *   bit 1 = HAL_GPIO_ReadPin(NODE_ID1_GPIO_Port, NODE_ID1_Pin)
     *   open jumper reads 1 (pull-up), bridged reads 0
     * Return 0..3. It is stored in the log header and offsets the CAN IDs (T6).
     * Test: bridge JP3 only -> 2 (binary 10), nothing bridged -> 3. */
    return 0;
}

/* ========================================================================
 * Spare analog AI8 / AI9 (MCU ADC, 12-bit, 3.3 V reference)
 * ======================================================================== */

void aux_adc_init(void)
{
    /* TODO(T3): run the ADC self-calibration once before the first conversion:
     *   HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED); same for hadc3. */
}

void aux_adc_read(uint16_t out[DDAQ_NUM_AUX])
{
    /* TODO(T3): one conversion per ADC, polled:
     *   HAL_ADC_Start(&hadc2); HAL_ADC_PollForConversion(&hadc2, 1); out[0] = HAL_ADC_GetValue(&hadc2);
     *   same with hadc3 -> out[1]
     * This runs inside the 1 kHz interrupt: start both ADCs first, then poll both,
     * so the two conversions overlap. Consider raising the sampling time in CubeMX
     * (ADC2/ADC3 -> Rank 1 -> Sampling Time) from 2.5 to 47.5 cycles for less noise.
     * Test: 2.50 V on AI8 -> about 1.50 V at PC5 -> raw about 1860. */
    out[0] = 0;
    out[1] = 0;
}

float aux_adc_to_volts(uint16_t raw)
{
    /* TODO(T3): connector volts. The pin sees Vin * 15k / (10k + 15k) = 0.6 * Vin
     * (R126/R127 for AI8, R128/R129 for AI9) and the ADC full scale is 3.3 V = 4095. */
    (void)raw;
    return 0.0f;
}

/**
 * @file    freq_in.h
 * @brief   Frequency inputs FREQ1..FREQ4 (wheel speed / hall sensors) on TIM2 CH1..CH4.
 *
 * Hardware: each input goes through a 2.2k pull-up, 10k + 1 nF filter, clamp
 * and an SN74LVC2G17 Schmitt buffer into PA0, PA1, PB10, PB11.
 * TIM2 is a 32-bit timer counting at 10 MHz; every rising edge latches the
 * count, and the difference between two edges is the period.
 */
#ifndef FREQ_IN_H
#define FREQ_IN_H

#include <stdint.h>
#include "ddaq_config.h"

/** Start input capture interrupts on all four channels. */
void freq_in_start(void);

/** Last measured period in TIM2 ticks (0.1 us each). 0 = no edge for DDAQ_FREQ_TIMEOUT_MS. */
uint32_t freq_in_get_period_ticks(uint8_t ch);

/** TODO(T5): frequency in Hz (0.0 when stopped). */
float freq_in_get_hz(uint8_t ch);

/** TODO(T5): wheel speed in km/h for a sensor reading `teeth_per_rev` teeth on a wheel of `tire_circumference_m`. */
float freq_in_get_wheel_kph(uint8_t ch, uint16_t teeth_per_rev, float tire_circumference_m);

#endif /* FREQ_IN_H */

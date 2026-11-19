/**
 * @file    sensors.h
 * @brief   Turns raw readings into engineering units (T4) and broadcasts them on CAN (T6).
 */
#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>
#include "ddaq_config.h"
#include "ads8688.h"
#include "logger.h"

typedef struct {
    const char     *name;      /* e.g. "brake_front"            */
    const char     *unit;      /* e.g. "psi"                    */
    ads8688_range_t range;     /* ADC input range for this channel */
    float           v_min;     /* sensor output at eng_min (V)  */
    float           v_max;     /* sensor output at eng_max (V)  */
    float           eng_min;   /* value at v_min                */
    float           eng_max;   /* value at v_max                */
} sensor_cfg_t;

/** Per-channel config table AI0..AI7 (filled in by T4). */
extern const sensor_cfg_t sensor_cfg[DDAQ_NUM_AI];

/** Volts at the connector for channel ch. */
float sensors_ai_volts(uint8_t ch, uint16_t raw);

/** TODO(T4): engineering value (psi, mm, degC ...) for channel ch. */
float sensors_ai_value(uint8_t ch, uint16_t raw);

/** TODO(T6): called every main-loop pass; sends telemetry frames on CAN at a fixed rate. */
void telemetry_service(uint32_t now_ms, uint8_t node_id, const log_sample_t *latest);

#endif /* SENSORS_H */

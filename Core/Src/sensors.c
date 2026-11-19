/**
 * @file    sensors.c
 * @brief   Sensor scaling table (T4) and CAN telemetry (T6).
 */
#include "sensors.h"
#include "can_bus.h"
#include "freq_in.h"

/* TODO(T4): fill this table with the real sensors.
 *   - ask the team which sensor is wired to which AMPSEAL pin (AI0 = J3.2,
 *     AI1 = J3.7, AI2..AI7 = J3.13..J3.18)
 *   - from each sensor datasheet copy the output range (e.g. 0.5-4.5 V) and the
 *     value at each end (e.g. 0-2000 psi)
 *   - keep ADS8688_RANGE_UNIPOLAR_5V12 for 0-5 V sensors
 * The values below are placeholders: 0-5 V reads as 0-5 "V". */
const sensor_cfg_t sensor_cfg[DDAQ_NUM_AI] = {
    /* name   unit  range                         v_min v_max eng_min eng_max */
    { "AI0", "mm", ADS8688_RANGE_UNIPOLAR_5V12, 0.0f, 5.0f, 0.0f, 50.0f },
    { "AI1", "mm", ADS8688_RANGE_UNIPOLAR_5V12, 0.0f, 5.0f, 0.0f, 50.0f },
    { "AI2", "mm", ADS8688_RANGE_UNIPOLAR_5V12, 0.0f, 5.0f, 0.0f, 50.0f },
    { "AI3", "mm", ADS8688_RANGE_UNIPOLAR_5V12, 0.0f, 5.0f, 0.0f, 50.0f },
    { "AI4", "psi", ADS8688_RANGE_UNIPOLAR_5V12, 0.5f, 5.0f, 0.0f, 2250.0f },
    { "AI5", "psi", ADS8688_RANGE_UNIPOLAR_5V12, 0.5f, 5.0f, 0.0f, 2250.0f },
    { "AI6", "psi", ADS8688_RANGE_UNIPOLAR_5V12, 0.5f, 5.0f, 0.0f, 2250.0f },
    { "AI7", "psi", ADS8688_RANGE_UNIPOLAR_5V12, 0.5f, 5.0f, 0.0f, 2250.0f },
};

// 0.5 psi/mV 
// 10 mm / V

float sensors_ai_volts(uint8_t ch, uint16_t raw)
{
    if (ch >= DDAQ_NUM_AI) return 0.0f;
    /* The 1k series resistor and 100k pull-down drop ~1 % at the ADC input;
     * the ADS8688 input is 1 Mohm, so the error is small. Calibrate if needed (T4 stretch). */
    return ads8688_code_to_volts(raw, sensor_cfg[ch].range);
}

float sensors_ai_value(uint8_t ch, uint16_t raw)
{
    /* TODO(T4): linear map from volts to engineering units:
     *   value = eng_min + (volts - v_min) * (eng_max - eng_min) / (v_max - v_min)
     * Guard against v_max == v_min. Stretch: flag a broken sensor when the
     * voltage is outside v_min..v_max by more than 0.2 V (open or shorted wire).
     * Test: with the placeholder table, 2.50 V in must give 2.50 out. */
    if (ch >= DDAQ_NUM_AI) return 0.0f;

    float volts = sensors_ai_volts(ch, raw);

    float v_min = sensor_cfg[ch].v_min;
    float v_max = sensor_cfg[ch].v_max;
    float eng_min = sensor_cfg[ch].eng_min;
    float eng_max = sensor_cfg[ch].eng_max;

    if (v_max == v_min) return 0.0f;

    // start of eng range, plus percent of voltage range times range of eng
    return eng_min + (volts - v_min) * (eng_max - eng_min) / (v_max - v_min);
    
}

void telemetry_service(uint32_t now_ms, uint8_t node_id, const log_sample_t *latest)
{
    /* TODO(T6): broadcast live data so the dash / pit laptop can see it.
     *   - every 10 ms (100 Hz): two frames with the 8 AI raw values, 4 x uint16 each:
     *       ID DDAQ_CAN_ID_BASE + node_id * 0x10 + 0  -> AI0..AI3
     *       ID DDAQ_CAN_ID_BASE + node_id * 0x10 + 1  -> AI4..AI7
     *   - every 100 ms (10 Hz): one frame with the 4 wheel speeds in 0.1 km/h (uses T5)
     *   - pack little-endian: data[0] = v & 0xFF; data[1] = v >> 8;
     *   - send with can_bus_send(DDAQ_CAN_BUS_2, id, data, 8)
     *   - write the frame layout down in the task board doc so the dash team can decode it
     * Use a static "last sent" timestamp; never HAL_Delay().
     * Test: USB-CAN adapter + PCAN-View / SavvyCAN shows the IDs at the right rates. */
    (void)now_ms;
    (void)node_id;
    (void)latest;
}

/**
 * @file    ads8688.c
 * @brief   ADS8688 driver: reset, range setup, 8-channel auto-scan read.
 *
 * Every SPI frame is 32 clocks = two 16-bit words with CS low:
 *   word 0 (MOSI): command or register write   word 1 (MISO): conversion result
 *
 * TODO(T10): bench-verify this driver on a real board (see the task board):
 *   1. ads8688_init() must return ADS8688_OK. If it returns ADS8688_ERR_VERIFY,
 *      check the SPI mode (CPOL 0 / CPHA 1) and the read-back byte position in
 *      ads8688_read_reg() against the datasheet's register-read timing diagram.
 *   2. Put known voltages on AI0..AI7 (e.g. 1.0, 2.0, 3.0 V ...) and confirm
 *      out[0] is AI0, out[1] is AI1 and so on. If every value is shifted by one
 *      channel, the result arrives one frame later than assumed in
 *      ads8688_read_all(): add one extra NO_OP frame and drop the first result.
 *   3. Confirm 0 V reads near 0 and 5.00 V reads near 64000 counts on the 0-5.12 V range.
 */
#include "ads8688.h"
#include "main.h"
#include "spi.h"

/* Commands (datasheet "Command register" table) */
#define CMD_NO_OP       0x0000u
#define CMD_RST         0x8500u   /* software reset of the program registers */
#define CMD_AUTO_RST    0xA000u   /* enter auto-scan, next conversion = first enabled channel */

/* Program registers */
#define REG_AUTO_SEQ_EN 0x01u     /* bit n = channel n is in the auto-scan sequence */
#define REG_CH_PWR_DN   0x02u
#define REG_RANGE_CH0   0x05u     /* range of channel n is register 0x05 + n */

#define SPI_TIMEOUT_MS  2u

static inline void cs_low(void)  { HAL_GPIO_WritePin(ADC_CS_GPIO_Port, ADC_CS_Pin, GPIO_PIN_RESET); }
static inline void cs_high(void) { HAL_GPIO_WritePin(ADC_CS_GPIO_Port, ADC_CS_Pin, GPIO_PIN_SET); }

/* One 32-clock frame: send `cmd`, return the second 16-bit word from the ADC. */
static ads8688_status_t frame(uint16_t cmd, uint16_t *result)
{
    uint16_t tx[2] = { cmd, 0x0000u };
    uint16_t rx[2] = { 0, 0 };

    cs_low();
    HAL_StatusTypeDef st = HAL_SPI_TransmitReceive(&hspi1, (uint8_t *)tx, (uint8_t *)rx, 2, SPI_TIMEOUT_MS);
    cs_high();

    if (st != HAL_OK) {
        return ADS8688_ERR_SPI;
    }
    if (result != NULL) {
        *result = rx[1];
    }
    return ADS8688_OK;
}

ads8688_status_t ads8688_write_reg(uint8_t reg, uint8_t value)
{
    /* [15:9] address, [8] = 1 write, [7:0] data */
    uint16_t cmd = (uint16_t)(((uint16_t)reg << 9) | 0x0100u | value);
    return frame(cmd, NULL);
}

ads8688_status_t ads8688_read_reg(uint8_t reg, uint8_t *value)
{
    /* [15:9] address, [8] = 0 read. The register value comes back in the
     * upper byte of the second word (bits 24..31 of the frame). */
    uint16_t word = 0;
    ads8688_status_t st = frame((uint16_t)((uint16_t)reg << 9), &word);
    if (st == ADS8688_OK && value != NULL) {
        *value = (uint8_t)(word >> 8);
    }
    return st;
}

ads8688_status_t ads8688_init(const ads8688_range_t ranges[DDAQ_NUM_AI])
{
    ads8688_status_t st;

    /* Hardware reset/power-down pin is active low: hold the ADC out of reset. */
    HAL_GPIO_WritePin(ADC_RST_GPIO_Port, ADC_RST_Pin, GPIO_PIN_SET);
    cs_high();
    HAL_Delay(20);                         /* internal reference start-up */

    st = frame(CMD_RST, NULL);             /* program registers to defaults */
    if (st != ADS8688_OK) return st;
    HAL_Delay(1);

    for (uint8_t ch = 0; ch < DDAQ_NUM_AI; ch++) {
        st = ads8688_write_reg((uint8_t)(REG_RANGE_CH0 + ch), (uint8_t)ranges[ch]);
        if (st != ADS8688_OK) return st;
    }
    st = ads8688_write_reg(REG_CH_PWR_DN, 0x00u);     /* all channels powered */
    if (st != ADS8688_OK) return st;
    st = ads8688_write_reg(REG_AUTO_SEQ_EN, 0xFFu);   /* scan all 8 channels  */
    if (st != ADS8688_OK) return st;

    /* Read back one range register to prove the SPI link works. */
    uint8_t check = 0xFFu;
    st = ads8688_read_reg(REG_RANGE_CH0, &check);
    if (st != ADS8688_OK) return st;
    if (check != (uint8_t)ranges[0]) return ADS8688_ERR_VERIFY;

    return frame(CMD_AUTO_RST, NULL);
}

ads8688_status_t ads8688_read_all(uint16_t out[DDAQ_NUM_AI])
{
    /* AUTO_RST restarts the sequence every call so the channel order can
     * never drift. The frame after AUTO_RST returns channel 0, the next
     * channel 1, ... (to be confirmed on the bench, TODO(T10) above). */
    ads8688_status_t st = frame(CMD_AUTO_RST, NULL);
    if (st != ADS8688_OK) return st;

    for (uint8_t ch = 0; ch < DDAQ_NUM_AI; ch++) {
        st = frame(CMD_NO_OP, &out[ch]);
        if (st != ADS8688_OK) return st;
    }
    return ADS8688_OK;
}

float ads8688_code_to_volts(uint16_t code, ads8688_range_t range)
{
    const float lsb_5v12  = 5.12f  / 65536.0f;
    const float lsb_10v24 = 10.24f / 65536.0f;

    switch (range) {
    case ADS8688_RANGE_UNIPOLAR_5V12:  return (float)code * lsb_5v12;
    case ADS8688_RANGE_UNIPOLAR_10V24: return (float)code * lsb_10v24;
    /* Bipolar ranges: code 0x8000 = 0 V (offset binary). */
    case ADS8688_RANGE_BIPOLAR_10V24:  return ((float)code - 32768.0f) * (20.48f / 65536.0f);
    case ADS8688_RANGE_BIPOLAR_5V12:   return ((float)code - 32768.0f) * (10.24f / 65536.0f);
    case ADS8688_RANGE_BIPOLAR_2V56:   return ((float)code - 32768.0f) * (5.12f  / 65536.0f);
    default:                           return 0.0f;
    }
}

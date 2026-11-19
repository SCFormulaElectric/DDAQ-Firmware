/**
 * @file    ads8688.h
 * @brief   Driver for the TI ADS8688 16-bit, 8-channel ADC (U5) on SPI1.
 *
 * Wiring (DDAQ schematic): SCLK PA5, SDO->MISO PA6, SDI<-MOSI PA7,
 * CS# PA4 (ADC_CS), RST#/PD# PC4 (ADC_RST). Internal 4.096 V reference.
 * SPI1 is set up by CubeMX for 16-bit words, SPI mode 1, 5.3 MHz.
 */
#ifndef ADS8688_H
#define ADS8688_H

#include <stdint.h>
#include "ddaq_config.h"

/* Input range codes (program registers 0x05..0x0C). Vref = 4.096 V. */
typedef enum {
    ADS8688_RANGE_BIPOLAR_10V24 = 0x00, /* +-2.5  x Vref = +-10.24 V */
    ADS8688_RANGE_BIPOLAR_5V12  = 0x01, /* +-1.25 x Vref = +-5.12 V  */
    ADS8688_RANGE_BIPOLAR_2V56  = 0x02, /* +-0.625x Vref = +-2.56 V  */
    ADS8688_RANGE_UNIPOLAR_10V24 = 0x05, /* 0 .. 2.5 x Vref = 0..10.24 V */
    ADS8688_RANGE_UNIPOLAR_5V12  = 0x06, /* 0 .. 1.25x Vref = 0..5.12 V  (default for 0-5 V sensors) */
} ads8688_range_t;

typedef enum {
    ADS8688_OK = 0,
    ADS8688_ERR_SPI,     /* HAL SPI call failed or timed out              */
    ADS8688_ERR_VERIFY,  /* a register read back a different value        */
} ads8688_status_t;

/** Reset the ADC, program the range of every channel and enable auto-scan. */
ads8688_status_t ads8688_init(const ads8688_range_t ranges[DDAQ_NUM_AI]);

/** Read all 8 channels once (raw 16-bit codes, AI0..AI7). Called at 1 kHz from the TIM6 interrupt. */
ads8688_status_t ads8688_read_all(uint16_t out[DDAQ_NUM_AI]);

/** Raw code -> volts at the ADC pin for the given range. */
float ads8688_code_to_volts(uint16_t code, ads8688_range_t range);

/** Low-level register access, useful for bench testing (T10). */
ads8688_status_t ads8688_write_reg(uint8_t reg, uint8_t value);
ads8688_status_t ads8688_read_reg(uint8_t reg, uint8_t *value);

#endif /* ADS8688_H */

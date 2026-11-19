/**
 * @file    sd_spi.h
 * @brief   microSD card driver in SPI mode on SPI2 (used by FatFs via user_diskio.c).
 *
 * Wiring (DDAQ schematic): SCK PB13 (through R9 33R), MISO PB14 (DAT0),
 * MOSI PB15 (CMD), CS PB12 (DAT3), card power switch Q7 on PD9 (low = on),
 * card-detect PD8 (R141 pull-up).
 *
 * Speeds: card init at SPI2 /256 = 664 kHz, then /16 = 10.6 MHz.
 */
#ifndef SD_SPI_H
#define SD_SPI_H

#include <stdint.h>

typedef enum {
    SD_OK = 0,
    SD_ERR_NO_RESPONSE,  /* card did not answer CMD0 (missing, unpowered, wiring) */
    SD_ERR_INIT,         /* card answered but initialisation failed or timed out  */
    SD_ERR_READ,
    SD_ERR_WRITE,
    SD_ERR_TIMEOUT,
} sd_status_t;

/** Power the card, initialise it in SPI mode, switch to the fast clock. */
sd_status_t sd_spi_init(void);

/** 1 after a successful sd_spi_init(). */
uint8_t sd_spi_is_ready(void);

sd_status_t sd_spi_read(uint8_t *buf, uint32_t sector, uint32_t count);
sd_status_t sd_spi_write(const uint8_t *buf, uint32_t sector, uint32_t count);

/** Wait until the card has finished its internal write. */
sd_status_t sd_spi_sync(void);

/** Card capacity in 512-byte sectors (0 on error). */
uint32_t sd_spi_sector_count(void);

/** Card power switch (Q7). Drive the SPI2 lines low before powering off (see T11). */
void sd_spi_power(uint8_t on);

/** TODO(T11): 1 when a card is in the socket (PD8). */
uint8_t sd_spi_card_present(void);

#endif /* SD_SPI_H */

/**
 * @file    logger.c
 * @brief   Ring buffer + FatFs writer. See logger.h for the file format.
 */
#include "logger.h"
#include <string.h>
#include "main.h"
#include "app_fatfs.h"
#include "ff.h"

#define BUF_MASK (DDAQ_LOG_BUF_SIZE - 1u)

#if (DDAQ_LOG_BUF_SIZE & BUF_MASK) != 0
#error "DDAQ_LOG_BUF_SIZE must be a power of 2"
#endif

static uint8_t           ring[DDAQ_LOG_BUF_SIZE];
static volatile uint32_t head;      /* total bytes pushed  (written by producers, IRQs off) */
static volatile uint32_t tail;      /* total bytes written to the file (main loop only)      */

static volatile logger_state_t state = LOGGER_OFF;
static logger_stats_t stats;
static uint32_t last_sync_ms;
static char     filename[16] = "";

/* ---- File naming -------------------------------------------------------- */

static void make_filename(char out[16])
{
    /* TODO(T8): pick the next free name so old logs are never overwritten.
     *   - try LOG00001.BIN, LOG00002.BIN, ... and use the first one where
     *     f_stat(name, &info) returns FR_NO_FILE
     *   - names must stay 8.3 (long file names are disabled in ffconf.h)
     *   - snprintf(out, 16, "LOG%05u.BIN", n) builds the name
     * Test: boot three times with the same card -> LOG00001..LOG00003 exist. */
    strcpy(out, "LOG00000.BIN");
}

/* ---- Producer side (interrupt safe) ------------------------------------- */

static void ring_copy_in(uint32_t pos, const uint8_t *src, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        ring[(pos + i) & BUF_MASK] = src[i];
    }
}

uint8_t logger_push(uint8_t type, const void *payload, uint16_t len)
{
    log_rec_hdr_t hdr = { LOG_REC_SYNC, type, len };
    uint32_t total = (uint32_t)sizeof hdr + len;
    uint8_t ok = 0;

    /* Several interrupts (TIM6 samples, FDCAN frames) can push, so the
     * space check and copy must not be interrupted. ~1 us for 44 bytes. */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    if (state == LOGGER_RUNNING && (DDAQ_LOG_BUF_SIZE - (head - tail)) >= total) {
        ring_copy_in(head, (const uint8_t *)&hdr, sizeof hdr);
        ring_copy_in(head + sizeof hdr, (const uint8_t *)payload, len);
        head += total;
        ok = 1;
    } else {
        stats.bytes_dropped += total;
    }

    __set_PRIMASK(primask);
    return ok;
}

/* ---- Consumer side (main loop) ------------------------------------------ */

static uint8_t write_raw(const void *data, UINT n)
{
    UINT bw = 0;
    if (f_write(&USERFile, data, n, &bw) != FR_OK || bw != n) {
        stats.write_errors++;
        state = LOGGER_ERROR;
        return 0;
    }
    stats.bytes_written += bw;
    return 1;
}

logger_state_t logger_init(const log_file_header_t *header)
{
    head = tail = 0;
    memset(&stats, 0, sizeof stats);

    if (f_mount(&USERFatFs, USERPath, 1) != FR_OK) {
        state = LOGGER_NO_CARD;
        return state;
    }

    make_filename(filename);
    if (f_open(&USERFile, filename, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
        state = LOGGER_ERROR;
        return state;
    }

    state = LOGGER_RUNNING;
    if (!write_raw(header, sizeof *header) || f_sync(&USERFile) != FR_OK) {
        state = LOGGER_ERROR;
    }
    last_sync_ms = HAL_GetTick();
    return state;
}

void logger_service(uint32_t now_ms)
{
    if (state != LOGGER_RUNNING) {
        return;
    }

    uint32_t used = head - tail;
    if (used > stats.max_fill) stats.max_fill = used;

    uint8_t sync_due = (now_ms - last_sync_ms) >= DDAQ_LOG_SYNC_MS;

    /* Write whole chunks as they fill; on a sync, also write the partial rest. */
    while (used >= DDAQ_LOG_CHUNK || (sync_due && used > 0u)) {
        uint32_t pos = tail & BUF_MASK;
        uint32_t n = used;
        if (n > DDAQ_LOG_BUF_SIZE - pos) n = DDAQ_LOG_BUF_SIZE - pos;   /* up to the wrap */
        if (n > DDAQ_LOG_CHUNK) n = DDAQ_LOG_CHUNK;

        if (!write_raw(&ring[pos], (UINT)n)) return;
        tail += n;          /* frees the space for producers */
        used -= n;
    }

    if (sync_due) {
        if (f_sync(&USERFile) != FR_OK) {
            stats.write_errors++;
            state = LOGGER_ERROR;
        }
        last_sync_ms = now_ms;
    }
}

void logger_stop(void)
{
    if (state == LOGGER_RUNNING) {
        logger_service(HAL_GetTick() + DDAQ_LOG_SYNC_MS);   /* force a full flush + sync */
        f_close(&USERFile);
    }
    f_mount(NULL, USERPath, 0);
    state = LOGGER_OFF;

    /* TODO(T11): SD card hot-swap. In app_loop(), watch sd_spi_card_present():
     *   - card removed  -> logger_stop(); then drive PB12/PB13/PB15 low and call
     *                      sd_spi_power(0) so the card is not back-powered
     *   - card inserted -> sd_spi_power(1), wait ~50 ms, call logger_init() again
     *                      (with T8 done this opens a new file)
     *   - if logger_get_state() == LOGGER_ERROR for > 2 s, power-cycle the card
     *                      the same way and try again
     * Test: pull the card while logging, reinsert, check a new file appears and
     * the FAULT LED clears. */
}

logger_state_t logger_get_state(void)        { return state; }
const char    *logger_get_filename(void)     { return filename; }

void logger_get_stats(logger_stats_t *out)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    *out = stats;
    __set_PRIMASK(primask);
}

/* The PC decoder (Tools/decode_log.py) relies on these exact sizes. */
_Static_assert(sizeof(log_file_header_t) == 32, "log header must be 32 bytes");
_Static_assert(sizeof(log_rec_hdr_t) == 4, "record header must be 4 bytes");
_Static_assert(sizeof(log_sample_t) == 40, "sample record must be 40 bytes");
_Static_assert(sizeof(log_can_t) == 20, "CAN record must be 20 bytes");

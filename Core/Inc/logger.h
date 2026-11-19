/**
 * @file    logger.h
 * @brief   Binary data logger: interrupt-safe RAM ring buffer -> FatFs file on the SD card.
 *
 * File layout (little-endian, decoded by Tools/decode_log.py):
 *   log_file_header_t                       (32 bytes, once at the start)
 *   then records:  log_rec_hdr_t + payload  (repeated)
 *
 * Producers (interrupts) call logger_push(); the main loop calls
 * logger_service() which writes 4 KB chunks and syncs the file once a second.
 * If the card stalls, samples wait in RAM (32 KB = ~0.8 s of data).
 */
#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include "ddaq_config.h"

#define LOG_MAGIC        "DDAQLOG"  /* 7 chars + NUL = 8 bytes */
#define LOG_FORMAT_VER   1u
#define LOG_REC_SYNC     0xA5u

/* Record types */
#define LOG_REC_SAMPLE   0x01u      /* log_sample_t, 1 kHz              */
#define LOG_REC_CAN      0x02u      /* log_can_t, one per received frame (T7) */
#define LOG_REC_EVENT    0x03u      /* free text, e.g. "card reinserted" (spare) */

typedef struct __attribute__((packed)) {
    char     magic[8];              /* "DDAQLOG\0" */
    uint16_t format_version;        /* LOG_FORMAT_VER */
    uint16_t sample_rate_hz;
    uint8_t  node_id;
    uint8_t  n_ai;
    uint8_t  n_aux;
    uint8_t  n_freq;
    uint32_t freq_tick_hz;          /* TIM2 tick rate, to turn periods into Hz */
    uint8_t  ai_range[8];           /* ads8688_range_t per channel */
    uint8_t  fw_major;
    uint8_t  fw_minor;
    uint8_t  reserved[2];
} log_file_header_t;                /* 32 bytes */

typedef struct __attribute__((packed)) {
    uint8_t  sync;                  /* LOG_REC_SYNC, lets the decoder resync after corruption */
    uint8_t  type;                  /* LOG_REC_* */
    uint16_t len;                   /* payload bytes that follow */
} log_rec_hdr_t;

typedef struct __attribute__((packed)) {
    uint32_t t_ms;                  /* sample counter since boot (1 per ms) */
    uint16_t ai_raw[DDAQ_NUM_AI];   /* ADS8688 codes */
    uint16_t aux_raw[DDAQ_NUM_AUX]; /* MCU ADC codes, 12-bit */
    uint32_t freq_period[DDAQ_NUM_FREQ]; /* TIM2 ticks, 0 = stopped */
} log_sample_t;                     /* 40 bytes */

typedef struct __attribute__((packed)) {
    uint32_t t_ms;                  /* HAL_GetTick() when received */
    uint32_t id;                    /* 11- or 29-bit identifier */
    uint8_t  bus;                   /* DDAQ_CAN_BUS_1 / DDAQ_CAN_BUS_2 */
    uint8_t  dlc;                   /* 0..8 */
    uint8_t  flags;                 /* bit0 = extended ID */
    uint8_t  reserved;
    uint8_t  data[8];
} log_can_t;                        /* 20 bytes */

typedef enum {
    LOGGER_OFF = 0,                 /* not started */
    LOGGER_NO_CARD,                 /* mount failed: no card / not FAT32 */
    LOGGER_RUNNING,
    LOGGER_ERROR,                   /* open or write failed */
} logger_state_t;

typedef struct {
    uint32_t bytes_written;
    uint32_t bytes_dropped;         /* ring buffer was full */
    uint32_t write_errors;
    uint32_t max_fill;              /* highest ring-buffer use seen, bytes */
} logger_stats_t;

/** Mount the card, create the log file and write the header. */
logger_state_t logger_init(const log_file_header_t *header);

/** Copy one record into the ring buffer. Safe from any interrupt. Returns 0 if it did not fit. */
uint8_t logger_push(uint8_t type, const void *payload, uint16_t len);

/** Main-loop work: write buffered data to the card, sync once a second. */
void logger_service(uint32_t now_ms);

/** Flush everything and close the file (before removing the card / power-off). */
void logger_stop(void);

logger_state_t logger_get_state(void);
void logger_get_stats(logger_stats_t *out);
const char *logger_get_filename(void);

#endif /* LOGGER_H */

/**
 * @file    app.h
 * @brief   DDAQ application: start-up, 1 kHz sampling, main loop.
 *
 * main.c calls app_init() once after the CubeMX init functions and
 * app_loop() forever. Nothing else in main.c needs to change.
 */
#ifndef APP_H
#define APP_H

#include <stdint.h>
#include "logger.h"

void app_init(void);
void app_loop(void);

/** Copy of the newest 1 kHz sample (safe to call from the main loop). */
void app_get_latest(log_sample_t *out);

/** DDAQ_FAULT_* bits currently set. */
uint32_t app_get_faults(void);

#endif /* APP_H */

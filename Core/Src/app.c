/**
 * @file    app.c
 * @brief   Ties the modules together.
 *
 *   TIM6 interrupt, 1 kHz (priority 1):
 *     ADS8688 x8 + MCU ADC x2 + frequency periods x4 -> log_sample_t -> logger_push()
 *   TIM2 interrupt (priority 0): captures frequency-input edges (freq_in.c)
 *   FDCAN interrupts (priority 2): received frames -> queue (can_bus.c)
 *   Main loop: SD writes, CAN queue, telemetry, LEDs. Never blocks the sampling.
 */
#include "app.h"
#include <string.h>
#include "main.h"
#include "tim.h"
#include "ddaq_config.h"
#include "ads8688.h"
#include "freq_in.h"
#include "can_bus.h"
#include "board_io.h"
#include "sensors.h"

static volatile uint32_t faults;
static volatile uint32_t sample_count;
static volatile uint32_t adc_errors;
static log_sample_t      latest;
static uint8_t           node_id;

static void sample_tick(void);

void app_init(void)
{
    ads8688_range_t ranges[DDAQ_NUM_AI];

    node_id = node_id_read();
    aux_adc_init();

    for (uint8_t ch = 0; ch < DDAQ_NUM_AI; ch++) ranges[ch] = sensor_cfg[ch].range;
    if (ads8688_init(ranges) != ADS8688_OK) faults |= DDAQ_FAULT_ADC;

    freq_in_start();

    if (can_bus_init() != HAL_OK) faults |= DDAQ_FAULT_CAN;

    log_file_header_t hdr;
    memset(&hdr, 0, sizeof hdr);
    memcpy(hdr.magic, LOG_MAGIC, sizeof LOG_MAGIC);
    hdr.format_version = LOG_FORMAT_VER;
    hdr.sample_rate_hz = DDAQ_SAMPLE_RATE_HZ;
    hdr.node_id        = node_id;
    hdr.n_ai           = DDAQ_NUM_AI;
    hdr.n_aux          = DDAQ_NUM_AUX;
    hdr.n_freq         = DDAQ_NUM_FREQ;
    hdr.freq_tick_hz   = DDAQ_FREQ_TICK_HZ;
    for (uint8_t ch = 0; ch < DDAQ_NUM_AI; ch++) hdr.ai_range[ch] = (uint8_t)ranges[ch];
    hdr.fw_major       = DDAQ_FW_VERSION_MAJOR;
    hdr.fw_minor       = DDAQ_FW_VERSION_MINOR;
    if (logger_init(&hdr) != LOGGER_RUNNING) faults |= DDAQ_FAULT_SD;

    HAL_TIM_Base_Start_IT(&htim6);   /* start the 1 kHz sampling last */
}

void app_loop(void)
{
    uint32_t now = HAL_GetTick();

    logger_service(now);
    if (logger_get_state() != LOGGER_RUNNING) faults |= DDAQ_FAULT_SD;

    logger_stats_t ls;
    logger_get_stats(&ls);
    if (ls.bytes_dropped) faults |= DDAQ_FAULT_LOG_OVERFLOW;

    can_frame_t f;
    while (can_bus_pop(&f)) {
        /* Frames for the DDAQ itself (config commands, time sync ...) would be handled here. */
    }

    log_sample_t s;
    app_get_latest(&s);
    telemetry_service(now, node_id, &s);

    leds_service(now, faults, can_bus_activity());
}

/* Called by HAL from TIM6_DAC_IRQHandler. */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6) {
        sample_tick();
    }
}

static void sample_tick(void)
{
    log_sample_t s;
    uint16_t ai[DDAQ_NUM_AI] = {0};
    uint16_t aux[DDAQ_NUM_AUX] = {0};

    s.t_ms = ++sample_count;
    if (ads8688_read_all(ai) != ADS8688_OK) {
        adc_errors++;
        memset(ai, 0, sizeof ai);
    }
    aux_adc_read(aux);
    /* log_sample_t is packed, so copy instead of passing pointers into it. */
    memcpy(s.ai_raw, ai, sizeof ai);
    memcpy(s.aux_raw, aux, sizeof aux);
    for (uint8_t ch = 0; ch < DDAQ_NUM_FREQ; ch++) {
        s.freq_period[ch] = freq_in_get_period_ticks(ch);
    }

    latest = s;   /* main loop reads it with interrupts off, see app_get_latest() */
    logger_push(LOG_REC_SAMPLE, &s, sizeof s);
}

void app_get_latest(log_sample_t *out)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    *out = latest;
    __set_PRIMASK(primask);
}

uint32_t app_get_faults(void)
{
    return faults;
}

/**
 * @file    freq_in.c
 * @brief   TIM2 input-capture handling for FREQ1..FREQ4.
 */
#include "freq_in.h"
#include "main.h"
#include "tim.h"

static const uint32_t tim_channel[DDAQ_NUM_FREQ] = {
    TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4
};

static volatile uint32_t last_capture[DDAQ_NUM_FREQ];
static volatile uint32_t period_ticks[DDAQ_NUM_FREQ];
static volatile uint32_t last_edge_ms[DDAQ_NUM_FREQ];
static volatile uint8_t  have_first_edge[DDAQ_NUM_FREQ];

void freq_in_start(void)
{
    for (uint8_t ch = 0; ch < DDAQ_NUM_FREQ; ch++) {
        have_first_edge[ch] = 0;
        period_ticks[ch] = 0;
        HAL_TIM_IC_Start_IT(&htim2, tim_channel[ch]);
    }
}

/* Called by HAL from TIM2_IRQHandler on every captured edge. */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM2) {
        return;
    }

    uint8_t ch;
    switch (htim->Channel) {
    case HAL_TIM_ACTIVE_CHANNEL_1: ch = 0; break;
    case HAL_TIM_ACTIVE_CHANNEL_2: ch = 1; break;
    case HAL_TIM_ACTIVE_CHANNEL_3: ch = 2; break;
    case HAL_TIM_ACTIVE_CHANNEL_4: ch = 3; break;
    default: return;
    }

    uint32_t now = HAL_TIM_ReadCapturedValue(htim, tim_channel[ch]);
    if (have_first_edge[ch]) {
        /* Unsigned subtraction handles the 32-bit counter wrapping. */
        period_ticks[ch] = now - last_capture[ch];
    }
    last_capture[ch] = now;
    have_first_edge[ch] = 1;
    last_edge_ms[ch] = HAL_GetTick();
}

uint32_t freq_in_get_period_ticks(uint8_t ch)
{
    if (ch >= DDAQ_NUM_FREQ || !have_first_edge[ch]) {
        return 0;
    }
    if ((HAL_GetTick() - last_edge_ms[ch]) > DDAQ_FREQ_TIMEOUT_MS) {
        return 0;   /* wheel stopped or sensor unplugged */
    }
    return period_ticks[ch];
}

float freq_in_get_hz(uint8_t ch)
{
    /* TODO(T5): convert the period to Hz.
     *   - period = freq_in_get_period_ticks(ch); one tick = 1 / DDAQ_FREQ_TICK_HZ seconds
     *   - return 0.0f when the period is 0 (stopped)
     * Test: feed a 100 Hz square wave from a function generator into FREQ1 -> expect 100.0. */
    uint32_t tickCount =  freq_in_get_period_ticks(ch);
    //return 0.0 if no tick count
    if(!tickCount) {return 0.0f;}
    //frq is 1/ period
     float freq = 1 / ((float)tickCount/DDAQ_FREQ_TICK_HZ);
     return freq;
   
    


}

float freq_in_get_wheel_kph(uint8_t ch, uint16_t teeth_per_rev, float tire_circumference_m)
{
    /* TODO(T5): wheel speed from tooth frequency.
     *   revs per second = Hz / teeth_per_rev
     *   metres per second = revs per second * tire_circumference_m
     *   km/h = m/s * 3.6
     * Guard against teeth_per_rev == 0. Ask the suspension/vehicle-dynamics
     * team for the tooth count and tyre circumference, and add them to ddaq_config.h. */
    float freq = freq_in_get_hz(ch);

    if(!teeth_per_rev){
        return 0.0f;
    }

    float rps = freq / teeth_per_rev;
    float mps = rps * tire_circumference_m;
    float kmph = mps * 3.6f;
    return kmph;

}

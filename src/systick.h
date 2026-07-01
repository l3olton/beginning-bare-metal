#ifndef SYSTICK_H
#define SYSTICK_H

#include "rcc.h"
#include "utils.h"
#include <stdbool.h>
#include <stdint.h>

struct SysTick {
    volatile uint32_t CTRL, LOAD, VAL, CALIB;
};
#define SYSTICK ((struct SysTick *) 0xe000e010)

// TODO: macros for bits
static inline void systick_init(uint32_t ticks) {
    if ((ticks - 1) > 0xffffff) return; // limit value to 24-bit, systick timer is 24-bit counter
    SYSTICK->LOAD = ticks - 1; // value to countdown from
    SYSTICK->VAL = 0;
    SYSTICK->CTRL = BIT(0) // set bit 0 to enable the counter
        | BIT(1) // set bit 1 to enable interrupt when counter reaches 0
        | BIT(2); // set bit 2 to use system clock
    RCC->APB2ENR |= BIT(14); // enable system configuration controller clock (SYSCFGEN)
}

extern volatile uint32_t s_ticks;

void SysTick_Handler(void);

bool timer_expired(uint32_t *t, uint32_t prd, uint32_t now);
#define DELAY(timer, ms) while (!timer_expired(&timer, ms, s_ticks))

#endif

#include "gpio.h"
#include "usart.h"
#include "rcc.h"
#include "utils.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct SysTick {
    volatile uint32_t CTRL, LOAD, VAL, CALIB;
};
#define SYSTICK ((struct SysTick *) 0xe000e010)

#define FREQ 16000000 // CPU frequency 16Mhz

static inline void spin(volatile uint32_t count) {
  while (count--) (void) 0;
}

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

static volatile uint32_t s_ticks;
void SysTick_Handler(void) {
    s_ticks++;
}

bool timer_expired(uint32_t *t, uint32_t prd, uint32_t now) {
  if (now + prd < *t) *t = 0;                    // Time wrapped? Reset timer
  if (*t == 0) *t = now + prd;                   // First poll? Set expiration
  if (*t > now) return false;                    // Not expired yet, return
  *t = (now - *t) > prd ? now + prd : *t + prd;  // Next expiration time
  return true;                                   // Expired, return true
}
#define DELAY(timer, ms) while (!timer_expired(&timer, ms, s_ticks))

int main(void) {
    systick_init(16000000 / 1000);

    // usart_init(USART3, 0x008B); // divider for 115200 baud rate
    usart_init(USART3, 0x0683); // divider for 9600 baud rate

    usart_write_buffer(USART3, "بسم الله\n", 16);

    gpio_set_mode(BLUE_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_mode(GREEN_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_mode(RED_LED_PIN, GPIO_MODE_OUTPUT);

    uint32_t led_timer = 0;
    uint32_t usart_timer = 0;
    uint32_t delay_timer = 0;

    while (1) {
        if (timer_expired(&led_timer, 1500, s_ticks)) {
            gpio_write(RED_LED_PIN, 1);
            DELAY(delay_timer, 250);
            gpio_write(BLUE_LED_PIN, 1);
            DELAY(delay_timer, 250);
            gpio_write(GREEN_LED_PIN, 1);
            DELAY(delay_timer, 250);
            gpio_write(RED_LED_PIN, 0);
            DELAY(delay_timer, 250);
            gpio_write(BLUE_LED_PIN, 0);
            DELAY(delay_timer, 250);
            gpio_write(GREEN_LED_PIN, 0);
            DELAY(delay_timer, 250);
        }

        if (timer_expired(&usart_timer, 1000, s_ticks)) {
            usart_write_buffer(USART3, "لا إله إلا الله\n", 28);
        }
    }

    return 0;
}

__attribute__((naked, noreturn)) void _reset(void) {
    extern long _sbss, _ebss, _sdata, _edata, _sidata;
    for (long *dst = &_sbss; dst < &_ebss; dst++) *dst = 0;
    for (long *dst = &_sdata, *src = &_sidata; dst < &_edata;) *dst++ = *src++;

    main();
    while (1) (void) 0;
}

extern void _estack(void);

__attribute__((section(".vectors"))) void (*const tab[16 + 91])(void) = {
    _estack, _reset, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, SysTick_Handler
};

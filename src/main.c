#include "gpio.h"
#include "usart.h"
#include "systick.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define FREQ 16000000 // CPU frequency 16Mhz

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

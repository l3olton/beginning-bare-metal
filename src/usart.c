#include "usart.h"
#include "rcc.h"
#include "gpio.h"
#include <stddef.h>

void usart_init(struct Usart *usart, unsigned long usart_div) {
    uint8_t af = 7;
    uint16_t rx = 0, tx = 0;

    if (usart == USART1) {
        RCC->APB2ENR |= USART1_CLOCK_ENABLE;
        tx = PIN('A', 9); // select transmission pin
        rx = PIN('A', 10); // select receiving pin
    } else if (usart == USART2) {
        RCC->APB1ENR |= USART2_CLOCK_ENABLE;
        tx = PIN('A', 2);
        rx = PIN('A', 3);
    } else if (usart == USART3) {
        RCC->APB1ENR |= USART3_CLOCK_ENABLE;
        tx = PIN('D', 8);
        rx = PIN('D', 9);
    } else {
        return; // TODO: maybe handle differently
    }

    gpio_set_mode(tx, GPIO_MODE_AF);
    gpio_set_af(tx, af);
    gpio_set_mode(rx, GPIO_MODE_AF);
    gpio_set_af(rx, af);

    usart->CR1 &= ~BIT(12) & ~BIT(15); // set data length to 8 bits and OVER8 to 0 (oversample by 16 bits)
    usart->CR2 &= ~BIT(12) & ~BIT(13); // set no. of stop bits to 1
    usart->BRR = usart_div;
    usart->CR1 |= USART_ENABLE | USART_TRANSMITTER_ENABLE;
}

static inline void usart_write_byte(struct Usart *usart, uint8_t data) {
    while (!(usart->SR & USART_TRANSMISSION_COMPLETE));
    usart->DR = data;
}

void usart_write_buffer(struct Usart *usart, char *buffer, size_t len) {
    while (len-- > 0) usart_write_byte(usart, *(uint8_t *) buffer++);
}

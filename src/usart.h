#ifndef USART_H
#define USART_H

#include "utils.h"
#include <stddef.h>
#include <stdint.h>

struct Usart {
    volatile uint32_t SR, DR, BRR, CR1, CR2, CR3, GTPR;
};

#define USART1 ((struct Usart *) 0x40011000)
#define USART2 ((struct Usart *) 0x40004400)
#define USART3 ((struct Usart *) 0x40004800)

#define USART1_CLOCK_ENABLE BIT(4)
#define USART2_CLOCK_ENABLE BIT(17)
#define USART3_CLOCK_ENABLE BIT(18)
#define USART_ENABLE BIT(13)
#define USART_TRANSMITTER_ENABLE BIT(3)
#define USART_TRANSMISSION_COMPLETE BIT(6)

void usart_init(struct Usart *usart, unsigned long usart_div);

static inline int usart_read_ready(struct Usart *usart) {
    return usart->SR & BIT(5); // if RXNE bit is set, data is ready to read
}

static inline uint8_t usart_read_byte(struct Usart *usart) {
    return (uint8_t) (usart->DR & 255); // bottom 8 bits of DR register hold received value
}

void usart_write_buffer(struct Usart *usart, char *buffer, size_t len);

#endif

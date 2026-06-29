#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define BIT(x) (1UL << (x))
#define PIN(bank, num) ((((bank) - 'A') << 8) | (num))
#define PINNO(pin) (pin & 255)
#define PINBANK(pin) (pin >> 8)

struct Gpio {
    volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR, LCKR, AFR[2];
};
#define GPIO(bank) ((struct Gpio *) (0x40020000 + 0x400 * (bank)))

struct Rcc {
    volatile uint32_t CR, PLLCFGR, CFGR, CIR, AHB1RSTR, AHB2RSTR, AHB3RSTR,
        RESERVED0, APB1RSTR, APB2RSTR, RESERVED1[2], AHB1ENR, AHB2ENR, AHB3ENR,
        RESERVED2, APB1ENR, APB2ENR, RESERVED3[2], AHB1LPENR, AHB2LPENR,
        AHB3LPENR, RESERVED4, APB1LPENR, APB2LPENR, RESERVED5[2], BDCR, CSR,
        RESERVED6[2], SSCGR, PLLI2SCFGR;
};
#define RCC ((struct Rcc *) 0x40023800)

struct SysTick {
    volatile uint32_t CTRL, LOAD, VAL, CALIB;
};
#define SYSTICK ((struct SysTick *) 0xe000e010)

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

#define FREQ 16000000 // CPU frequency 16Mhz

typedef enum { GPIO_MODE_INPUT, GPIO_MODE_OUTPUT, GPIO_MODE_AF, GPIO_MODE_ANALOG } GpioMode;

static inline void spin(volatile uint32_t count) {
  while (count--) (void) 0;
}

// TODO: possibly rewrite
static inline void gpio_set_mode(uint16_t pin, GpioMode mode) {
    struct Gpio *gpio = GPIO(PINBANK(pin));
    int n = PINNO(pin);
    RCC->AHB1ENR |= BIT(PINBANK(pin)); // enable clock for pin
    gpio->MODER &= ~(3U << (n * 2)); // clear mode register
    gpio->MODER |= (mode & 3U) << (n * 2); // set mode register
}

// TODO: possibly rewrite
static inline void gpio_write(uint16_t pin, bool val) {
    struct Gpio *gpio = GPIO(PINBANK(pin));
    gpio->BSRR = (1U << PINNO(pin)) << (val ? 0 : 16);
}

// TODO: possibly rewrite
static inline void gpio_set_af(uint16_t pin, uint8_t af_num) {
    struct Gpio *gpio = GPIO(PINBANK(pin));
    int n = PINNO(pin);
    gpio->AFR[n >> 3] &= ~(15UL << ((n & 7) * 4)); // clearing AFR bits (?)
    gpio->AFR[n >> 3] |= ((uint32_t) af_num) << ((n & 7) * 4); // setting AFR bits
}

static inline void usart_init(struct Usart *usart, unsigned long usart_div) {
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

static inline int usart_read_ready(struct Usart *usart) {
    return usart->SR & BIT(5); // if RXNE bit is set, data is ready to read
}

static inline uint8_t usart_read_byte(struct Usart *usart) {
    return (uint8_t) (usart->DR & 255); // bottom 8 bits of DR register hold received value
}

static inline void usart_write_byte(struct Usart *usart, uint8_t data) {
    while (!(usart->SR & USART_TRANSMISSION_COMPLETE));
    usart->DR = data;
}

static inline void usart_write_buffer(struct Usart *usart, char *buffer, size_t len) {
    while (len-- > 0) usart_write_byte(usart, *(uint8_t *) buffer++);
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

void blinkies(void) {
    uint16_t blue = PIN('B', 7);
    uint16_t green = PIN('B', 0);
    uint16_t red = PIN('B', 14);

    gpio_set_mode(blue, GPIO_MODE_OUTPUT);

    gpio_set_mode(green, GPIO_MODE_OUTPUT);

    gpio_set_mode(red, GPIO_MODE_OUTPUT);

    uint32_t red_timer = 0;
    uint32_t period_250_ms = 500;

    uint32_t blue_timer = 0;
    uint32_t period_500_ms = 600;

    uint32_t green_timer = 0;
    uint32_t period_750_ms = 700;

    for (;;) {
        if (timer_expired(&red_timer, period_250_ms, s_ticks)) {
            static bool on;
            gpio_write(red, on);
            on = !on;
        }

        if (timer_expired(&blue_timer, period_500_ms, s_ticks)) {
            static bool on;
            gpio_write(blue, on);
            on = !on;
        }

        if (timer_expired(&green_timer, period_750_ms, s_ticks)) {
            static bool on;
            gpio_write(green, on);
            on = !on;
        }
    }
}

int main(void) {
    systick_init(16000000 / 1000);

    // usart_init(USART3, 0x008B); // divider for 115200 baud rate
    usart_init(USART3, 0x0683); // divider for 9600 baud rate

    usart_write_buffer(USART3, "Hello World\n", 12);

    uint32_t usart_timer = 0;
    uint32_t period = 1000;

    for (;;) {
        if (timer_expired(&usart_timer, period, s_ticks)) {
            usart_write_buffer(USART3, "abc\n", 4);
        }
    }

    // blinkies();

    return 0;
}

__attribute__((naked, noreturn)) void _reset(void) {
    extern long _sbss, _ebss, _sdata, _edata, _sidata;
    for (long *dst = &_sbss; dst < &_ebss; dst++) *dst = 0;
    for (long *dst = &_sdata, *src = &_sidata; dst < &_edata;) *dst++ = *src++;

    main();
    for (;;) (void) 0;
}

extern void _estack(void);

__attribute__((section(".vectors"))) void (*const tab[16 + 91])(void) = {
    _estack, _reset, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, SysTick_Handler
};

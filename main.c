#include <stdint.h>
#include <stdbool.h>

#define BIT(x) (1UL << (x))
#define PIN(bank, num) ((((bank) - 'A') << 8) | (num))
#define PINNO(pin) (pin & 255)
#define PINBANK(pin) (pin >> 8)

struct gpio {
    volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR, LCKR, AFR[2];
};
#define GPIO(bank) ((struct gpio *) (0x40020000 + 0x400 * (bank)))

typedef enum { GPIO_MODE_INPUT, GPIO_MODE_OUTPUT, GPIO_MODE_AF, GPIO_MODE_ANALOG } GpioMode ;

static inline void gpio_set_mode(uint16_t pin, GpioMode mode) { // TODO: use enum type?
    struct gpio *gpio = GPIO(PINBANK(pin));
    int n = PINNO(pin);
    gpio->MODER &= ~(3U << (n * 2));
    gpio->MODER |= (mode & 3U) << (n * 2);
}

static inline void gpio_write(uint16_t pin, bool val) {
    struct gpio *gpio = GPIO(PINBANK(pin));
    gpio->BSRR = (1U << PINNO(pin)) << (val ? 0 : 16);
}

static inline void spin(volatile uint32_t count) {
    while (count--) (void) 0;
}

struct rcc {
    volatile uint32_t CR, PLLCFGR, CFGR, CIR, AHB1RSTR, AHB2RSTR, AHB3RSTR,
        RESERVED0, APB1RSTR, APB2RSTR, RESERVED1[2], AHB1ENR, AHB2ENR, AHB3ENR,
        RESERVED2, APB1ENR, APB2ENR, RESERVED3[2], AHB1LPENR, AHB2LPENR,
        AHB3LPENR, RESERVED4, APB1LPENR, APB2LPENR, RESERVED5[2], BDCR, CSR,
        RESERVED6[2], SSCGR, PLLI2SCFGR;
};
#define RCC ((struct rcc *) 0x40023800)

int main(void) {
    uint16_t blue = PIN('B', 7);
    uint16_t green = PIN('B', 0);
    uint16_t red = PIN('B', 14);

    RCC->AHB1ENR |= BIT(PINBANK(blue));
    gpio_set_mode(blue, GPIO_MODE_OUTPUT);

    RCC->AHB1ENR |= BIT(PINBANK(green));
    gpio_set_mode(green, GPIO_MODE_OUTPUT);

    RCC->AHB1ENR |= BIT(PINBANK(red));
    gpio_set_mode(red, GPIO_MODE_OUTPUT);

    for (;;) {
        gpio_write(red, true);
        spin(799999);
        gpio_write(blue, true);
        spin(799999);
        gpio_write(green, true);
        spin(799999);
        gpio_write(red, false);
        spin(799999);
        gpio_write(blue, false);
        spin(799999);
        gpio_write(green, false);
        spin(799999);
    };
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
    _estack, _reset
};

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

typedef enum { GPIO_MODE_INPUT, GPIO_MODE_OUTPUT, GPIO_MODE_AF, GPIO_MODE_ANALOG } GpioMode ;

static inline void gpio_set_mode(uint16_t pin, GpioMode mode) {
    struct Gpio *gpio = GPIO(PINBANK(pin));
    int n = PINNO(pin);
    gpio->MODER &= ~(3U << (n * 2));
    gpio->MODER |= (mode & 3U) << (n * 2);
}

static inline void gpio_write(uint16_t pin, bool val) {
    struct Gpio *gpio = GPIO(PINBANK(pin));
    gpio->BSRR = (1U << PINNO(pin)) << (val ? 0 : 16);
}

static inline void spin(volatile uint32_t count) {
    while (count--) (void) 0;
}

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

    systick_init(16000000 / 1000);

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

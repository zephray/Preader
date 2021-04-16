#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "power.h"
#include "lcddata.pio.h"

#define LCD_COMLP 11
#define LCD_FLM 12
#define LCD_M 13
#define LCD_SEGLP 14
#define LCD_DISPON 15
#define LCD_CP 16
#define LCD_D0 17

// Clear
// V0: 23.0V
// V1: 21.9V
// V2: 21.5V
// V3: 1.36V
// V4: 0.68V

// Refresh
// V0: 28.2V
// V1: 22.0V
// V2: 15.7V
// V3: 12.45V
// V4: 6.25V

#include "bg.h"

uint8_t framebuffer[480 * 52];

#define LCD_DATA_SM (0)
PIO lcd_pio = pio0;

#define LCD_TARGET_PIXCLK (1000000)

void delay_arb(int t) {
    volatile int x = t;
    while (x--);
}

void delay(void) {
    delay_arb(10);
}

void delay_long(void) {
    for (int x = 0; x < 1000; x++) {
        delay_arb(1000);
    }
}

static inline void lcdsm_put(uint32_t dat) {
    while (pio_sm_is_tx_fifo_full(lcd_pio, LCD_DATA_SM));
    *(volatile uint32_t *)&lcd_pio->txf[LCD_DATA_SM] = dat;
}

static inline void lcdsm_wait(void) {
    uint32_t sm_stall_mask = 1u << (LCD_DATA_SM + PIO_FDEBUG_TXSTALL_LSB);
    lcd_pio->fdebug = sm_stall_mask;
    while (!(lcd_pio->fdebug & sm_stall_mask));
}

void lcd_set_bias(float vh, int bias) {
    float v4 = vh / bias;
    float v3 = v4 * 2;
    float v1 = vh - v4;
    float v2 = vh - v3;
    power_set_lcd_vh(vh);
    power_set_lcd_bias(v1, v2, v3, v4);
    sleep_ms(100);
}

void lcd_clear_screen(void) {
    power_set_lcd_vh(23.0f);
    power_set_lcd_bias(21.9f, 21.5f, 2.0f, 1.41f);
    // Wait for voltage to stable
    sleep_ms(100);

    gpio_put(LCD_DISPON, 1);

    pio_sm_set_enabled(lcd_pio, LCD_DATA_SM, true);
    for (int i = 0; i < 13; i++) { // will send 416 pixels, doesn't matter
       // loop filling FIFO
        lcdsm_put(0xffffffff);
    }
    lcdsm_wait();
    pio_sm_set_enabled(lcd_pio, LCD_DATA_SM, false);

    gpio_put(LCD_SEGLP, 1);
    delay();
    gpio_put(LCD_SEGLP, 0);
    delay();
    
    gpio_put(LCD_M, 1);
    gpio_put(LCD_FLM, 1);
    for (int i = 0; i < 484; i++) {
        delay_arb(30000);
        gpio_put(LCD_COMLP, 1);
        delay();
        gpio_put(LCD_COMLP, 0);
        delay();
        gpio_put(LCD_FLM, 0);
    }

    gpio_put(LCD_DISPON, 0);
    delay_long();
}

void lcd_refresh(int c) {

    lcd_set_bias(18, 4);
   /* power_set_lcd_vh(29.0f);
    power_set_lcd_bias(21.9f, 21.5f, 2.0f, 1.41f);*/
    sleep_ms(100);

    gpio_put(LCD_FLM, 1);
    gpio_put(LCD_M, 0);
    gpio_put(LCD_COMLP, 0);
    delay();
    gpio_put(LCD_DISPON, 1);

    uint32_t *rdptr = framebuffer;

    for (int i = 0; i < 480; i++) {
        pio_sm_set_enabled(lcd_pio, LCD_DATA_SM, true);
        for (int j = 0; j < 13; j++) { // will send 416 pixels, doesn't matter
            lcdsm_put(*rdptr++);
        }
        lcdsm_wait();
        pio_sm_set_enabled(lcd_pio, LCD_DATA_SM, false);

        if (i == 0) {
            gpio_put(LCD_SEGLP, 1);
            delay();
            gpio_put(LCD_SEGLP, 0);
        }
        else {
            gpio_put(LCD_COMLP, 1);
            gpio_put(LCD_SEGLP, 1);
            delay();
            gpio_put(LCD_COMLP, 0);
            gpio_put(LCD_SEGLP, 0);
            gpio_put(LCD_FLM, 0);
        }
        sleep_us(2000);
        //delay_arb(100000);
    }
    gpio_put(LCD_DISPON, 0);
}

void lcd_refresh_fast_prepare(void) {
    lcd_set_bias(32.0, 24);
    gpio_put(LCD_FLM, 1);
    gpio_put(LCD_M, 0);
    gpio_put(LCD_COMLP, 0);
    delay();
    pio_sm_set_enabled(lcd_pio, LCD_DATA_SM, true);
}

void lcd_refresh_fast(void) {
    uint32_t *rdptr = framebuffer;
    static int toggle = 0;

    gpio_put(LCD_DISPON, 1);
    gpio_put(LCD_FLM, 1);
    for (int i = 0; i < 481; i++) {
        for (int j = 0; j < 13; j++) { // will send 416 pixels, doesn't matter
            lcdsm_put(*rdptr++);
        }
        lcdsm_wait();

        if (i == 0) {
            gpio_put(LCD_SEGLP, 1);
            delay();
            gpio_put(LCD_SEGLP, 0);
        }
        else {
            gpio_put(LCD_COMLP, 1);
            gpio_put(LCD_SEGLP, 1);
            delay();
            gpio_put(LCD_COMLP, 0);
            gpio_put(LCD_SEGLP, 0);
            gpio_put(LCD_FLM, 0);
        }
        
        gpio_put(LCD_M, toggle);
        toggle = !toggle;
    }

    gpio_put(LCD_DISPON, 0);
    
}

static void lcdsm_init() {
    static uint udata_offset;

    for (int i = 0; i < 8; i++) {
        pio_gpio_init(lcd_pio, LCD_D0 + i);
    }
    pio_gpio_init(lcd_pio, LCD_CP);
    pio_sm_set_consecutive_pindirs(lcd_pio, LCD_DATA_SM, LCD_D0, 8, true);
    pio_sm_set_consecutive_pindirs(lcd_pio, LCD_DATA_SM, LCD_CP, 1, true);

    udata_offset = pio_add_program(lcd_pio, &lcd_data_program);

    int cycles_per_pclk = 2;
    float div = clock_get_hz(clk_sys) / (LCD_TARGET_PIXCLK * cycles_per_pclk);

    pio_sm_config cu = lcd_data_program_get_default_config(udata_offset);
    sm_config_set_sideset_pins(&cu, LCD_CP);
    sm_config_set_out_pins(&cu, LCD_D0, 8);
    sm_config_set_fifo_join(&cu, PIO_FIFO_JOIN_TX);
    sm_config_set_out_shift(&cu, true, true, 32);
    sm_config_set_clkdiv(&cu, div);
    pio_sm_init(lcd_pio, LCD_DATA_SM, udata_offset, &cu);
}

static void lcd_gpio_pin_init(int pin) {
    gpio_init(pin);
    gpio_put(pin, 0);
    gpio_set_dir(pin, GPIO_OUT);
}

static void lcd_pad_set_property(int pin) {
    uint32_t *padctrl = (uint32_t *)(PADS_BANK0_BASE + 4 + pin * 4);
    *padctrl &= 0x31;
    *padctrl |= 0x31;
    //*padctrl |= (slew & 0x1);
}

void lcd_set_pixel(int x, int y, int c) {
    int ax = 479 - x;
    int ay = y + 48;
    uint8_t *ptr = &(framebuffer[ax * 52 + ay / 8]);
    if (c)
        *ptr |= (1 << (ay % 8));
    else
        *ptr &= (1 << (ay % 8));
}

void lcd_xor_pixel(int x, int y) {
    int ax = 479 - x;
    int ay = y + 48;
    uint8_t *ptr = &(framebuffer[ax * 52 + ay / 8]);
    *ptr ^= (1 << (ay % 8));
}

void lcd_init(void) {
    lcd_gpio_pin_init(LCD_COMLP);
    lcd_gpio_pin_init(LCD_FLM);
    lcd_gpio_pin_init(LCD_M);
    lcd_gpio_pin_init(LCD_SEGLP);
    lcd_gpio_pin_init(LCD_DISPON);
    lcdsm_init();

    // Set slew rate to fast
    lcd_pad_set_property(LCD_COMLP);
    lcd_pad_set_property(LCD_FLM);
    lcd_pad_set_property(LCD_M);
    lcd_pad_set_property(LCD_SEGLP);
    lcd_pad_set_property(LCD_DISPON);
    lcd_pad_set_property(LCD_CP);
    for (int i = 0; i < 8; i++) {
        lcd_pad_set_property(LCD_D0 + i);
    }

    /*memset(framebuffer, 0x00, 480 * 52);
    for (int i = 0; i < 480; i++) {
        lcd_set_pixel(i, 0, 1);
        lcd_set_pixel(i, 360 - 1, 1);
    }
    //lcd_set_pixel(100, 100, 1);
    for (int i = 0; i < 360; i++) {
        lcd_set_pixel(0, i, 1);
        lcd_set_pixel(480 - 1, i, 1);
    }
    for (int i = 100; i < 200; i++) {
        for (int j = 100; j < 200; j++) {
            lcd_set_pixel(i, j, 1);
        }
    }*/
    memcpy(framebuffer, gImage_bg, 480 * 52);
}

void lcd_test() {
    lcd_refresh_fast_prepare();
    int x = 0, y = 0;
    int dir = 0;
    for (;;) {
        //memset(framebuffer, 0, 480 * 52);
        memcpy(framebuffer, gImage_bg, 480 * 52);
        bool xright = (x == 480 - 100);
        bool ydown = (y == 360 - 100);
        bool yup = (y == 0);
        bool xleft = (x == 0);
        if (dir == 0) {
            // Right down
            if (ydown) dir = 1; // right up
            if (xright) dir = 2; // left down
            if (ydown && xright) dir = 3;
        }
        else if (dir == 1) {
            // Right up
            if (yup) dir = 0; // right down
            if (xright) dir = 3; // left up
            if (yup && xright) dir = 2;
        }
        else if (dir == 2) {
            // Left down
            if (ydown) dir = 3; // left up
            if (xleft) dir = 0; // right down
            if (ydown && xleft) dir = 1;
        }
        else if (dir == 3) {
            // Left up
            if (yup) dir = 2; // left down
            if (xleft) dir = 1; // right up
            if (yup && xleft) dir = 0;
        }

        if (dir == 0) {
            // Right down
            x++; y++;
        }
        else if (dir == 1) {
            // Right up
            x++; y--;
        }
        else if (dir == 2) {
            // Left down
            x--; y++;
        }
        else if (dir == 3) {
            // Left up
            x--; y--;
        }

        for (int i = x; i < x + 100; i++) {
            for (int j = y; j < y + 100; j++) {
                lcd_xor_pixel(i, j);
            }
        }        

        lcd_refresh_fast();
    }
}
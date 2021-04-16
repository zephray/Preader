#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "power.h"
#include "lcd.h"

#define LED_PIN 6

int main()
{
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_put(LED_PIN, 0);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    //sleep_ms(2000);

    puts("\r\npREADER Firmware\r\n");

    power_init();

    gpio_put(LED_PIN, 1);
    sleep_ms(500);
    gpio_put(LED_PIN, 0);
    sleep_ms(500);
    
    power_enable_lcd();
    sleep_ms(100);
    lcd_init();
    lcd_clear_screen();

    gpio_put(LED_PIN, 1);
    sleep_ms(500);
    gpio_put(LED_PIN, 0);
    sleep_ms(500);
    
    lcd_refresh(1);
    //lcd_test();

    power_disable_lcd();

    while(1) {
        gpio_put(LED_PIN, 1);
        sleep_ms(500);
        gpio_put(LED_PIN, 0);
        sleep_ms(500);
    }

    return 0;
}

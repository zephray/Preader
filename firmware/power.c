#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"

#define ADP5360_ADDR (0x46)
#define DS4424_ADDR (0x10)

#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1

#define VH_PWM_PIN 3

#define SD_LCD_PWR_EN 4

void i2c_board_init(void) {
    i2c_init(I2C_PORT, 400*1000);
    
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);
}

void i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t dat) {
    uint8_t buf[2] = {reg, dat};
    int result = i2c_write_blocking(I2C_PORT, addr, buf, 2, false);
    if (result != 2) {
        puts("Failed writing data to i2c");
    }
}

void i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *dat) {
    int result;
    result = i2c_write_blocking(I2C_PORT, addr, &reg, 1, true);
    if (result != 1) {
        puts("Failed writing data to i2c");
    }
    result = i2c_read_blocking(i2c_default, addr, dat, 1, false);
    if (result != 1) {
        puts("Failed reading data from i2c");
    }
}

void power_set_usb_current_limit(void) {
    // Adaptive current limit threshold = 4.6V, VBUS current limit = 500mA
    i2c_write_reg(ADP5360_ADDR, 0x02, 0x87);
}

void pwm_board_init(void) {
    gpio_set_function(VH_PWM_PIN, GPIO_FUNC_PWM);
    uint vh_pwm_slice = pwm_gpio_to_slice_num(VH_PWM_PIN);
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 4.f);
    pwm_config_set_wrap(&config, 255);
    pwm_init(vh_pwm_slice, &config, false);
    pwm_set_gpio_level(VH_PWM_PIN, 127);
    pwm_set_enabled(vh_pwm_slice, true);
}

void power_set_lcd_vh(float vh_volt) {
    float pwm = ((vh_volt - 8.2f) / 24.3f) * 255.f;
    int pwm_int;
    if (pwm < 0) pwm_int = 0;
    else if (pwm > 255) pwm_int = 127;
    else pwm_int = (int)pwm;
    pwm_set_gpio_level(VH_PWM_PIN, 255 - pwm_int);
}

static uint8_t lcd_bias_volt_to_curr(float volt) {
    // V = 0.129 X + 16.71
    float setp = (volt - 16.71f) / 0.129f;
    int setp_int;
    if (setp < 0) {
        if (setp < -127)
            setp_int = 0xff;
        else
            setp_int = 0x80 | ((int)(-setp));
    }
    else {
        if (setp > 127)
            setp_int = 0x7f;
        else
            setp_int = (int)(setp);
    }
    return setp_int;
}

void power_set_lcd_bias(float v1, float v2, float v3, float v4) {
    // 0x7f = ?
    // 0x5f = 29.47
    // 0x00 = 16.28
    // 0xcf = 5.58
    // 0xff = 1.41
    i2c_write_reg(DS4424_ADDR, 0xf8, lcd_bias_volt_to_curr(v1));
    i2c_write_reg(DS4424_ADDR, 0xf9, lcd_bias_volt_to_curr(v2));
    i2c_write_reg(DS4424_ADDR, 0xfa, lcd_bias_volt_to_curr(v3));
    i2c_write_reg(DS4424_ADDR, 0xfb, lcd_bias_volt_to_curr(v4));
}

void power_reset_lcd_bias(void) {
    i2c_write_reg(DS4424_ADDR, 0xf8, 0);
    i2c_write_reg(DS4424_ADDR, 0xf9, 0);
    i2c_write_reg(DS4424_ADDR, 0xfa, 0);
    i2c_write_reg(DS4424_ADDR, 0xfb, 0);
}

void power_enable_lcd(void) {
    gpio_put(SD_LCD_PWR_EN, 1);
}

void power_disable_lcd(void) {
    gpio_put(SD_LCD_PWR_EN, 0);
    power_reset_lcd_bias();
}

void power_init(void) {
    uint8_t id;
    
    // IO
    i2c_board_init();
    gpio_init(SD_LCD_PWR_EN);
    gpio_put(SD_LCD_PWR_EN, 0);
    gpio_set_dir(SD_LCD_PWR_EN, GPIO_OUT);

    // Setup PMIC
    i2c_read_reg(ADP5360_ADDR, 0x00, &id);
    printf("ADP5360 ID: %02x\n", id);
    power_set_usb_current_limit();

    // Start PWM
    pwm_board_init();

    power_set_lcd_vh(32.f);

    power_set_lcd_bias(2.0f, 4.0f, 8.0f, 15.0f);

    printf("Power up.\n");
}

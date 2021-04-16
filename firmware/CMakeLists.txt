# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initalize pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "/home/wenting/pico/pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(firmware C CXX ASM)

set(PICO_BOARD_HEADER_DIRS ${CMAKE_SOURCE_DIR}/board)
set(PICO_BOARD "preader")

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(firmware
        firmware.c
        lcd.c
        power.c
        )

pico_generate_pio_header(firmware ${CMAKE_CURRENT_LIST_DIR}/lcddata.pio)

pico_set_program_name(firmware "firmware")
pico_set_program_version(firmware "0.1")

pico_enable_stdio_uart(firmware 0)
pico_enable_stdio_usb(firmware 1)

# Add the standard library to the build
target_link_libraries(firmware pico_stdlib)

# Add any user requested libraries
target_link_libraries(firmware
        hardware_i2c
        hardware_dma
        hardware_pio
        hardware_pwm
        )

pico_add_extra_outputs(firmware)


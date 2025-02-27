cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if(EXISTS ${picoVscode})
    include(${picoVscode})
endif()

set(PICO_BOARD pico_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)
project(Conversores_AD C CXX ASM)
pico_sdk_init()

add_executable(Conversores_AD 
    Conversores_AD.c 
    inc/ssd1306.c)

pico_set_program_name(Conversores_AD "Conversores_AD")
pico_set_program_version(Conversores_AD "0.1")

pico_enable_stdio_uart(Conversores_AD 1)
pico_enable_stdio_usb(Conversores_AD 1)

target_link_libraries(Conversores_AD
    pico_stdlib
    hardware_adc
    hardware_pwm
    hardware_i2c)

target_include_directories(Conversores_AD PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(Conversores_AD)

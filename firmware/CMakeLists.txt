cmake_minimum_required(VERSION 3.16)

# Target board for YAHAL
set(YAHAL_BOARD "rp2350-launchpad")

# Locate the YAHAL library that ships with the course material.
# Override by exporting YAHAL_DIR in your environment if your layout differs.
if (NOT YAHAL_DIR AND NOT DEFINED ENV{YAHAL_DIR})
    set(YAHAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../yahal_with_examples/YAHAL)
endif ()

include(YAHAL_import.cmake)

project(edge_ai_firmware CXX C ASM)

add_executable(edge_ai_firmware
    src/main.cpp
    src/face.cpp
    src/leds.cpp
    src/menu.cpp
)

# Pull in YAHAL and the libraries/drivers this firmware needs:
#   uGUI + lcd  -> animated face on the ST7735S display
#   ws2812      -> reactive on-board RGB LEDs (PIO)
# The host link uses the back-channel UART (uart_rp2350, part of YAHAL core),
# bridged to the laptop by the on-board debug probe; no USB stack needed.
yahal_add_me(edge_ai_firmware)
yahal_add_library(edge_ai_firmware uGUI)
yahal_add_driver(edge_ai_firmware lcd)
yahal_add_driver(edge_ai_firmware rp2350/ws2812)
yahal_add_driver(edge_ai_firmware rp2350/pcm_pwm)   # buzzer audio playback

# Build helpers: flashing targets and .uf2 / .hex / .bin outputs
yahal_add_custom_targets(edge_ai_firmware)
yahal_add_extra_outputs(edge_ai_firmware)

# The animated LCD face also has a stand-alone demo project (no USB needed):
# see firmware/face_demo/.

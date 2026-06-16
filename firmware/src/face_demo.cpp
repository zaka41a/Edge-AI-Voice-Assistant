// ---------------------------------------------
//  Edge AI Voice Assistant - animated face DEMO
//  Author: Zakaria Sabiri
//
//  Stand-alone demo so the LCD face can be flashed
//  and seen WITHOUT the host bridge or USB. It walks
//  the assistant through a full interaction loop:
//
//      READY -> LISTENING -> THINKING -> SPEAKING
//
//  cycling through every mood while "speaking", so
//  the whole repertoire is visible at a glance.
//
//  Once the framed USB protocol is in place, the real
//  firmware will call exactly the same Face API, but
//  driven by MOOD/TEXT frames from the laptop instead
//  of this scripted timeline.
//
//  LCD bring-up mirrors the YAHAL example
//  examples/rp2350-launchpad/boostxl_edumkII/uGUI_simple.
// ---------------------------------------------
#include "boostxl_eduMKII.h"

#include "gpio_rp2350.h"
#include "spi_rp2350.h"
#include "st7735s_drv.h"

#include "uGUI.h"
#include "task.h"

#include "face.h"

int main(void)
{
    // --- LCD bring-up -----------------------------------------------------
    gpio_rp2350 lcd_bl(EDU_LCD_BL);
    lcd_bl.gpioMode(GPIO::OUTPUT | GPIO::INIT_HIGH);     // backlight on

    gpio_rp2350 lcd_cs(EDU_LCD_CS);
    spi_rp2350  spi(EDU_LCD_MISO, EDU_LCD_MOSI, EDU_LCD_SCLK, lcd_cs);
    spi.setSpeed(24000000);

    gpio_rp2350 lcd_rst(EDU_LCD_RST);
    gpio_rp2350 lcd_dc(EDU_LCD_DC);
    st7735s_drv lcd(spi, lcd_rst, lcd_dc, st7735s_drv::Crystalfontz_128x128);

    uGUI gui(lcd);

    // --- the face ---------------------------------------------------------
    Face face(gui);
    face.begin();

    constexpr uint32_t FRAME_MS = 80;       // ~12 fps animation

    // A scripted conversation timeline. Each step lasts `frames` ticks.
    struct Step { Face::State state; Face::Mood mood; int frames; };
    static const Step script[] = {
        { Face::IDLE,      Face::NEUTRAL,  20 },   // waiting
        { Face::LISTENING, Face::NEUTRAL,  22 },   // hears the user
        { Face::THINKING,  Face::NEUTRAL,  22 },   // querying the LLM
        { Face::SPEAKING,  Face::HAPPY,    24 },   // cheerful reply
        { Face::IDLE,      Face::HAPPY,     8 },
        { Face::LISTENING, Face::NEUTRAL,  18 },
        { Face::THINKING,  Face::NEUTRAL,  20 },
        { Face::SPEAKING,  Face::CURIOUS,  24 },   // curious reply
        { Face::IDLE,      Face::CURIOUS,   8 },
        { Face::THINKING,  Face::NEUTRAL,  18 },
        { Face::SPEAKING,  Face::SAD,      24 },    // sad reply
        { Face::IDLE,      Face::SAD,       8 },
        { Face::THINKING,  Face::NEUTRAL,  18 },
        { Face::SPEAKING,  Face::ANGRY,    24 },    // annoyed reply
        { Face::IDLE,      Face::NEUTRAL,  10 },
    };
    constexpr int N = sizeof(script) / sizeof(script[0]);

    int step = 0, left = 0;
    while (true) {
        if (left == 0) {                    // advance to the next scene
            face.setState(script[step].state);
            face.setMood (script[step].mood);
            left = script[step].frames;
            step = (step + 1) % N;
        }
        face.tick();
        --left;
        task::sleep_ms(FRAME_MS);
    }
}

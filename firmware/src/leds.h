// ---------------------------------------------
//  Edge AI Voice Assistant - reactive WS2812 LEDs
//  Module: Mikrocontrollertechnik (FH Aachen)
//  Author: Zakaria Sabiri
//
//  Drives the 8 on-board WS2812 RGB LEDs (GPIO 39)
//  so they tell the assistant's state at a glance:
//    IDLE      -> gentle breathing in the mood color
//    LISTENING -> a blue dot scanning back and forth
//    THINKING  -> an orange spinner
//    SPEAKING  -> the whole strip pulsing in mood color
//
//  Pure PIO timing under the hood (ws2812_rp2350). The
//  same mood/state vocabulary as the LCD Face, so the
//  two react together.
// ---------------------------------------------
#ifndef EDGE_AI_LEDS_H
#define EDGE_AI_LEDS_H

#include <cstdint>
#include "ws2812_rp2350.h"
#include "board.h"

class MoodLeds {
public:
    enum Mood  { NEUTRAL, HAPPY, SAD, ANGRY, CURIOUS };
    enum State { IDLE, LISTENING, THINKING, SPEAKING };

    MoodLeds();

    void setMood(Mood m)   { _mood  = m; _solid = false; }
    void setState(State s) { _state = s; _solid = false; }
    void setMood(uint8_t moodByte);
    void setState(uint8_t stateByte);

    // Force a fixed color on the whole strip (function-calling: "set LEDs red").
    // Stays until the next setMood()/setState().
    void setSolid(uint32_t rgb);

    // Advance one animation frame. Call about every 50 ms.
    void tick();

private:
    ws2812_rp2350 _leds;
    Mood     _mood  = NEUTRAL;
    State    _state = IDLE;
    bool     _solid = false;
    uint32_t _solidRgb = 0;
    uint32_t _frame = 0;

    uint32_t _moodColor() const;
    void     _fill(uint32_t rgb);
    void     _flush(uint32_t (&buf)[LED_RGB_COUNT]);
};

#endif  // EDGE_AI_LEDS_H

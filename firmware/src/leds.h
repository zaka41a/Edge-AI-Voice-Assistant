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

    void setSolid(uint32_t rgb);

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

#endif

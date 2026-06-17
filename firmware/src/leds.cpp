#include "leds.h"

namespace {
    constexpr int N = LED_RGB_COUNT;

    uint32_t scale(uint32_t rgb, uint8_t b) {
        uint32_t r = ((rgb >> 16) & 0xFF) * b / 255;
        uint32_t g = ((rgb >>  8) & 0xFF) * b / 255;
        uint32_t bl = (rgb & 0xFF) * b / 255;
        return (r << 16) | (g << 8) | bl;
    }

    uint8_t triangle(uint32_t frame, uint32_t period) {
        uint32_t p = frame % period;
        uint32_t half = period / 2;
        uint32_t up = (p < half) ? p : (period - p);
        return static_cast<uint8_t>(up * 255 / half);
    }
}

MoodLeds::MoodLeds() : _leds(LED_RGB_GPIO, LED_RGB_COUNT) {
    _fill(0);
}

uint32_t MoodLeds::_moodColor() const {
    switch (_mood) {
        case HAPPY:   return 0x00FF00;
        case SAD:     return 0x1E90FF;
        case ANGRY:   return 0xFF0000;
        case CURIOUS: return 0xFFD700;
        case NEUTRAL:
        default:      return 0x00FFFF;
    }
}

static inline uint8_t gamma8(uint8_t v) {
    return static_cast<uint8_t>((static_cast<uint16_t>(v) * v) / 255);
}
static inline uint32_t gamma_rgb(uint32_t c) {
    return (static_cast<uint32_t>(gamma8((c >> 16) & 0xFF)) << 16) |
           (static_cast<uint32_t>(gamma8((c >>  8) & 0xFF)) <<  8) |
            static_cast<uint32_t>(gamma8(c & 0xFF));
}

void MoodLeds::_flush(uint32_t (&buf)[N]) {
    for (int i = 0; i < N; ++i) buf[i] = gamma_rgb(buf[i]);
    _leds.set_colors(buf, N);
}

void MoodLeds::_fill(uint32_t rgb) {
    uint32_t buf[N];
    for (int i = 0; i < N; ++i) buf[i] = rgb;
    _flush(buf);
}

void MoodLeds::setMood(uint8_t moodByte) {
    setMood(moodByte <= CURIOUS ? static_cast<Mood>(moodByte) : NEUTRAL);
}

void MoodLeds::setState(uint8_t stateByte) {
    setState(stateByte <= SPEAKING ? static_cast<State>(stateByte) : IDLE);
}

void MoodLeds::setSolid(uint32_t rgb) {
    _solid = true;
    _solidRgb = rgb;
    _fill(rgb);
}

void MoodLeds::tick() {
    if (_solid) { _fill(_solidRgb); ++_frame; return; }

    const uint32_t col = _moodColor();
    uint32_t buf[N];

    switch (_state) {
        case IDLE: {
            uint8_t b = 20 + triangle(_frame, 60) * 60 / 255;
            for (int i = 0; i < N; ++i) buf[i] = scale(col, b);
            break;
        }
        case LISTENING: {
            int pos = triangle(_frame, (N - 1) * 2) * (N - 1) / 255;
            for (int i = 0; i < N; ++i) {
                int d = (i > pos) ? (i - pos) : (pos - i);
                uint8_t b = (d == 0) ? 255 : (d == 1) ? 60 : 8;
                buf[i] = scale(0x1E90FF, b);
            }
            break;
        }
        case THINKING: {
            int head = (_frame / 2) % N;
            for (int i = 0; i < N; ++i) {
                int d = (head - i + N) % N;
                uint8_t b = (d == 0) ? 255 : (d == 1) ? 90 : (d == 2) ? 25 : 4;
                buf[i] = scale(0xFF6000, b);
            }
            break;
        }
        case SPEAKING: {
            uint8_t b = 60 + triangle(_frame, 12) * 195 / 255;
            for (int i = 0; i < N; ++i) buf[i] = scale(col, b);
            break;
        }
    }
    _flush(buf);
    ++_frame;
}

#include <cstring>

#include "board.h"
#include "boostxl_eduMKII.h"
#include "gpio_rp2350.h"
#include "spi_rp2350.h"
#include "st7735s_drv.h"
#include "task.h"
#include "uart_rp2350.h"
#include "adc_rp2350.h"
#include "pcm_pwm_rp2350_drv.h"

#include "uGUI.h"

#include "protocol.h"
#include "face.h"
#include "leds.h"
#include "menu.h"

namespace {
    constexpr int      SAMPLE_RATE   = 16000;
    constexpr int      MAX_SAMPLES   = SAMPLE_RATE * 6;
    constexpr int      MIN_SAMPLES   = SAMPLE_RATE / 3;
    constexpr int      CHUNK8        = 480;

    constexpr int      PLAY_RATE     = 16000;
    constexpr int      PLAY_CAP      = 16000 * 9;

    int16_t g_audio[MAX_SAMPLES];
    uint8_t g_pay[CHUNK8];
    uint8_t g_tx[proto::OVERHEAD + CHUNK8];
    uint8_t g_play[PLAY_CAP];
}

static void play_audio(pcm_pwm_rp2350_drv &pcm, const uint8_t *data, int n) {
    pcm.timer_reset();
    for (int i = 0; i < n; ++i) {
        int16_t s = static_cast<int16_t>(static_cast<int8_t>(data[i])) << 8;
        pcm_value_t v{ s, s };
        while (pcm.pcmFifoAvailablePut() < 1) { }
        pcm.pcmFifoPut(v);
    }
    task::sleep_ms(60);
}

static void uart_send(uart_rp2350 &u, const uint8_t *data, size_t n) {
    for (size_t i = 0; i < n; ++i) u.putc(static_cast<char>(data[i]));
}

static void capture_and_stream(uart_rp2350 &uart, adc_rp2350_channel &mic,
                               Face &face, MoodLeds &leds,
                               gpio_rp2350 *hold, int fixed_ms) {
    face.setState(Face::LISTENING);
    leds.setState(MoodLeds::LISTENING);
    face.tick();
    leds.tick();

    const int limit = hold ? MAX_SAMPLES : (SAMPLE_RATE * fixed_ms / 1000);
    const uint64_t start = task::micros();
    int n = 0;
    int release = 0;
    while (n < limit) {
        if (hold) {
            if (hold->gpioRead() != LOW) {

                if (++release > 3200 && n >= MIN_SAMPLES) break;
            } else {
                release = 0;
            }
        }
        uint16_t v = mic.adcReadRaw();
        g_audio[n] = static_cast<int16_t>((static_cast<int>(v) - 2048) << 4);
        ++n;
        uint64_t next = start + static_cast<uint64_t>(n) * 1000000ULL / SAMPLE_RATE;
        while (task::micros() < next) { }
    }

    face.setState(Face::THINKING);
    leds.setState(MoodLeds::THINKING);
    face.tick();
    leds.tick();

    uint8_t seq = 0;
    for (int off = 0; off < n; off += CHUNK8) {
        int c = (n - off < CHUNK8) ? (n - off) : CHUNK8;
        for (int k = 0; k < c; ++k) {
            int amp = static_cast<int>(g_audio[off + k]) * 6;
            if (amp > 32767) amp = 32767;
            else if (amp < -32768) amp = -32768;
            g_pay[k] = static_cast<uint8_t>(amp >> 8);
        }
        size_t len = proto::encode(proto::AUDIO_IN, seq++, g_pay,
                                   static_cast<uint16_t>(c), g_tx);
        uart_send(uart, g_tx, len);
    }
    size_t len = proto::encode(proto::AUDIO_IN, seq++, nullptr, 0, g_tx);
    uart_send(uart, g_tx, len);
}

static void handle_frame(const proto::Frame &f, Face &face, MoodLeds &leds) {
    switch (f.type) {
    case proto::MOOD:
        if (f.len >= 1) {
            face.setMood(static_cast<Face::Mood>(f.payload[0] % 5));
            leds.setMood(f.payload[0]);
        }
        break;
    case proto::CMD:
        if (f.len >= 2 && f.payload[0] == proto::CMD_STATE) {
            face.setState(static_cast<Face::State>(f.payload[1] % 4));
            leds.setState(f.payload[1]);
        } else if (f.len >= 4 && f.payload[0] == proto::CMD_LED_RGB) {
            leds.setSolid((static_cast<uint32_t>(f.payload[1]) << 16) |
                          (static_cast<uint32_t>(f.payload[2]) <<  8) |
                           static_cast<uint32_t>(f.payload[3]));
        }
        break;
    case proto::TEXT:
        face.setText(reinterpret_cast<const char *>(f.payload), f.len);
        break;
    default:
        break;
    }
}

int main() {

    gpio_rp2350 lcd_bl(EDU_LCD_BL);
    lcd_bl.gpioMode(GPIO::OUTPUT | GPIO::INIT_HIGH);

    gpio_rp2350 lcd_cs(EDU_LCD_CS);
    spi_rp2350  spi(EDU_LCD_MISO, EDU_LCD_MOSI, EDU_LCD_SCLK, lcd_cs);
    spi.setSpeed(24000000);

    gpio_rp2350 lcd_rst(EDU_LCD_RST);
    gpio_rp2350 lcd_dc(EDU_LCD_DC);
    st7735s_drv lcd(spi, lcd_rst, lcd_dc, st7735s_drv::Crystalfontz_128x128);

    uGUI     gui(lcd);
    Face     face(gui);
    MoodLeds leds;
    face.splash();
    face.begin();

    uart_rp2350 uart;

    adc_rp2350_channel mic(EDU_MIC);
    mic.adcMode(ADC::ADC_12_BIT);
    adc_rp2350_channel joy_y(EDU_JOY_Y);
    joy_y.adcMode(ADC::ADC_12_BIT);

    gpio_rp2350 s1(EDU_BUTTON1);
    s1.gpioMode(GPIO::INPUT | GPIO::PULLUP);
    gpio_rp2350 s2(EDU_BUTTON2);
    s2.gpioMode(GPIO::INPUT | GPIO::PULLUP);
    gpio_rp2350 joy_btn(EDU_JOY_BUTTON);
    joy_btn.gpioMode(GPIO::INPUT | GPIO::PULLUP);

    pcm_pwm_rp2350_drv pcm(EDU_BUZZER);
    pcm.setPcmRate(PLAY_RATE);
    pcm.enable_output(true);

    Menu menu(gui);

    const Face::Mood MOODS[] = { Face::NEUTRAL, Face::HAPPY, Face::SAD,
                                 Face::ANGRY, Face::CURIOUS };
    const uint32_t   LED_COLORS[] = { 0xFF0000, 0x00FF00, 0x0000FF, 0x787878, 0 };
    int mood_idx = 0, led_idx = 0;

    static proto::Decoder decoder;
    static proto::Frame   frame;
    uint64_t next_tick     = 0;
    uint64_t talk_deadline = 0;
    bool s1_down = false, s2_down = false, joy_down = false, joy_centered = true;

    while (true) {

        while (uart.available()) {
            if (decoder.feed(static_cast<uint8_t>(uart.getc()), frame)) {
                if (frame.type == proto::AUDIO_OUT) {

                    int len = 0;
                    bool done = (frame.len == 0);
                    for (int k = 0; k < frame.len && len < PLAY_CAP; ++k)
                        g_play[len++] = frame.payload[k];
                    uint64_t deadline = task::millis() + 12000;
                    while (!done && task::millis() < deadline) {
                        if (uart.available() &&
                            decoder.feed(static_cast<uint8_t>(uart.getc()), frame) &&
                            frame.type == proto::AUDIO_OUT) {
                            if (frame.len == 0) done = true;
                            else for (int k = 0; k < frame.len && len < PLAY_CAP; ++k)
                                g_play[len++] = frame.payload[k];
                        }
                    }
                    face.setState(Face::SPEAKING);
                    leds.setState(MoodLeds::SPEAKING);
                    face.tick();
                    leds.tick();
                    play_audio(pcm, g_play, len);
                    face.forceRedraw();
                    talk_deadline = 0;
                } else {
                    handle_frame(frame, face, leds);
                    talk_deadline = 0;
                }
            }
        }

        if (talk_deadline && task::millis() > talk_deadline) {
            face.setState(Face::IDLE);
            leds.setState(MoodLeds::IDLE);
            talk_deadline = 0;
        }

        bool s2p = (s2.gpioRead() == LOW);
        if (s2p && !s2_down) {
            task::sleep_ms(15);
            if (s2.gpioRead() == LOW) {
                if (menu.isOpen()) { menu.close(); face.forceRedraw(); }
                else                 menu.open();
            }
        }
        s2_down = s2p;

        if (menu.isOpen()) {

            uint16_t y = joy_y.adcReadRaw();
            if (joy_centered) {
                if (y < 1200)      { menu.moveUp();   joy_centered = false; }
                else if (y > 2900) { menu.moveDown(); joy_centered = false; }
            } else if (y > 1600 && y < 2500) {
                joy_centered = true;
            }

            bool jp = (joy_btn.gpioRead() == LOW);
            if (jp && !joy_down) {
                task::sleep_ms(15);
                if (joy_btn.gpioRead() == LOW) {
                    switch (menu.confirm()) {
                        case Menu::TALK:
                            menu.close();
                            capture_and_stream(uart, mic, face, leds, nullptr, 3000);
                            face.forceRedraw();
                            talk_deadline = task::millis() + 9000;
                            break;
                        case Menu::MOOD:
                            mood_idx = (mood_idx + 1) % 5;
                            face.setMood(MOODS[mood_idx]);
                            leds.setMood(static_cast<uint8_t>(mood_idx));
                            break;
                        case Menu::LEDS:
                            led_idx = (led_idx + 1) % 5;
                            leds.setSolid(LED_COLORS[led_idx]);
                            break;
                        case Menu::STATUS: {
                            menu.close();
                            const char *s = "Edge AI ready. LLM via Groq.";
                            face.setText(s, 28);
                            face.forceRedraw();
                            break;
                        }
                        case Menu::RESET:

                            *(volatile uint32_t *)0xE000ED0C = 0x05FA0004;
                            while (true) { }
                            break;
                        case Menu::CLOSE:
                            menu.close();
                            face.forceRedraw();
                            break;
                        default: break;
                    }
                }
            }
            joy_down = jp;

            if (menu.dirty()) menu.draw();
            if (task::millis() >= next_tick) {
                leds.tick();
                next_tick = task::millis() + 60;
            }
        } else {

            if (face.showingText()) {
                uint16_t jy = joy_y.adcReadRaw();
                if (joy_centered) {
                    if (jy < 1200)      { face.scrollText(-1); joy_centered = false; }
                    else if (jy > 2900) { face.scrollText(+1); joy_centered = false; }
                } else if (jy > 1600 && jy < 2500) {
                    joy_centered = true;
                }
            }

            bool s1p = (s1.gpioRead() == LOW);
            if (s1p && !s1_down) {
                task::sleep_ms(20);
                if (s1.gpioRead() == LOW) {
                    capture_and_stream(uart, mic, face, leds, &s1, 0);
                    next_tick = 0;
                    talk_deadline = task::millis() + 9000;
                }
            }
            s1_down = s1p;

            if (task::millis() >= next_tick) {
                face.tick();
                leds.tick();
                next_tick = task::millis() + 60;
            }
        }
    }
}

// ---------------------------------------------
//  Edge AI Voice Assistant - RP2350 firmware
//  Module: Mikrocontrollertechnik (FH Aachen)
//  Authors: Saad Fihi, Zakaria Sabiri
//
//  Integrated output firmware: the board is the visible
//  front-end of the assistant. It receives framed commands
//  from the host bridge over the back-channel UART (which
//  the on-board debug probe bridges to the laptop as a USB
//  serial port) and drives:
//
//    MOOD frame            -> animated LCD face + LED mood color
//    CMD(CMD_STATE)        -> face/LED state (listen/think/speak)
//    CMD(CMD_LED_RGB)      -> solid LED color (function-calling)
//    TEXT frame            -> acknowledged (LCD caption: next step)
//
//  After each valid frame the board sends back an ACK, and a
//  LOG greeting once at boot, so the host can confirm the link.
//  The face and LEDs animate continuously regardless of the
//  link, so the device never looks dead.
//
//  Link note: on this LaunchPad the single USB cable goes to
//  the debug probe, not to the RP2350's native USB. The probe
//  bridges the back-channel UART (BC_UART_TX/RX), so we talk
//  UART here. Shared protocol: src/protocol.h.
// ---------------------------------------------
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

// --- microphone capture / streaming (push-to-talk) ---------------------------
namespace {
    constexpr int      SAMPLE_RATE   = 16000;            // matches host STT (Vosk)
    constexpr int      MAX_SAMPLES   = SAMPLE_RATE * 6;  // 6 s cap (192 KB)
    constexpr int      MIN_SAMPLES   = SAMPLE_RATE / 3;  // ignore < ~0.33 s blips
    constexpr int      CHUNK8        = 480;              // 8-bit samples per frame

    constexpr int      PLAY_RATE     = 8000;             // buzzer playback rate
    constexpr int      PLAY_CAP      = 8000 * 8;         // 8 s of 8-bit audio

    int16_t g_audio[MAX_SAMPLES];                        // capture buffer (BSS)
    uint8_t g_pay[CHUNK8];                               // 8-bit payload scratch
    uint8_t g_tx[proto::OVERHEAD + CHUNK8];              // one encoded frame
    uint8_t g_play[PLAY_CAP];                            // received reply audio
}

// Play 8-bit signed PCM through the buzzer (PWM). Blocking: feeds the driver's
// FIFO at the playback rate until the whole clip is out.
static void play_audio(pcm_pwm_rp2350_drv &pcm, const uint8_t *data, int n) {
    pcm.timer_reset();
    for (int i = 0; i < n; ++i) {
        int16_t s = static_cast<int16_t>(static_cast<int8_t>(data[i])) << 8;
        pcm_value_t v{ s, s };
        while (pcm.pcmFifoAvailablePut() < 1) { }   // wait for FIFO space
        pcm.pcmFifoPut(v);
    }
    task::sleep_ms(60);                              // let the FIFO drain
}

static void uart_send(uart_rp2350 &u, const uint8_t *data, size_t n) {
    for (size_t i = 0; i < n; ++i) u.putc(static_cast<char>(data[i]));
}

// Record from the microphone at 16 kHz and stream it to the host as AUDIO_IN
// frames (16-bit signed mono, little-endian), ended by an empty frame.
// If `hold` is given, records WHILE the button is held (press-and-hold to talk),
// up to MAX_SAMPLES; otherwise records a fixed `fixed_ms`. Blocking on purpose.
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
    int release = 0;                       // consecutive "released" reads
    while (n < limit) {
        if (hold) {
            if (hold->gpioRead() != LOW) {
                // need ~200 ms of sustained release to stop (ignore bounce/noise,
                // so it never ends before you actually let go of the button)
                if (++release > 3200 && n >= MIN_SAMPLES) break;
            } else {
                release = 0;
            }
        }
        uint16_t v = mic.adcReadRaw();                       // 12-bit, ~2048 centered
        g_audio[n] = static_cast<int16_t>((static_cast<int>(v) - 2048) << 4);
        ++n;
        uint64_t next = start + static_cast<uint64_t>(n) * 1000000ULL / SAMPLE_RATE;
        while (task::micros() < next) { }
    }

    face.setState(Face::THINKING);
    leds.setState(MoodLeds::THINKING);
    face.tick();
    leds.tick();

    // Stream as 8-bit signed samples (high byte) to halve the link load - the
    // debug-probe UART bridge is much more likely to deliver it intact.
    // Apply gain BEFORE the 8-bit downscale: speech is quiet, so without this
    // the 8-bit samples only use a few levels and the recognizer gets garbage.
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
    size_t len = proto::encode(proto::AUDIO_IN, seq++, nullptr, 0, g_tx);  // end marker
    uart_send(uart, g_tx, len);
}

// Act on one decoded, CRC-valid frame from the host.
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
    // --- LCD + face + LEDs come up first, so the device is alive at once ---
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
    face.begin();

    // --- host link: back-channel UART, bridged by the debug probe ---------
    // Normally we only receive (the probe's UART bridge is more reliable one-
    // directional). We transmit ONLY in bursts, to stream captured microphone
    // audio after a button press (push-to-talk).
    uart_rp2350 uart;  // defaults: BC_UART_TX/RX, 115200 8N1

    // --- microphone + buttons + joystick ----------------------------------
    adc_rp2350_channel mic(EDU_MIC);
    mic.adcMode(ADC::ADC_12_BIT);
    adc_rp2350_channel joy_y(EDU_JOY_Y);              // menu navigation
    joy_y.adcMode(ADC::ADC_12_BIT);

    gpio_rp2350 s1(EDU_BUTTON1);                      // push-to-talk
    s1.gpioMode(GPIO::INPUT | GPIO::PULLUP);
    gpio_rp2350 s2(EDU_BUTTON2);                      // open/close menu
    s2.gpioMode(GPIO::INPUT | GPIO::PULLUP);
    gpio_rp2350 joy_btn(EDU_JOY_BUTTON);              // confirm in menu
    joy_btn.gpioMode(GPIO::INPUT | GPIO::PULLUP);

    pcm_pwm_rp2350_drv pcm(EDU_BUZZER);               // speak replies via buzzer
    pcm.setPcmRate(PLAY_RATE);
    pcm.enable_output(true);

    Menu menu(gui);

    // Cycled by the menu's Humeur / LEDs items.
    const Face::Mood MOODS[] = { Face::NEUTRAL, Face::HAPPY, Face::SAD,
                                 Face::ANGRY, Face::CURIOUS };
    const uint32_t   LED_COLORS[] = { 0xFF0000, 0x00FF00, 0x0000FF, 0x787878, 0 };
    int mood_idx = 0, led_idx = 0;

    // --- main loop --------------------------------------------------------
    static proto::Decoder decoder;
    static proto::Frame   frame;
    uint64_t next_tick     = 0;
    uint64_t talk_deadline = 0;    // after talking, fall back to idle if no reply
    bool s1_down = false, s2_down = false, joy_down = false, joy_centered = true;

    while (true) {
        // 1) Drain the UART (host commands). Any reply cancels the talk timeout.
        while (uart.available()) {
            if (decoder.feed(static_cast<uint8_t>(uart.getc()), frame)) {
                if (frame.type == proto::AUDIO_OUT) {
                    // Reply audio: receive the whole clip in a tight loop (no
                    // animation -> no UART RX overflow), then play it on the buzzer.
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

        // If we sent audio but the host never answered, don't stay stuck.
        if (talk_deadline && task::millis() > talk_deadline) {
            face.setState(Face::IDLE);
            leds.setState(MoodLeds::IDLE);
            talk_deadline = 0;
        }

        // 2) S2 opens / closes the on-device menu.
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
            // Joystick up/down (one step per push, must re-center between steps).
            uint16_t y = joy_y.adcReadRaw();
            if (joy_centered) {
                if (y < 1200)      { menu.moveUp();   joy_centered = false; }
                else if (y > 2900) { menu.moveDown(); joy_centered = false; }
            } else if (y > 1600 && y < 2500) {
                joy_centered = true;
            }

            // Joystick button confirms the highlighted item.
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
                            // Full software reboot for a clean state if anything hangs.
                            *(volatile uint32_t *)0xE000ED0C = 0x05FA0004;  // SYSRESETREQ
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
            if (task::millis() >= next_tick) {        // LEDs still animate
                leds.tick();
                next_tick = task::millis() + 60;
            }
        } else {
            // S1 = press-and-hold to talk: record while held, stream on release.
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

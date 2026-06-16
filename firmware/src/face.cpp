// ---------------------------------------------
//  Edge AI Voice Assistant - animated robot face
//  Author: Zakaria Sabiri
//
//  Implementation of the Face view (see face.h).
//  Everything is drawn with plain uGUI primitives
//  (circles, lines, frames, pixels) so it needs no
//  bitmaps and stays tiny.
// ---------------------------------------------
#include "face.h"

#include "uGUI.h"
#include "uGUI_colors.h"
#include "font_6x8.h"
#include "font_8x12.h"

#include <cstring>

namespace {
    // ----- 128x128 layout -------------------------------------------------
    constexpr int16_t  W        = 128;
    constexpr UG_COLOR BG       = C_BLACK;       // screen background
    constexpr UG_COLOR PANEL    = 0x0A1430;      // dark-navy "head" panel

    // Head panel (rounded rectangle the face lives in) - near full width
    constexpr int16_t  HEAD_X1  = 1,  HEAD_Y1 = 16;
    constexpr int16_t  HEAD_X2  = 126, HEAD_Y2 = 112;

    // Eyes - spread wider to fill the bigger face
    constexpr int16_t  L_EYE_X  = 36, R_EYE_X = 92, EYE_Y = 54;
    constexpr int16_t  SCLERA_R = 17;

    // Mouth
    constexpr int16_t  MOUTH_X  = 64, MOUTH_Y = 90, MOUTH_W = 30;
    constexpr int16_t  MOUTH_AMP = 13;
}

Face::Face(uGUI &gui) : _gui(gui) {}

uint32_t Face::_moodColor() const {
    switch (_mood) {
        case HAPPY:   return C_LIME;
        case SAD:     return C_DODGER_BLUE;
        case ANGRY:   return C_RED;
        case CURIOUS: return C_GOLD;
        case NEUTRAL:
        default:      return C_CYAN;
    }
}

const char *Face::_stateLabel() const {
    switch (_state) {
        case LISTENING: return "LISTENING";
        case THINKING:  return "THINKING";
        case SPEAKING:  return "SPEAKING";
        case IDLE:
        default:        return "READY";
    }
}

void Face::setMood(Mood m)  { if (m != _mood)  { _mood = m;  _redrawStatic = true; } }
void Face::setState(State s){ if (s != _state) { _state = s; _redrawStatic = true; } }

void Face::setMood(const char *mood) {
    if (!mood)                       setMood(NEUTRAL);
    else if (!strcmp(mood, "happy")) setMood(HAPPY);
    else if (!strcmp(mood, "sad"))   setMood(SAD);
    else if (!strcmp(mood, "angry")) setMood(ANGRY);
    else if (!strcmp(mood, "curious")) setMood(CURIOUS);
    else                             setMood(NEUTRAL);
}

void Face::begin() {
    _gui.FillScreen(BG);
    _redrawStatic = true;
    _frame = 0;
    tick();
}

// ---- static frame: title + head panel + status bar -----------------------
void Face::_drawHead() {
    const UG_COLOR col = _moodColor();

    // Title bar
    _gui.FillFrame(0, 0, W - 1, 14, BG);
    _gui.SetForecolor(col);
    _gui.SetBackcolor(BG);
    _gui.FontSelect(&FONT_8X12);
    _gui.PutString(28, 2, "EDGE AI");

    // Head panel with a mood-colored outline
    _gui.FillRoundFrame(HEAD_X1, HEAD_Y1, HEAD_X2, HEAD_Y2, 12, PANEL);
    _gui.DrawRoundFrame(HEAD_X1, HEAD_Y1, HEAD_X2, HEAD_Y2, 12, col);
}

void Face::_clearFaceArea() {
    // Wipe the dynamic interior back to the panel color (no full-screen
    // clear -> no flicker), leaving the rounded border intact.
    _gui.FillFrame(HEAD_X1 + 2, HEAD_Y1 + 2, HEAD_X2 - 2, HEAD_Y2 - 2, PANEL);
}

// ---- one eye: white sclera, mood-colored iris, dark pupil, highlight -----
static void drawEye(uGUI &gui, int16_t cx, int16_t cy,
                    int16_t dx, int16_t dy, UG_COLOR iris, bool blink) {
    if (blink) {                       // closed: a thick lid line
        gui.FillFrame(cx - SCLERA_R, cy - 2, cx + SCLERA_R, cy + 2, C_GRAY);
        return;
    }
    gui.FillCircle(cx, cy, SCLERA_R, C_WHITE);
    gui.FillCircle(cx + dx, cy + dy, 8, iris);
    gui.FillCircle(cx + dx, cy + dy, 4, C_BLACK);
    gui.DrawPixel (cx + dx - 2, cy + dy - 2, C_WHITE);  // catch-light
}

void Face::_drawEyes() {
    const UG_COLOR col = _moodColor();

    // --- where the pupils look, per state -------------------------------
    int16_t dx = 0, dy = 0;
    bool blink = false;

    if (_state == LISTENING) {                 // scan left<->right
        int p = _frame % 16;
        dx = (p < 8) ? (p - 4) : (12 - p);     // -4 .. +4 .. -4
    } else if (_state == THINKING) {           // look up and circle around
        static const int8_t ox[8] = { 0, 3, 4, 3, 0, -3, -4, -3 };
        static const int8_t oy[8] = {-5,-4,-2,-1,-2, -4, -5, -5 };
        int i = (_frame / 2) % 8;
        dx = ox[i]; dy = oy[i];
    } else if (_state == IDLE) {               // occasional blink
        blink = (_frame % 28) < 2;
    }

    // mood nudges the gaze a little
    if (_mood == SAD)     dy += 3;
    if (_mood == CURIOUS) dy -= 2;

    drawEye(_gui, L_EYE_X, EYE_Y, dx, dy, col, blink);
    drawEye(_gui, R_EYE_X, EYE_Y, dx, dy, col, blink);

    if (blink) return;

    // --- eyebrows give the mood its character ---------------------------
    const int16_t by = EYE_Y - SCLERA_R - 5;   // brow baseline
    if (_mood == ANGRY) {                       // angled inward / down
        _gui.DrawLine(L_EYE_X - 14, by, L_EYE_X + 8, by + 7, col);
        _gui.DrawLine(R_EYE_X + 14, by, R_EYE_X - 8, by + 7, col);
    } else if (_mood == SAD) {                   // angled outward / down
        _gui.DrawLine(L_EYE_X - 14, by + 7, L_EYE_X + 8, by, col);
        _gui.DrawLine(R_EYE_X + 14, by + 7, R_EYE_X - 8, by, col);
    } else if (_mood == CURIOUS) {               // one raised brow
        _gui.DrawLine(L_EYE_X - 12, by + 2, L_EYE_X + 10, by + 2, col);
        _gui.DrawLine(R_EYE_X - 10, by - 4, R_EYE_X + 12, by - 4, col);
    }
}

// ---- mouth: a curve whose shape follows the mood, animated when speaking -
void Face::_drawMouth() {
    const UG_COLOR col = _moodColor();

    if (_state == SPEAKING) {                    // open mouth that "talks"
        static const int8_t open[6] = { 3, 7, 11, 7, 3, 2 };
        int h = open[_frame % 6];
        _gui.FillRoundFrame(MOUTH_X - 14, MOUTH_Y - h,
                            MOUTH_X + 14, MOUTH_Y + h,
                            (h < 10 ? h : 10), col);
        _gui.FillRoundFrame(MOUTH_X - 14 + 3, MOUTH_Y - h + 2,
                            MOUTH_X + 14 - 3, MOUTH_Y + h - 2,
                            (h < 8 ? h : 8), 0x300000);
        return;
    }

    if (_state == THINKING) {                    // three animated dots
        int active = (_frame / 3) % 3;
        for (int k = 0; k < 3; ++k) {
            int16_t x = MOUTH_X - 18 + k * 18;
            _gui.FillCircle(x, MOUTH_Y, (k == active) ? 5 : 3,
                            (k == active) ? col : C_GRAY);
        }
        return;
    }

    if (_mood == CURIOUS) {                       // small round "o"
        _gui.FillCircle(MOUTH_X, MOUTH_Y, 7, col);
        _gui.FillCircle(MOUTH_X, MOUTH_Y, 4, PANEL);
        return;
    }

    if (_mood == NEUTRAL) {                        // flat line
        _gui.FillFrame(MOUTH_X - MOUTH_W, MOUTH_Y - 1,
                       MOUTH_X + MOUTH_W, MOUTH_Y + 1, col);
        return;
    }

    // HAPPY -> smile (corners up), SAD/ANGRY -> frown (corners down)
    const bool smile = (_mood == HAPPY);
    const int16_t amp = (_mood == ANGRY) ? 8 : MOUTH_AMP;
    for (int16_t x = -MOUTH_W; x <= MOUTH_W; ++x) {
        int t100 = (x * 100) / MOUTH_W;                 // -100..100
        int yoff = (amp * (10000 - t100 * t100)) / 10000;
        int16_t py = smile ? (MOUTH_Y + yoff) : (MOUTH_Y - yoff);
        _gui.DrawPixel(MOUTH_X + x, py,     col);
        _gui.DrawPixel(MOUTH_X + x, py + 1, col);       // 2 px thick
    }
}

void Face::_drawStatus() {
    _gui.FillFrame(0, 116, W - 1, W - 1, BG);
    _gui.SetForecolor(_moodColor());
    _gui.SetBackcolor(BG);
    _gui.FontSelect(&FONT_6X8);
    _gui.PutString(4, 118, _stateLabel());
}

void Face::forceRedraw() {
    _gui.FillScreen(0x000000);
    _redrawStatic = true;
}

void Face::setText(const char *s, uint16_t len) {
    uint16_t n = (len < TEXT_CAP - 1) ? len : (TEXT_CAP - 1);
    for (uint16_t i = 0; i < n; ++i) _text[i] = s[i];
    _text[n] = 0;
    // Show the reply for a while regardless of state changes, then auto-return
    // to the animated face. ~120 frames at 60 ms ≈ 7 s.
    _textFrames = (n > 0) ? 120 : 0;
    _redrawStatic = true;
}

// Full-screen, word-wrapped rendering of the reply text. Drawn once when the
// screen switches into text mode (not every frame), to avoid flicker.
void Face::_drawTextScreen() {
    const UG_COLOR col = _moodColor();
    _gui.FillScreen(0x000000);
    _gui.FontSelect(&FONT_6X8);
    _gui.SetBackcolor(0x000000);
    _gui.SetForecolor(col);
    _gui.PutString(4, 2, "REPLY");
    _gui.DrawLine(0, 12, 127, 12, col);
    _gui.SetForecolor(0xFFFFFF);

    constexpr int MAXC = 20, X0 = 4, LH = 10, YMAX = 116;
    char line[MAXC + 1];
    int  ll = 0, y = 16;

    auto flush = [&]() {
        line[ll] = 0;
        if (y <= YMAX) _gui.PutString(X0, y, line);
        y += LH;
        ll = 0;
    };

    char word[MAXC + 1];
    int  wl = 0;
    for (int i = 0;; ++i) {
        char c = _text[i];
        bool end = (c == 0);
        if (c == ' ' || c == '\n' || end) {
            if (wl > 0) {
                int need = (ll ? ll + 1 : 0) + wl;
                if (need > MAXC) flush();               // word starts a new line
                if (ll && ll < MAXC) line[ll++] = ' ';
                for (int k = 0; k < wl && ll < MAXC; ++k) line[ll++] = word[k];
                wl = 0;
            }
            if (c == '\n') flush();
            if (end) { if (ll) flush(); break; }
        } else if (wl < MAXC) {
            word[wl++] = c;
        }
    }
}

void Face::tick() {
    if (_textFrames > 0) --_textFrames;
    const bool wantText = (_textFrames > 0 && _text[0] != 0);
    if (wantText != _showText) {
        const bool leavingText = (_showText && !wantText);
        _showText = wantText;
        _redrawStatic = true;
        if (leavingText) _gui.FillScreen(0x000000);  // wipe text before the face
    }

    if (_redrawStatic) {
        if (_showText) _drawTextScreen();          // clears + draws wrapped text
        else { _drawHead(); _drawStatus(); }
        _redrawStatic = false;
    }

    if (_showText) {
        // small blinking "speaking" dot, top-right corner
        const bool on = ((_frame / 4) & 1);
        _gui.FillCircle(120, 6, 3, on ? _moodColor() : 0x000000);
    } else {
        _clearFaceArea();
        _drawEyes();
        _drawMouth();
    }
    ++_frame;
}

#include "face.h"

#include "uGUI.h"
#include "uGUI_colors.h"
#include "font_6x8.h"
#include "font_8x12.h"
#include "task.h"

#include <cstring>

namespace {

    constexpr int16_t  W        = 128;
    constexpr UG_COLOR BG       = C_BLACK;
    constexpr UG_COLOR PANEL    = 0x0A1430;

    constexpr int16_t  HEAD_X1  = 1,  HEAD_Y1 = 16;
    constexpr int16_t  HEAD_X2  = 126, HEAD_Y2 = 112;

    constexpr int16_t  L_EYE_X  = 36, R_EYE_X = 92, EYE_Y = 54;
    constexpr int16_t  SCLERA_R = 17;

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

void Face::splash() {
    const UG_COLOR ACC = 0x1E90FF;
    _gui.FillScreen(0x000000);

    for (int y = 0; y <= 128; y += 7) {
        _gui.DrawLine(0, y, 127, y, 0x12243f);
        task::sleep_ms(6);
    }
    _gui.FillScreen(0x000000);

    _gui.SetBackcolor(0x000000);
    _gui.FontSelect(&FONT_8X12);
    _gui.SetForecolor(ACC);
    _gui.PutString(22, 48, "EDGE AI");
    _gui.FontSelect(&FONT_6X8);
    _gui.SetForecolor(0x6b7280);
    _gui.PutString(16, 68, "Voice Assistant");

    for (int x = 0; x <= 118; x += 5) {
        _gui.DrawLine(5, 84, 5 + x, 84, ACC);
        task::sleep_ms(8);
    }
    task::sleep_ms(450);
}

void Face::begin() {
    _gui.FillScreen(BG);
    _redrawStatic = true;
    _frame = 0;
    tick();
}

void Face::_drawHead() {
    const UG_COLOR col = _moodColor();

    _gui.FillFrame(0, 0, W - 1, 14, BG);
    _gui.SetForecolor(col);
    _gui.SetBackcolor(BG);
    _gui.FontSelect(&FONT_8X12);
    _gui.PutString(28, 2, "EDGE AI");

    _gui.FillRoundFrame(HEAD_X1, HEAD_Y1, HEAD_X2, HEAD_Y2, 12, PANEL);
    _gui.DrawRoundFrame(HEAD_X1, HEAD_Y1, HEAD_X2, HEAD_Y2, 12, col);
}

void Face::_clearFaceArea() {

    _gui.FillFrame(HEAD_X1 + 2, HEAD_Y1 + 2, HEAD_X2 - 2, HEAD_Y2 - 2, PANEL);
}

static void drawEye(uGUI &gui, int16_t cx, int16_t cy,
                    int16_t dx, int16_t dy, UG_COLOR iris, bool blink) {
    if (blink) {
        gui.FillFrame(cx - SCLERA_R, cy - 2, cx + SCLERA_R, cy + 2, C_GRAY);
        return;
    }
    gui.FillCircle(cx, cy, SCLERA_R, C_WHITE);
    gui.FillCircle(cx + dx, cy + dy, 8, iris);
    gui.FillCircle(cx + dx, cy + dy, 4, C_BLACK);
    gui.DrawPixel (cx + dx - 2, cy + dy - 2, C_WHITE);
}

void Face::_drawEyes() {
    const UG_COLOR col = _moodColor();

    int16_t dx = 0, dy = 0;
    bool blink = false;

    if (_state == LISTENING) {
        int p = _frame % 16;
        dx = (p < 8) ? (p - 4) : (12 - p);
    } else if (_state == THINKING) {
        static const int8_t ox[8] = { 0, 3, 4, 3, 0, -3, -4, -3 };
        static const int8_t oy[8] = {-5,-4,-2,-1,-2, -4, -5, -5 };
        int i = (_frame / 2) % 8;
        dx = ox[i]; dy = oy[i];
    } else if (_state == IDLE) {
        blink = (_frame % 28) < 2;
    }

    if (_mood == SAD)     dy += 3;
    if (_mood == CURIOUS) dy -= 2;

    drawEye(_gui, L_EYE_X, EYE_Y, dx, dy, col, blink);
    drawEye(_gui, R_EYE_X, EYE_Y, dx, dy, col, blink);

    if (blink) return;

    const int16_t by = EYE_Y - SCLERA_R - 5;
    if (_mood == ANGRY) {
        _gui.DrawLine(L_EYE_X - 14, by, L_EYE_X + 8, by + 7, col);
        _gui.DrawLine(R_EYE_X + 14, by, R_EYE_X - 8, by + 7, col);
    } else if (_mood == SAD) {
        _gui.DrawLine(L_EYE_X - 14, by + 7, L_EYE_X + 8, by, col);
        _gui.DrawLine(R_EYE_X + 14, by + 7, R_EYE_X - 8, by, col);
    } else if (_mood == CURIOUS) {
        _gui.DrawLine(L_EYE_X - 12, by + 2, L_EYE_X + 10, by + 2, col);
        _gui.DrawLine(R_EYE_X - 10, by - 4, R_EYE_X + 12, by - 4, col);
    }
}

void Face::_drawMouth() {
    const UG_COLOR col = _moodColor();

    if (_state == SPEAKING) {
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

    if (_state == THINKING) {
        int active = (_frame / 3) % 3;
        for (int k = 0; k < 3; ++k) {
            int16_t x = MOUTH_X - 18 + k * 18;
            _gui.FillCircle(x, MOUTH_Y, (k == active) ? 5 : 3,
                            (k == active) ? col : C_GRAY);
        }
        return;
    }

    if (_mood == CURIOUS) {
        _gui.FillCircle(MOUTH_X, MOUTH_Y, 7, col);
        _gui.FillCircle(MOUTH_X, MOUTH_Y, 4, PANEL);
        return;
    }

    if (_mood == NEUTRAL) {
        _gui.FillFrame(MOUTH_X - MOUTH_W, MOUTH_Y - 1,
                       MOUTH_X + MOUTH_W, MOUTH_Y + 1, col);
        return;
    }

    const bool smile = (_mood == HAPPY);
    const int16_t amp = (_mood == ANGRY) ? 8 : MOUTH_AMP;
    for (int16_t x = -MOUTH_W; x <= MOUTH_W; ++x) {
        int t100 = (x * 100) / MOUTH_W;
        int yoff = (amp * (10000 - t100 * t100)) / 10000;
        int16_t py = smile ? (MOUTH_Y + yoff) : (MOUTH_Y - yoff);
        _gui.DrawPixel(MOUTH_X + x, py,     col);
        _gui.DrawPixel(MOUTH_X + x, py + 1, col);
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
    _wrapText();
    _textScroll = 0;

    _textFrames = (n > 0) ? (180 + _lineCount * 20) : 0;
    _redrawStatic = true;
}

void Face::_wrapText() {
    constexpr int MAXC = LINE_W - 1;
    _lineCount = 0;
    char cur[LINE_W];
    int  ll = 0;
    char word[LINE_W];
    int  wl = 0;
    for (int i = 0;; ++i) {
        char c = _text[i];
        bool end = (c == 0);
        if (c == ' ' || c == '\n' || end) {
            if (wl > 0) {
                int need = (ll ? ll + 1 : 0) + wl;
                if (need > MAXC && _lineCount < LINE_CAP) {
                    cur[ll] = 0;
                    for (int k = 0; k <= ll; ++k) _lines[_lineCount][k] = cur[k];
                    ++_lineCount;
                    ll = 0;
                }
                if (ll && ll < MAXC) cur[ll++] = ' ';
                for (int k = 0; k < wl && ll < MAXC; ++k) cur[ll++] = word[k];
                wl = 0;
            }
            if ((c == '\n' || end) && (ll || end) && _lineCount < LINE_CAP) {
                if (ll || end) {
                    cur[ll] = 0;
                    for (int k = 0; k <= ll; ++k) _lines[_lineCount][k] = cur[k];
                    if (ll) ++_lineCount;
                    ll = 0;
                }
            }
            if (end) break;
        } else if (wl < MAXC) {
            word[wl++] = c;
        }
    }
}

void Face::scrollText(int delta) {
    int maxScroll = _lineCount - VISIBLE;
    if (maxScroll < 0) maxScroll = 0;
    int s = _textScroll + delta;
    if (s < 0) s = 0;
    if (s > maxScroll) s = maxScroll;
    if (s != _textScroll) {
        _textScroll = s;
        _redrawStatic = true;
    }
    _textFrames = 120;
}

void Face::_drawTextScreen() {
    const UG_COLOR col = _moodColor();
    _gui.FillScreen(0x000000);
    _gui.FontSelect(&FONT_6X8);
    _gui.SetBackcolor(0x000000);
    _gui.SetForecolor(col);
    _gui.PutString(4, 2, "REPLY");
    _gui.DrawLine(0, 12, 127, 12, col);

    constexpr int X0 = 4, LH = 10, Y0 = 16;
    _gui.SetForecolor(0xFFFFFF);
    for (int r = 0; r < VISIBLE; ++r) {
        int li = _textScroll + r;
        if (li >= _lineCount) break;
        _gui.PutString(X0, Y0 + r * LH, _lines[li]);
    }

    _gui.SetForecolor(col);
    if (_textScroll > 0)                    _gui.PutString(118, 2, "^");
    if (_textScroll + VISIBLE < _lineCount) _gui.PutString(118, 118, "v");
}

void Face::tick() {
    if (_textFrames > 0) --_textFrames;
    const bool wantText = (_textFrames > 0 && _text[0] != 0);
    if (wantText != _showText) {
        const bool leavingText = (_showText && !wantText);
        _showText = wantText;
        _redrawStatic = true;
        if (leavingText) _gui.FillScreen(0x000000);
    }

    if (_redrawStatic) {
        if (_showText) _drawTextScreen();
        else { _drawHead(); _drawStatus(); }
        _redrawStatic = false;
    }

    if (!_showText) {
        _clearFaceArea();
        _drawEyes();
        _drawMouth();
    }
    ++_frame;
}

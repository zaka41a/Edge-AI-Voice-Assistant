// ---------------------------------------------
//  Edge AI Voice Assistant - on-device menu
//  Author: Zakaria Sabiri
//  Implementation, see menu.h.
// ---------------------------------------------
#include "menu.h"

#include "uGUI.h"
#include "uGUI_colors.h"
#include "font_6x8.h"
#include "font_8x12.h"

namespace {
    const char *ITEMS[]  = { "Talk", "Mood", "LEDs", "Status", "Reset", "Close" };
    const char *ICONS[]  = { ">",    "*",    "#",    "i",      "!",     "x"     };
    constexpr UG_COLOR BG      = 0x000000;
    constexpr UG_COLOR ACCENT  = 0x1E90FF;   // dodger blue
    constexpr UG_COLOR ROW_BG  = 0x101826;
    constexpr int      ROW_H   = 15;
    constexpr int      ROW_Y0  = 21;
}

Menu::Menu(uGUI &gui) : _gui(gui) {}

void Menu::open()  { _open = true;  _sel = 0; _dirty = true; }
void Menu::close() { _open = false; _dirty = false; }

void Menu::moveUp()   { _sel = (_sel + COUNT - 1) % COUNT; _dirty = true; }
void Menu::moveDown() { _sel = (_sel + 1) % COUNT;          _dirty = true; }

Menu::Action Menu::confirm() {
    switch (_sel) {
        case 0: return TALK;
        case 1: _dirty = true; return MOOD;     // stays open, cycles mood
        case 2: _dirty = true; return LEDS;     // stays open, cycles LEDs
        case 3: return STATUS;
        case 4: return RESET;
        case 5: return CLOSE;
        default: return NONE;
    }
}

void Menu::draw() {
    _dirty = false;
    _gui.FillScreen(BG);

    // Title bar
    _gui.SetForecolor(ACCENT);
    _gui.SetBackcolor(BG);
    _gui.FontSelect(&FONT_8X12);
    _gui.PutString(8, 3, "MENU");
    _gui.DrawLine(0, 17, 127, 17, ACCENT);

    // Items
    _gui.FontSelect(&FONT_6X8);
    for (int i = 0; i < COUNT; ++i) {
        int y = ROW_Y0 + i * ROW_H;
        bool on = (i == _sel);
        if (on) {
            _gui.FillRoundFrame(3, y - 2, 124, y + ROW_H - 6, 4, ROW_BG);
            _gui.DrawRoundFrame(3, y - 2, 124, y + ROW_H - 6, 4, ACCENT);
        }
        _gui.SetBackcolor(on ? ROW_BG : BG);avant
        _gui.SetForecolor(on ? ACCENT : 0x808890);
        _gui.PutString(10, y + 1, ICONS[i]);
        _gui.SetForecolor(on ? 0xFFFFFF : 0xB8C0CC);
        _gui.PutString(28, y + 1, ITEMS[i]);
    }

    // Footer hint
    _gui.SetBackcolor(BG);
    _gui.SetForecolor(0x5A6270);
    _gui.PutString(6, 120, "Joy: move   Btn: ok");
}

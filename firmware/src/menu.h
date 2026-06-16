// ---------------------------------------------
//  Edge AI Voice Assistant - on-device menu
//  Module: Mikrocontrollertechnik (FH Aachen)
//  Author: Zakaria Sabiri
//
//  A small, professional menu drawn on the LCD and
//  navigated with the BoosterPack joystick (up/down)
//  and confirmed with the joystick button. Opened with
//  the S2 button. Lets the user act on the board itself:
//  talk, change mood, change LEDs, show status.
// ---------------------------------------------
#ifndef EDGE_AI_MENU_H
#define EDGE_AI_MENU_H

#include <cstdint>

class uGUI;

class Menu {
public:
    enum Action { NONE, TALK, MOOD, LEDS, STATUS, RESET, CLOSE };

    explicit Menu(uGUI &gui);

    bool isOpen() const { return _open; }
    void open();
    void close();

    void moveUp();
    void moveDown();
    Action confirm();          // act on the highlighted item

    void draw();               // render the whole menu
    bool dirty() const { return _dirty; }   // needs a redraw?

private:
    static constexpr int COUNT = 6;

    uGUI &_gui;
    bool  _open  = false;
    bool  _dirty = false;
    int   _sel   = 0;
};

#endif  // EDGE_AI_MENU_H

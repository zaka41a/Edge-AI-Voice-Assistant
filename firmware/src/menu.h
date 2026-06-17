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
    Action confirm();

    void draw();
    bool dirty() const { return _dirty; }

private:
    static constexpr int COUNT = 6;

    uGUI &_gui;
    bool  _open  = false;
    bool  _dirty = false;
    int   _sel   = 0;
};

#endif

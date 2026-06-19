#ifndef EDGE_AI_FACE_H
#define EDGE_AI_FACE_H

#include <cstdint>

class uGUI;

class Face {
public:

    enum Mood  { NEUTRAL, HAPPY, SAD, ANGRY, CURIOUS };

    enum State { IDLE, LISTENING, THINKING, SPEAKING };

    explicit Face(uGUI &gui);

    void splash();

    void begin();

    void setMood(Mood m);
    void setState(State s);

    void setMood(const char *mood);

    void setText(const char *s, uint16_t len);

    bool showingText() const { return _showText; }

    void scrollText(int delta);

    void tick();

    void forceRedraw();

private:
    static constexpr int TEXT_CAP  = 256;
    static constexpr int LINE_CAP  = 20;
    static constexpr int LINE_W    = 22;
    static constexpr int VISIBLE   = 10;

    uGUI    &_gui;
    Mood     _mood        = NEUTRAL;
    State    _state       = IDLE;
    uint32_t _frame       = 0;
    bool     _redrawStatic = true;
    bool     _showText     = false;
    int      _textFrames   = 0;
    int      _textScroll   = 0;
    int      _lineCount    = 0;
    char     _text[TEXT_CAP] = {0};
    char     _lines[LINE_CAP][LINE_W] = {{0}};

    void     _wrapText();

    uint32_t _moodColor() const;
    const char *_stateLabel() const;

    int16_t _lastDx = 999, _lastDy = 999;
    bool    _lastBlink = false;

    void _drawHead();
    void _drawEyes(bool force);
    void _drawMouth(bool force);
    void _drawStatus();
    void _drawTextScreen();
};

#endif

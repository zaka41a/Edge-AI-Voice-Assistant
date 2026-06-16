// ---------------------------------------------
//  Edge AI Voice Assistant - animated robot face
//  Module: Mikrocontrollertechnik (FH Aachen)
//  Author: Zakaria Sabiri
//
//  Turns the BoosterPack MKII LCD into a little
//  character that reacts to what the assistant is
//  doing (listening / thinking / speaking) and to
//  the emotional tone of the reply (the "mood").
//
//  The face is purely a view: the host bridge tells
//  the board "the mood is happy, you are speaking",
//  and this class renders it. Drawn with uGUI on the
//  ST7735S 128x128 display.
// ---------------------------------------------
#ifndef EDGE_AI_FACE_H
#define EDGE_AI_FACE_H

#include <cstdint>

class uGUI;  // forward declaration, full type only needed in face.cpp

class Face {
public:
    // Emotional tone of the reply -> drives eye/mouth shape and color.
    // Matches the mood strings used by the bridge (llm.py MOODS).
    enum Mood  { NEUTRAL, HAPPY, SAD, ANGRY, CURIOUS };

    // What the assistant is currently doing -> drives the animation.
    enum State { IDLE, LISTENING, THINKING, SPEAKING };

    explicit Face(uGUI &gui);

    // Draw the static parts once (call after the LCD is initialised).
    void begin();

    // Update mood / state. Cheap: just flags a redraw on the next tick().
    void setMood(Mood m);
    void setState(State s);

    // Map a mood string ("happy", "sad", ...) coming from the bridge.
    void setMood(const char *mood);

    // Set the reply text to show on screen. While SPEAKING, the display switches
    // from the animated face to this word-wrapped text. Copies up to capacity.
    void setText(const char *s, uint16_t len);

    // Advance the animation by one frame. Call this on a fixed cadence
    // (about every 80 ms gives a lively but smooth result).
    void tick();

    // Wipe the screen and force a full redraw on the next tick (used after the
    // menu, which draws over the whole display, is closed).
    void forceRedraw();

private:
    static constexpr int TEXT_CAP = 192;

    uGUI    &_gui;
    Mood     _mood        = NEUTRAL;
    State    _state       = IDLE;
    uint32_t _frame       = 0;
    bool     _redrawStatic = true;
    bool     _showText     = false;        // currently rendering the text screen
    int      _textFrames   = 0;            // frames left to keep showing the text
    char     _text[TEXT_CAP] = {0};

    uint32_t _moodColor() const;
    const char *_stateLabel() const;

    void _drawHead();
    void _clearFaceArea();
    void _drawEyes();
    void _drawMouth();
    void _drawStatus();
    void _drawTextScreen();
};

#endif  // EDGE_AI_FACE_H

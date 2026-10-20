#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Colours, fonts and the drawing primitives every part of the faceplate shares.
namespace ui
{
    inline const juce::Colour purple    { 0xffb18cfe };
    inline const juce::Colour purpleHi  { 0xffd9c7ff };
    inline const juce::Colour text      { 0xfff0f1f3 };
    inline const juce::Colour labelGrey { 0xff8f9299 };
    inline const juce::Colour warn      { 0xffff7a8a };   // negative correlation

    constexpr int kWidth = 802, kHeight = 192;   // size at 100 %
    constexpr int kScopeExtra = 176;             // added to the width while the goniometer is open
    constexpr float kDimAlpha = 0.45f;           // controls of a section (or effect) that is off

    juce::Font font (float size, bool bold = false, float kerning = 0.0f);
    juce::Font monoFont (float size, bool bold = false);

    // point at radius r and angle (radians, clockwise from 12 o'clock) around c
    juce::Point<float> polar (juce::Point<float> c, float r, float angle);

    // recessed dark box (step boxes, toggles, meters)
    void drawRecess (juce::Graphics&, juce::Rectangle<float>);
    void drawRecess (juce::Graphics&, juce::Point<float> centre, float size);

    // darker inset panel with a faint lavender edge (bottom strip, correlation box, goniometer)
    void drawInsetPanel (juce::Graphics&, juce::Rectangle<float>);

    void drawHoverWash (juce::Graphics&, juce::Rectangle<float>);
    void drawDropDownMark (juce::Graphics&, float x, float y, juce::Colour);

    // knob faces; pos is 0..1, hot = hovered or dragged
    void drawKnob (juce::Graphics&, juce::Point<float> centre, float pos, bool hot);
    void drawMiniKnob (juce::Graphics&, juce::Point<float> centre, float pos, bool hot);

    // round section LED
    void drawLed (juce::Graphics&, juce::Point<float> centre, bool on);

    void drawScrew (juce::Graphics&, float cx, float cy);

    // Gothic "GA", drawn with a flat pen held at an angle (own letterforms, no font needed)
    juce::Path makeGothicGA (float scale, float nib, float angleDeg);

    // small caps name under a control; (cx, cy) is the centre of the control
    void drawName (juce::Graphics&, float cx, float cy, const juce::String& name);
}

class HyperLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HyperLookAndFeel();

    // compact menus in the faceplate colours, lavender highlight
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    void drawCornerResizer (juce::Graphics&, int w, int h, bool isMouseOver, bool isMouseDragging) override;   // purple
};

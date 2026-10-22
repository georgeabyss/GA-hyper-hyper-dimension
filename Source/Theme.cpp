#include "Theme.h"

namespace hd
{
ga::Theme theme()
{
    ga::Theme t;
    auto& c = t.colours;
    c.accent      = juce::Colour (0xffb18cfe);
    c.accentHi    = juce::Colour (0xffd9c7ff);
    c.accentLight = juce::Colour (0xffeadfff);
    c.accentDeep  = juce::Colour (0xff7c55d6);
    c.locked      = juce::Colour (0xffb9a6e6);
    c.lampLit     = juce::Colour (0xffe6d8ff);
    c.lampLitEdge = juce::Colour (0xff8559e0);
    c.ledLit      = juce::Colour (0xfff3ecff);
    c.lensOff     = juce::Colour (0xff3a3148);
    c.lampOffEdge = juce::Colour (0xff251f30);
    c.ledOffEdge  = juce::Colour (0xff1e1a26);
    c.displayTop    = juce::Colour (0xff07060a);
    c.displayBottom = juce::Colour (0xff120e17);
    c.displayDotOff = juce::Colour (0xff2a2232);
    c.displayDotOn  = juce::Colour (0xfff1e9ff);
    return t;
}
}

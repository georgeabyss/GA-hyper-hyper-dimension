#pragma once

#include "UiStyle.h"

// one line for the correlation box: label, -1 .. +1 bar filled from the centre, value
class CorrelationMeter : public juce::Component
{
public:
    void setTarget (float correlation, bool hasSignal);   // no signal: glides slowly back to 0
    void paint (juce::Graphics&) override;

private:
    float shown = 0.0f;
    bool idle = true;   // no signal: drawn greyed out
};

// Lissajous / goniometer: mid up, side sideways, with persistence and auto gain
class Goniometer : public juce::Component
{
public:
    void push (const float* l, const float* r, int n);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Image trail;
    float env = 0.1f;
};

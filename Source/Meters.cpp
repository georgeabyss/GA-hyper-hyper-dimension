#include "Meters.h"

//==============================================================================
void CorrelationMeter::setTarget (float correlation, bool hasSignal)
{
    const float before = shown;
    if (hasSignal)
        shown += (juce::jlimit (-1.0f, 1.0f, correlation) - shown) * 0.3f;
    else
        shown += (0.0f - shown) * 0.035f;   // silence: back to 0 slowly (about 1.5 s)
    if (std::abs (shown) < 1.0e-3f && ! hasSignal)
        shown = 0.0f;
    if (idle == hasSignal || std::abs (shown - before) > 1.0e-4f)
        repaint();
    idle = ! hasSignal;
}

void CorrelationMeter::setAlarm (float amount)
{
    if (std::abs (amount - alarm) > 0.01f) { alarm = amount; repaint(); }
}

void CorrelationMeter::paint (juce::Graphics& g)
{
    // CORRELATION  -1 [=====|==   ] +1  +0.82
    const float cy = (float) getHeight() * 0.5f;
    g.setFont (ui::font (6.5f, false, 0.15f));
    g.setColour (ui::labelGrey.interpolatedWith (ui::alarm, alarm));   // orange while Phase Protect works
    g.drawText ("CORRELATION", juce::Rectangle<float> (4.0f, cy - 5.0f, 60.0f, 10.0f), juce::Justification::centredLeft, false);

    // the bar takes what is left between the label and the value
    const auto bar = juce::Rectangle<float> (72.0f, cy - 4.5f, (float) getWidth() - 72.0f - 52.0f, 9.0f);
    g.setFont (ui::font (6.0f));
    g.setColour (juce::Colour (0xff5d6068));
    g.drawText ("-1", juce::Rectangle<float> (bar.getX() - 12.0f, cy - 5.0f, 10.0f, 10.0f), juce::Justification::centredRight, false);
    g.drawText ("+1", juce::Rectangle<float> (bar.getRight() + 3.0f, cy - 5.0f, 12.0f, 10.0f), juce::Justification::centredLeft, false);
    ui::drawRecess (g, bar);

    const auto in = bar.reduced (1.5f);
    const float cx = in.getCentreX(), half = in.getWidth() * 0.5f;
    g.setColour (juce::Colour (0xff34373c));
    for (float t : { -0.5f, 0.0f, 0.5f })
        g.drawVerticalLine (juce::roundToInt (cx + t * half), in.getY(), in.getBottom());

    {
        const float x = cx + shown * half;
        const auto col = (shown < 0.0f ? ui::warn : ui::purple).withMultipliedAlpha (idle ? 0.5f : 1.0f);
        g.setColour (col.withMultipliedAlpha (0.45f));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (juce::jmin (cx, x), in.getY(), juce::jmax (cx, x), in.getBottom()));
        g.setColour (col);
        g.fillRect (juce::Rectangle<float> (2.0f, in.getHeight() + 2.0f).withCentre ({ x, in.getCentreY() }));
    }

    g.setFont (ui::monoFont (8.5f));
    g.setColour (idle ? juce::Colour (0xff5d6068) : juce::Colour (0xffe4e6ea));
    g.drawText ((shown >= 0.0f ? "+" : "") + juce::String (shown, 2),
                juce::Rectangle<float> (bar.getRight() + 18.0f, cy - 6.0f, 34.0f, 12.0f), juce::Justification::centredLeft, false);
}

//==============================================================================
void Goniometer::resized()
{
    // twice the resolution, so the dots stay sharp on high density screens
    trail = juce::Image (juce::Image::ARGB, juce::jmax (1, getWidth() * 2), juce::jmax (1, getHeight() * 2), true);
}

void Goniometer::push (const float* l, const float* r, int n)
{
    if (! trail.isValid())
        return;

    trail.multiplyAllAlphas (0.78f);   // persistence
    if (n > 0)
    {
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
            peak = juce::jmax (peak, std::abs (l[i] + r[i]), std::abs (l[i] - r[i]));
        peak *= 0.70710678f;
        env = juce::jmax (0.001f, peak > env ? peak : juce::jmax (peak, env * 0.97f));
        const float gain = juce::jmin (16.0f, 0.9f / env);   // auto gain: quiet signals still fill the view

        juce::Graphics tg (trail);
        tg.addTransform (juce::AffineTransform::scale (2.0f));
        const float cx = (float) getWidth() * 0.5f, cy = (float) getHeight() * 0.5f;
        const float R = (float) juce::jmin (getWidth(), getHeight()) * 0.5f - 10.0f;
        tg.setColour (ui::purpleHi.withAlpha (0.55f));
        const int stride = juce::jmax (1, n / 3000);
        for (int i = 0; i < n; i += stride)
        {
            const float s = juce::jlimit (-1.05f, 1.05f, (r[i] - l[i]) * 0.70710678f * gain);   // left signal leans left
            const float m = juce::jlimit (-1.05f, 1.05f, (l[i] + r[i]) * 0.70710678f * gain);
            tg.fillRect (cx + s * R - 0.6f, cy - m * R - 0.6f, 1.2f, 1.2f);
        }
    }
    repaint();
}

void Goniometer::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 10.0f;

    juce::Path diamond;
    diamond.startNewSubPath (c.x, c.y - R);
    diamond.lineTo (c.x + R, c.y);
    diamond.lineTo (c.x, c.y + R);
    diamond.lineTo (c.x - R, c.y);
    diamond.closeSubPath();
    g.setColour (juce::Colour (0xff2a2c31));
    g.strokePath (diamond, juce::PathStrokeType (1.0f));
    g.setColour (juce::Colour (0xff1f2125));
    g.drawLine (c.x, c.y - R, c.x, c.y + R, 1.0f);                                  // M
    g.drawLine (c.x - R, c.y, c.x + R, c.y, 1.0f);                                  // S
    g.drawLine (c.x - R * 0.5f, c.y - R * 0.5f, c.x + R * 0.5f, c.y + R * 0.5f, 1.0f);   // L
    g.drawLine (c.x + R * 0.5f, c.y - R * 0.5f, c.x - R * 0.5f, c.y + R * 0.5f, 1.0f);   // R

    g.setFont (ui::font (6.5f, true, 0.12f));
    g.setColour (ui::labelGrey);
    g.drawText ("M", juce::Rectangle<float> (12.0f, 9.0f).withCentre ({ c.x, c.y - R - 5.0f }), juce::Justification::centred, false);
    g.drawText ("L", juce::Rectangle<float> (12.0f, 9.0f).withCentre ({ c.x - R * 0.62f, c.y - R * 0.62f }), juce::Justification::centred, false);
    g.drawText ("R", juce::Rectangle<float> (12.0f, 9.0f).withCentre ({ c.x + R * 0.62f, c.y - R * 0.62f }), juce::Justification::centred, false);

    g.drawImage (trail, b, juce::RectanglePlacement::stretchToFit);
}

//==============================================================================

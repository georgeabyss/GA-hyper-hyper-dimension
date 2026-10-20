#include "UiStyle.h"

namespace ui
{
    juce::Font font (float size, bool bold, float kerning)
    {
        auto o = juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain);
        if (kerning != 0.0f) o = o.withKerningFactor (kerning);
        return juce::Font (o);
    }

    juce::Font monoFont (float size, bool bold)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size,
                                              bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Point<float> polar (juce::Point<float> c, float r, float angle)
    {
        return { c.x + std::sin (angle) * r, c.y - std::cos (angle) * r };
    }

    // recessed dark square used by the step box and the toggles
    void drawRecess (juce::Graphics& g, juce::Point<float> c, float size)
    {
        drawRecess (g, juce::Rectangle<float> (size, size).withCentre (c));
    }
    void drawRecess (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff08090a), 0.0f, r.getY(), juce::Colour (0xff131417), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 2.5f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (r, 2.5f, 1.0f);
        g.setColour (juce::Colour (0xff34373c));
        g.drawLine (r.getX() + 1.5f, r.getBottom() + 0.5f, r.getRight() - 1.5f, r.getBottom() + 0.5f, 1.0f);
    }

    // Gothic "GA", drawn with a flat pen held at an angle (own letterforms, no font needed).
    juce::Path makeGothicGA (float scale, float nib, float angleDeg)
    {
        using P = juce::Point<float>;
        using Stroke = std::vector<P>;

        const std::vector<Stroke> glyphG {
            { { 23, 8 }, { 18, 4.5f }, { 12, 5 }, { 8, 10 }, { 6.5f, 17 }, { 8, 24 }, { 13, 29.5f }, { 19, 30 }, { 24, 25.5f } },
            { { 24, 25.5f }, { 24, 17.5f } },
            { { 14.5f, 17.5f }, { 25, 17.5f } },
            { { 19, 3 }, { 25, 7 } } };
        const std::vector<Stroke> glyphA {
            { { 19, 3.5f }, { 12, 8 }, { 7.5f, 15 }, { 8, 22.5f }, { 12, 26.5f }, { 18, 24.5f } },
            { { 24, 6 }, { 24, 29 }, { 27.5f, 31 } },
            { { 19, 3.5f }, { 24, 6 } },
            { { 11, 16.5f }, { 24, 16.5f } } };

        auto catmullRom = [] (const Stroke& s, int steps)
        {
            Stroke P;
            P.push_back (s.front());
            P.insert (P.end(), s.begin(), s.end());
            P.push_back (s.back());
            Stroke out;
            for (size_t i = 1; i + 2 < P.size(); ++i)
                for (int k = 0; k < steps; ++k)
                {
                    const float t = (float) k / (float) steps, t2 = t * t, t3 = t2 * t;
                    auto f = [&] (float p0, float p1, float p2, float p3)
                    {
                        return 0.5f * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3);
                    };
                    out.push_back ({ f (P[i - 1].x, P[i].x, P[i + 1].x, P[i + 2].x),
                                     f (P[i - 1].y, P[i].y, P[i + 1].y, P[i + 2].y) });
                }
            out.push_back (s.back());
            return out;
        };

        const float a = juce::degreesToRadians (angleDeg);
        const float nx = std::cos (a) * nib * 0.5f, ny = -std::sin (a) * nib * 0.5f;

        juce::Path path;
        auto addGlyph = [&] (const std::vector<Stroke>& strokes, float dx)
        {
            for (const auto& s : strokes)
            {
                Stroke pts;
                if (s.size() > 2) pts = catmullRom (s, 10);
                else for (int i = 0; i <= 8; ++i) pts.push_back (s[0] + (s[1] - s[0]) * ((float) i / 8.0f));

                for (size_t i = 0; i + 1 < pts.size(); ++i)
                {
                    P q[4] = { { pts[i].x - nx, pts[i].y - ny }, { pts[i].x + nx, pts[i].y + ny },
                               { pts[i + 1].x + nx, pts[i + 1].y + ny }, { pts[i + 1].x - nx, pts[i + 1].y - ny } };
                    float area = 0.0f;
                    for (int k = 0; k < 4; ++k)
                        area += q[k].x * q[(k + 1) % 4].y - q[(k + 1) % 4].x * q[k].y;
                    if (area < 0.0f) { std::swap (q[0], q[3]); std::swap (q[1], q[2]); }

                    path.startNewSubPath ((q[0].x + dx) * scale, q[0].y * scale);
                    for (int k = 1; k < 4; ++k)
                        path.lineTo ((q[k].x + dx) * scale, q[k].y * scale);
                    path.closeSubPath();
                }
            }
        };
        addGlyph (glyphG, 0.0f);
        addGlyph (glyphA, 30.0f);
        return path;
    }

    void drawScrew (juce::Graphics& g, float cx, float cy)
    {
        // rack slot
        const auto slot = juce::Rectangle<float> (11.0f, 17.0f).withCentre ({ cx, cy });
        g.setColour (juce::Colour (0xff050506));
        g.fillRoundedRectangle (slot, 5.5f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (slot, 5.5f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (slot.reduced (0.5f), 5.0f, 1.0f);

        // screw head with Phillips cross
        const float r = 4.2f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff7a7d84), cx - r * 0.3f, cy - r * 0.4f,
                                                 juce::Colour (0xff1c1d20), cx + r * 0.9f, cy + r * 0.9f, true));
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colours::black);
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);
        g.setColour (juce::Colour (0xff0d0e10));
        g.drawLine (cx - 2.5f, cy, cx + 2.5f, cy, 1.2f);
        g.drawLine (cx, cy - 2.5f, cx, cy + 2.5f, 1.2f);
        g.setColour (juce::Colours::white.withAlpha (0.15f));
        g.drawLine (cx - 2.5f, cy + 0.8f, cx + 2.5f, cy + 0.8f, 0.6f);
    }
    // Knob look taken from the Knob-Tuner, radius 14. pos is 0..1.
    void drawKnob (juce::Graphics& g, juce::Point<float> c, float pos, bool hot)
    {
        constexpr float R = 14.0f, ri = R - 2.6f;
        const float startAngle = juce::degreesToRadians (225.0f), endAngle = juce::degreesToRadians (495.0f);

        // three position dots: start, centre, end
        {
            const float mid = 0.5f * (startAngle + endAngle);
            const std::pair<float, juce::Colour> dots[] = { { startAngle, juce::Colour (0xff5d6068) },
                                                             { mid,        juce::Colour (0xff9a9da6) },
                                                             { endAngle,   juce::Colour (0xff5d6068) } };
            for (auto& d : dots)
            {
                const auto p = polar (c, R + 5.5f, d.first);
                g.setColour (d.second);
                g.fillEllipse (p.x - 1.3f, p.y - 1.3f, 2.6f, 2.6f);
            }
        }

        // soft shadow
        {
            juce::Path sp;
            sp.addEllipse (c.x - R - 1.2f, c.y + 1.6f - R - 1.2f, (R + 1.2f) * 2.0f, (R + 1.2f) * 2.0f);
            juce::DropShadow (juce::Colours::black.withAlpha (0.75f), 5, { 0, 0 }).drawForPath (g, sp);
        }

        // outer ring, linear fade from top to bottom (a little lighter while hovered / dragged)
        const juce::uint8 lift = hot ? 12 : 0;
        g.setGradientFill (juce::ColourGradient (juce::Colour ((juce::uint8) (90 + lift), (juce::uint8) (90 + lift), (juce::uint8) (94 + lift)), c.x, c.y - R,
                                                 juce::Colour ((juce::uint8) (66 + lift), (juce::uint8) (66 + lift), (juce::uint8) (70 + lift)), c.x, c.y + R, false));
        g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
        g.setColour (juce::Colours::black);
        g.drawEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.drawEllipse (c.x - R + 0.7f, c.y - R + 0.7f, (R - 0.7f) * 2.0f, (R - 0.7f) * 2.0f, 1.0f);

        // darker core
        g.setGradientFill (juce::ColourGradient (juce::Colour ((juce::uint8) 42, 42, 44), c.x, c.y - ri,
                                                 juce::Colour ((juce::uint8) 35, 35, 37), c.x, c.y + ri, false));
        g.fillEllipse (c.x - ri, c.y - ri, ri * 2.0f, ri * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (c.x - ri, c.y - ri, ri * 2.0f, ri * 2.0f, 1.0f);

        // pointer from the rim inwards
        const float a = startAngle + pos * (endAngle - startAngle);
        const float r1 = R - 1.3f, r0 = r1 - 6.5f;
        juce::Path line;
        line.startNewSubPath (polar (c, r0, a));
        line.lineTo (polar (c, r1, a));
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.strokePath (line, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (0.5f, 0.7f));
        g.setColour (hot ? ui::purpleHi : ui::purple);
        g.strokePath (line, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // small dial for the bottom strip: radius 7 with a value arc around it
    void drawMiniKnob (juce::Graphics& g, juce::Point<float> c, float pos, bool hot)
    {
        constexpr float R = 7.0f, ra = R + 2.6f;
        const float startAngle = juce::degreesToRadians (225.0f), endAngle = juce::degreesToRadians (495.0f);
        const float a = startAngle + pos * (endAngle - startAngle);
        const juce::PathStrokeType arcStroke (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track, value;
        track.addCentredArc (c.x, c.y, ra, ra, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff2a2c31));
        g.strokePath (track, arcStroke);
        if (pos > 0.002f)
        {
            value.addCentredArc (c.x, c.y, ra, ra, 0.0f, startAngle, a, true);
            g.setColour (hot ? ui::purpleHi : ui::purple.withAlpha (0.85f));
            g.strokePath (value, arcStroke);
        }

        const juce::uint8 lift = hot ? 12 : 0;
        g.setGradientFill (juce::ColourGradient (juce::Colour ((juce::uint8) (90 + lift), (juce::uint8) (90 + lift), (juce::uint8) (94 + lift)), c.x, c.y - R,
                                                 juce::Colour ((juce::uint8) (56 + lift), (juce::uint8) (56 + lift), (juce::uint8) (60 + lift)), c.x, c.y + R, false));
        g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
        g.setColour (juce::Colours::black);
        g.drawEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f, 1.0f);

        juce::Path line;
        line.startNewSubPath (polar (c, 2.0f, a));
        line.lineTo (polar (c, R - 1.5f, a));
        g.setColour (hot ? ui::purpleHi : ui::purple);
        g.strokePath (line, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // darker inset panel that sets the bottom strip and the goniometer apart from the rest
    void drawInsetPanel (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0c0d0f), 0.0f, r.getY(), juce::Colour (0xff121316), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (r, 5.0f, 1.0f);
        g.setColour (ui::purple.withAlpha (0.18f));
        g.drawRoundedRectangle (r.reduced (1.0f), 4.0f, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawLine (r.getX() + 5.0f, r.getBottom() + 0.5f, r.getRight() - 5.0f, r.getBottom() + 0.5f, 1.0f);
    }


    // round LED: lit while the section is active (its amount above 0)
    void drawLed (juce::Graphics& g, juce::Point<float> c, bool on)
    {
        constexpr float r = 3.0f;
        if (on)
        {
            g.setGradientFill (juce::ColourGradient (ui::purple.withAlpha (0.45f), c.x, c.y, ui::purple.withAlpha (0.0f), c.x + 6.5f, c.y, true));
            g.fillEllipse (c.x - 6.5f, c.y - 6.5f, 13.0f, 13.0f);
        }
        juce::ColourGradient lens (on ? juce::Colour (0xfff3ecff) : juce::Colour (0xff3a3148), c.x - 0.6f, c.y - 0.9f,
                                   on ? juce::Colour (0xff7c55d6) : juce::Colour (0xff1e1a26), c.x + r, c.y + r, true);
        if (on) lens.addColour (0.55, ui::purple);
        g.setGradientFill (lens);
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour (juce::Colours::black);
        g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.0f);
    }

    void drawName (juce::Graphics& g, float cx, float cy, const juce::String& name)
    {
        g.setColour (labelGrey);
        g.setFont (font (6.8f, false, 0.15f));
        g.drawText (name.toUpperCase(), juce::Rectangle<float> (60.0f, 10.0f).withCentre ({ cx, cy + 43.0f - 3.0f }),
                    juce::Justification::centred, false);
    }

    void drawPadlock (juce::Graphics& g, float x, float y, bool open, juce::Colour colour)
    {
        juce::Path shackle;
        shackle.startNewSubPath (x + 1.25f, y + 2.8f);
        shackle.lineTo (x + 1.25f, y + 1.9f);
        shackle.addCentredArc (x + 2.5f, y + 1.9f, 1.25f, 1.25f, 0.0f, -juce::MathConstants<float>::halfPi,
                               juce::MathConstants<float>::halfPi, false);
        if (! open)
            shackle.lineTo (x + 3.75f, y + 2.8f);
        g.setColour (colour);
        g.strokePath (shackle, juce::PathStrokeType (0.8f));
        g.fillRoundedRectangle (x + 0.5f, y + 2.75f, 4.0f, 3.0f, 0.5f);
    }

    void drawHoverWash (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (juce::Colours::white.withAlpha (0.04f));
        g.fillRoundedRectangle (r, 2.5f);
    }

    void drawDropDownMark (juce::Graphics& g, float x, float y, juce::Colour colour)
    {
        juce::Path tri;
        tri.addTriangle (x - 2.4f, y - 1.2f, x + 2.4f, y - 1.2f, x, y + 1.8f);
        g.setColour (colour);
        g.fillPath (tri);
    }
}

//==============================================================================
HyperLookAndFeel::HyperLookAndFeel()
{
    setColour (juce::PopupMenu::backgroundColourId,            juce::Colour (0xff17181b));
    setColour (juce::PopupMenu::textColourId,                  juce::Colour (0xffe4e6ea));
    setColour (juce::PopupMenu::headerTextColourId,            ui::labelGrey);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::purple);
    setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colour (0xff141518));
    setColour (juce::TooltipWindow::backgroundColourId,        juce::Colour (0xff17181b));
    setColour (juce::TooltipWindow::textColourId,              ui::text);
    setColour (juce::TooltipWindow::outlineColourId,           ui::purple.withAlpha (0.4f));

    // preset dialogs (name, delete)
    setColour (juce::AlertWindow::backgroundColourId,          juce::Colour (0xff17181b));
    setColour (juce::AlertWindow::textColourId,                ui::text);
    setColour (juce::AlertWindow::outlineColourId,             ui::purple.withAlpha (0.35f));
    setColour (juce::TextButton::buttonColourId,               juce::Colour (0xff222327));
    setColour (juce::TextButton::buttonOnColourId,             ui::purple);
    setColour (juce::TextButton::textColourOffId,              ui::text);
    setColour (juce::TextEditor::backgroundColourId,           juce::Colour (0xff08090a));
    setColour (juce::TextEditor::textColourId,                 ui::text);
    setColour (juce::TextEditor::highlightColourId,            ui::purple.withAlpha (0.45f));
    setColour (juce::TextEditor::outlineColourId,              juce::Colour (0xff2a2c31));
    setColour (juce::TextEditor::focusedOutlineColourId,       ui::purple);
    setColour (juce::CaretComponent::caretColourId,            ui::purpleHi);
}

juce::Font HyperLookAndFeel::getPopupMenuFont()
{
    return ui::font (10.5f);
}

void HyperLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    const auto r = juce::Rectangle<int> (width, height).toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1a1b1e), 0.0f, 0.0f, juce::Colour (0xff111215), 0.0f, r.getBottom(), false));
    g.fillRect (r);
    g.setColour (ui::purple.withAlpha (0.35f));
    g.drawRect (r, 1.0f);
}

void HyperLookAndFeel::drawCornerResizer (juce::Graphics& g, int w, int h, bool isMouseOver, bool isMouseDragging)
{
    const float thickness = juce::jmin ((float) w, (float) h) * 0.08f;
    g.setColour (ui::purple.withAlpha (isMouseOver || isMouseDragging ? 1.0f : 0.7f));
    for (float i = 0.0f; i < 1.0f; i += 0.3f)
        g.drawLine ((float) w * i, (float) h + 1.0f, (float) w + 1.0f, (float) h * i, thickness);
}

void HyperLookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    g.setFont (ui::font (7.5f, true, 0.14f));
    g.setColour (ui::labelGrey);
    g.drawText (name.toUpperCase(), area.reduced (10, 0).withTrimmedTop (2), juce::Justification::centredLeft, true);
}


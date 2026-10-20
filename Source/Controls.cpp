#include "Controls.h"

namespace
{
    // Right-click menu for a parameter: the caller's items first (ids >= 100), then reset to
    // default and the host's own items for this parameter (automation, MIDI learn, ...).
    void showParameterMenu (juce::Component& target, juce::RangedAudioParameter& param, juce::PopupMenu items,
                            std::function<void (int)> onItem, std::function<void()> onReset)
    {
        juce::PopupMenu m;
        m.addSectionHeader (param.getName (64));
        if (items.getNumItems() > 0)
        {
            for (juce::PopupMenu::MenuItemIterator it (items); it.next();)
                m.addItem (it.getItem());
            m.addSeparator();
        }
        m.addItem (1, "Reset to default (" + param.getText (param.getDefaultValue(), 32)
                        + (param.getLabel().isNotEmpty() ? " " + param.getLabel() : juce::String()) + ")");

        std::shared_ptr<juce::HostProvidedContextMenu> hostMenu;
        if (auto* editor = target.findParentComponentOfClass<juce::AudioProcessorEditor>())
            if (auto* host = editor->getHostContext())
                hostMenu = host->getContextMenuForParameter (&param);
        if (hostMenu != nullptr)
            m.addSubMenu ("Host", hostMenu->getEquivalentPopupMenu());

        m.setLookAndFeel (&target.getLookAndFeel());   // popup menus do not inherit it by themselves
        juce::Component::SafePointer<juce::Component> safe (&target);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target).withMousePosition(),
                         [safe, hostMenu, onItem = std::move (onItem), onReset = std::move (onReset)] (int result)
                         {
                             if (safe == nullptr || result == 0) return;
                             if (result == 1) onReset();
                             else if (result >= 100 && onItem != nullptr) onItem (result);
                         });
    }

    // wheel movement as one number, positive = up / right, honouring "natural" scrolling
    float wheelAmount (const juce::MouseWheelDetails& w)
    {
        const float d = std::abs (w.deltaX) > std::abs (w.deltaY) ? -w.deltaX : w.deltaY;
        return w.isReversed ? -d : d;
    }
}

//==============================================================================
Knob::Knob (juce::RangedAudioParameter& p, const juce::String& name, bool centreDetent, Style st)
    : param (p),
      nameText (name),
      style (st),
      detentWidth (centreDetent ? 0.04f : 0.0f),
      attach (p, [this] (float v) { value = v; repaint(); })
{
    setRepaintsOnMouseActivity (true);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    setTooltip ("Drag up/down (Shift: fine). Double-click: default. Right-click: menu.");
    attach.sendInitialUpdate();
}

// Travel is the drag position. With a detent, the centre is stretched over detentWidth,
// so the value rests at the centre for a few pixels and then moves on without a jump.
float Knob::toTravel (float prop) const
{
    if (detentWidth <= 0.0f || prop < 0.5f - 1.0e-4f) return prop;
    if (prop <= 0.5f + 1.0e-4f) return 0.5f + 0.5f * detentWidth;
    return prop + detentWidth;
}

float Knob::fromTravel (float t) const
{
    if (t < 0.5f) return t;
    if (t < 0.5f + detentWidth) return 0.5f;
    return t - detentWidth;
}

juce::String Knob::valueString() const
{
    const auto label = param.getLabel();
    return param.getText (proportion(), 32) + (label.isNotEmpty() ? " " + label : juce::String());
}

void Knob::paint (juce::Graphics& g)
{
    const bool hot = isMouseOverOrDragging();
    const auto valueColour = dragging ? juce::Colours::white : juce::Colour (0xffe4e6ea);

    if (style == Style::mini)
    {
        const float cy = (float) getHeight() * 0.5f;
        ui::drawMiniKnob (g, { 12.0f, cy }, proportion(), hot);
        g.setColour (ui::labelGrey);
        g.setFont (ui::font (6.5f, false, 0.15f));
        g.drawText (nameText.toUpperCase(), juce::Rectangle<float> (26.0f, cy - 10.0f, (float) getWidth() - 26.0f, 9.0f),
                    juce::Justification::centredLeft, false);
        if (valueEditor == nullptr)
        {
            g.setColour (valueColour);
            g.setFont (ui::monoFont (8.5f));
            g.drawText (valueString(), juce::Rectangle<float> (26.0f, cy - 0.5f, (float) getWidth() - 26.0f, 11.0f),
                        juce::Justification::centredLeft, false);
        }
        return;
    }

    ui::drawKnob (g, { 28.0f, 22.0f }, proportion(), hot);
    if (valueEditor == nullptr)
    {
        g.setColour (valueColour);
        g.setFont (ui::monoFont (8.5f));
        g.drawText (valueString(), juce::Rectangle<float> (56.0f, 12.0f).withCentre ({ 28.0f, 22.0f + 31.0f - 4.0f }),
                    juce::Justification::centred, false);
    }
    ui::drawName (g, 28.0f, 22.0f, nameText);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showMenu(); return; }
    if (e.mods.isAltDown())   { resetToDefault(); return; }

    dragging = true;
    travel = toTravel (proportion());
    lastY = e.position.y;
    attach.beginGesture();
    e.source.enableUnboundedMouseMovement (true);   // hides the cursor, no screen edge
    repaint();
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    // relative movement: the knob never jumps, and pressing Shift mid-drag just slows it down
    const float dy = lastY - e.position.y;
    lastY = e.position.y;
    const bool fine = e.mods.isShiftDown() || e.mods.isCommandDown();
    travel = juce::jlimit (0.0f, 1.0f + detentWidth, travel + dy / (fine ? 1600.0f : 220.0f));
    attach.setValueAsPartOfGesture (param.convertFrom0to1 (fromTravel (travel)));
}

void Knob::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        attach.endGesture();
    dragging = false;
    repaint();
}

void Knob::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        resetToDefault();
}

void Knob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float d = wheelAmount (w);
    if (dragging || d == 0.0f)
        return;
    const float next = juce::jlimit (0.0f, 1.0f, proportion() + d * (e.mods.isShiftDown() ? 0.03f : 0.15f));
    attach.setValueAsCompleteGesture (param.convertFrom0to1 (next));
}

void Knob::resetToDefault()
{
    attach.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

void Knob::showMenu()
{
    juce::PopupMenu items;
    items.addItem (100, "Enter value...");
    juce::Component::SafePointer<Knob> safe (this);
    showParameterMenu (*this, param, items,
                       [safe] (int) { if (safe != nullptr) safe->showValueEditor(); },
                       [safe] { if (safe != nullptr) safe->resetToDefault(); });
}

void Knob::showValueEditor()
{
    valueEditor = std::make_unique<juce::TextEditor>();
    auto& ed = *valueEditor;
    ed.setJustification (juce::Justification::centred);
    ed.setFont (ui::monoFont (9.0f));
    ed.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff08090a));
    ed.setColour (juce::TextEditor::textColourId, ui::text);
    ed.setColour (juce::TextEditor::highlightColourId, ui::purple.withAlpha (0.45f));
    ed.setColour (juce::TextEditor::outlineColourId, ui::purple.withAlpha (0.6f));
    ed.setColour (juce::TextEditor::focusedOutlineColourId, ui::purple);
    ed.setColour (juce::CaretComponent::caretColourId, ui::purpleHi);
    ed.setIndents (2, 1);
    ed.setText (param.getText (proportion(), 32), false);
    if (style == Style::mini)
        ed.setBounds (24, getHeight() / 2 - 1, getWidth() - 24, 13);
    else
        ed.setBounds (1, 40, 54, 14);
    ed.setJustification (style == Style::mini ? juce::Justification::centredLeft : juce::Justification::centred);
    addAndMakeVisible (ed);
    ed.selectAll();
    ed.grabKeyboardFocus();

    ed.onReturnKey = [this]
    {
        const auto t = valueEditor->getText().trim();
        closeValueEditor();
        if (t.isNotEmpty())
            attach.setValueAsCompleteGesture (param.convertFrom0to1 (juce::jlimit (0.0f, 1.0f, param.getValueForText (t))));
    };
    ed.onEscapeKey = [this] { closeValueEditor(); };
    ed.onFocusLost = [this] { closeValueEditor(); };
    repaint();
}

void Knob::closeValueEditor()
{
    if (valueEditor == nullptr || ! valueEditor->isVisible())
        return;
    valueEditor->setVisible (false);
    // deleted later: this is called from inside the editor's own callbacks
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Knob> (this)]
    {
        if (safe != nullptr) { safe->valueEditor.reset(); safe->repaint(); }
    });
}

//==============================================================================
StepBox::StepBox (juce::RangedAudioParameter& p, bool showNames, bool showArrows)
    : param (p),
      names (showNames),
      arrows (showArrows || ! showNames),
      attach (p, [this] (float v) { value = (int) std::lround (v); repaint(); })
{
    const auto r = p.getNormalisableRange();
    minValue = (int) r.start;
    maxValue = (int) r.end;
    setMouseCursor (names ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::UpDownResizeCursor);
    setTooltip (! names ? "Drag up/down, click the arrows or use the wheel. Double-click: default. Right-click: menu."
                        : arrows ? "Click: choose. Arrows or wheel: step. Right-click: menu."
                                 : "Click: choose. Wheel: step. Right-click: menu.");
    attach.sendInitialUpdate();
}

juce::Rectangle<float> StepBox::boxRect() const
{
    if (names)
        return getLocalBounds().toFloat().reduced (2.0f, 1.0f);
    return juce::Rectangle<float> (22.0f, 22.0f).withCentre (getLocalBounds().toFloat().getCentre());
}

int StepBox::arrowAt (juce::Point<float> p) const
{
    if (! arrows)
        return 0;
    const auto r = boxRect();
    if (p.x < r.getRight() - (names ? 12.0f : 9.0f) || p.x > r.getRight() + 2.0f)
        return 0;
    return p.y < r.getCentreY() ? 1 : -1;
}

juce::String StepBox::valueText (int v) const
{
    return param.getText (param.convertTo0to1 ((float) v), 24);
}

void StepBox::step (int delta)
{
    int next = value + delta;
    if (names)   // choices wrap around
        next = next > maxValue ? minValue : (next < minValue ? maxValue : next);
    attach.setValueAsCompleteGesture ((float) juce::jlimit (minValue, maxValue, next));
}

void StepBox::paint (juce::Graphics& g)
{
    const auto r = boxRect();
    const auto c = r.getCentre();
    ui::drawRecess (g, r);
    if (isMouseOverOrDragging() && hoverArrow == 0)
    {
        ui::drawHoverWash (g, r);
    }

    g.setColour (juce::Colour (0xfff1f2f5));
    if (names)
    {
        g.setFont (ui::monoFont (7.0f, true));
        g.drawText (valueText (value).toUpperCase(),
                    r.withTrimmedRight (11.0f).withTrimmedLeft (2.0f), juce::Justification::centred, false);
    }
    else
    {
        g.setFont (ui::monoFont (11.5f, true));
        g.drawText (juce::String (value), juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ c.x - 3.0f, c.y }),
                    juce::Justification::centred, false);
    }

    const float ax = r.getRight() - 5.0f;
    const float ay = c.y;
    if (! arrows)   // drop-down mark
    {
        ui::drawDropDownMark (g, ax, ay, isMouseOver() ? ui::purpleHi : juce::Colour (0xff9a9da6));
        return;
    }
    juce::Path up, down;
    up.addTriangle (ax - 2.4f, ay - 1.2f, ax, ay - 4.0f, ax + 2.4f, ay - 1.2f);
    down.addTriangle (ax - 2.4f, ay + 1.2f, ax, ay + 4.0f, ax + 2.4f, ay + 1.2f);
    g.setColour (hoverArrow == 1 ? ui::purpleHi : juce::Colour (0xff9a9da6));
    g.fillPath (up);
    g.setColour (hoverArrow == -1 ? ui::purpleHi : juce::Colour (0xff9a9da6));
    g.fillPath (down);
}

void StepBox::mouseMove (const juce::MouseEvent& e)
{
    const int a = arrowAt (e.position);
    if (a != hoverArrow) { hoverArrow = a; repaint(); }
    setMouseCursor (a != 0 || names ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::UpDownResizeCursor);
}

void StepBox::mouseExit (const juce::MouseEvent&)
{
    hoverArrow = 0;
    repaint();
}

void StepBox::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;
    if (e.mods.isPopupMenu()) { showMenu (false); return; }

    if (const int a = arrowAt (e.position)) { step (a); return; }
    if (names) { showMenu (true); return; }   // the name box is a drop-down

    dragging = true;
    lastY = e.position.y;
    dragAccum = 0.0f;
    attach.beginGesture();
    e.source.enableUnboundedMouseMovement (true);
}

void StepBox::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;
    dragAccum += lastY - e.position.y;
    lastY = e.position.y;
    const float pxPerStep = e.mods.isShiftDown() ? 24.0f : 10.0f;
    const int steps = (int) (dragAccum / pxPerStep);
    if (steps != 0)
    {
        dragAccum -= (float) steps * pxPerStep;
        attach.setValueAsPartOfGesture ((float) juce::jlimit (minValue, maxValue, value + steps));
    }
}

void StepBox::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        attach.endGesture();
    dragging = false;
}

void StepBox::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! names && arrowAt (e.position) == 0 && ! e.mods.isPopupMenu())
        attach.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

void StepBox::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float d = wheelAmount (w);
    if (dragging || d == 0.0f)
        return;
    if (! w.isSmooth) { step (d > 0.0f ? 1 : -1); return; }   // mouse wheel: one notch, one step

    // trackpad: many tiny events, so collect them first
    wheelAccum += d;
    constexpr float perStep = 0.12f;
    while (std::abs (wheelAccum) >= perStep)
    {
        step (wheelAccum > 0.0f ? 1 : -1);
        wheelAccum -= std::copysign (perStep, wheelAccum);
    }
}

void StepBox::showMenu (bool valuesOnly)
{
    juce::PopupMenu items;
    for (int v = minValue; v <= maxValue; ++v)
        items.addItem (100 + v - minValue, valueText (v), true, v == value);

    juce::Component::SafePointer<StepBox> safe (this);
    auto onItem = [safe] (int id)
    {
        if (safe != nullptr)
            safe->attach.setValueAsCompleteGesture ((float) (safe->minValue + id - 100));
    };

    items.setLookAndFeel (&getLookAndFeel());
    if (valuesOnly)
    {
        items.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                             [onItem] (int id) { if (id >= 100) onItem (id); });
        return;
    }
    showParameterMenu (*this, param, items, onItem,
                       [safe] { if (safe != nullptr) safe->attach.setValueAsCompleteGesture (safe->param.convertFrom0to1 (safe->param.getDefaultValue())); });
}

//==============================================================================
bool LampButton::hitTest (int x, int y)
{
    return juce::Rectangle<int> (12, 12).withCentre (getLocalBounds().getCentre()).contains (x, y);
}

void LampButton::paintButton (juce::Graphics& g, bool, bool)
{
    const bool on = getToggleState();
    const auto c = getLocalBounds().toFloat().getCentre();

    // thin dark holder, then the milky lens (glow stays inside)
    const auto holder = juce::Rectangle<float> (14.4f, 14.4f).withCentre (c);
    g.setColour (juce::Colour (0xff050506));
    g.fillRoundedRectangle (holder, 1.2f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (holder, 1.2f, 1.0f);

    const auto lens = juce::Rectangle<float> (12.0f, 12.0f).withCentre (c);
    {
        const auto centre = juce::Point<float> (c.x, c.y - 0.6f);
        juce::ColourGradient grad (on ? juce::Colour (0xffe6d8ff) : juce::Colour (0xff3a3148), centre.x, centre.y,
                                   on ? juce::Colour (0xff8559e0) : juce::Colour (0xff251f30), centre.x + 7.8f, centre.y, true);
        if (on) grad.addColour (0.55, juce::Colour (0xffb18cfe));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (lens, 0.8f);
    }
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (lens, 0.8f, 1.0f);

    // two soft reflections, drawn inside the lens as vector gradients (no clipping, so they
    // stay smooth at every window size)
    {
        const float ys[2] = { c.y - 4.3f, c.y + 4.3f };
        const float as[2] = { on ? 0.60f : 0.26f, on ? 0.42f : 0.17f };
        for (int i = 0; i < 2; ++i)
        {
            const auto band = juce::Rectangle<float> (lens.getX() + 1.0f, ys[i] - 1.6f, lens.getWidth() - 2.0f, 3.2f);
            juce::ColourGradient rg (juce::Colours::white.withAlpha (as[i]), band.getCentreX(), band.getCentreY(),
                                     juce::Colours::white.withAlpha (0.0f), band.getCentreX(), band.getY(), false);
            g.setGradientFill (rg);
            g.fillRoundedRectangle (band, 1.2f);
        }
    }
}

//==============================================================================
void SquareToggle::paintButton (juce::Graphics& g, bool over, bool)
{
    const bool lit = invert ? ! getToggleState() : getToggleState();
    const auto c = getLocalBounds().toFloat().getCentre();
    const float size = (float) juce::jmin (getWidth(), getHeight()) - 2.0f;
    ui::drawRecess (g, c, size);

    if (over)
    {
        ui::drawHoverWash (g, juce::Rectangle<float> (size, size).withCentre (c));
    }

    const auto col = lit ? ui::purple : juce::Colour (0xff5d6068);

    if (lit)
    {
        const float gr = size * 0.4f;
        g.setGradientFill (juce::ColourGradient (ui::purple.withAlpha (0.28f), c.x, c.y, ui::purple.withAlpha (0.0f), c.x + gr, c.y, true));
        g.fillEllipse (c.x - gr, c.y - gr, gr * 2.0f, gr * 2.0f);
    }

    g.setColour (col);
    if (icon == Icon::scope)
    {
        // tiny goniometer: the diamond with a narrow, nearly mono trace
        constexpr float s = 4.6f;
        juce::Path d;
        d.startNewSubPath (c.x, c.y - s);
        d.lineTo (c.x + s, c.y);
        d.lineTo (c.x, c.y + s);
        d.lineTo (c.x - s, c.y);
        d.closeSubPath();
        g.strokePath (d, juce::PathStrokeType (1.0f));
        g.fillEllipse (c.x - 1.1f, c.y - 2.8f, 2.2f, 5.6f);
    }
    else
    {
        juce::Path p;
        p.addCentredArc (c.x, c.y, 5.0f, 5.0f, 0.0f, juce::degreesToRadians (57.7f), juce::degreesToRadians (302.3f), true);
        p.startNewSubPath (c.x, c.y - 6.0f);
        p.lineTo (c.x, c.y - 0.8f);
        g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

//==============================================================================
void SectionLed::paintButton (juce::Graphics& g, bool isOver, bool)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    ui::drawLed (g, c, lit);
    if (isOver)
    {
        g.setColour (ui::purpleHi.withAlpha (0.45f));
        g.drawEllipse (c.x - 4.5f, c.y - 4.5f, 9.0f, 9.0f, 1.0f);
    }
}

//==============================================================================
void MenuBox::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    ui::drawRecess (g, r);
    if (isMouseOver())
    {
        ui::drawHoverWash (g, r);
    }
    g.setFont (ui::monoFont (7.0f, true));
    g.setColour (juce::Colour (0xfff1f2f5));
    g.drawText (getText != nullptr ? getText().toUpperCase() : juce::String(), r.withTrimmedRight (9.0f),
                juce::Justification::centred, false);

    ui::drawDropDownMark (g, r.getRight() - 6.0f, r.getCentreY(), juce::Colour (0xff9a9da6));
}

void MenuBox::mouseDown (const juce::MouseEvent&)
{
    if (getItems == nullptr)
        return;
    const auto items = getItems();
    const int selected = getSelected != nullptr ? getSelected() : -1;
    juce::PopupMenu m;
    for (int i = 0; i < items.size(); ++i)
        m.addItem (1 + i, items[i], true, i == selected);

    m.setLookAndFeel (&getLookAndFeel());
    juce::Component::SafePointer<MenuBox> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()),
                     [safe] (int id)
                     {
                         if (safe != nullptr && id > 0 && safe->onSelect != nullptr)
                         {
                             safe->onSelect (id - 1);
                             safe->repaint();
                         }
                     });
}


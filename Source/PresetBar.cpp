#include "PresetBar.h"

namespace
{
    constexpr float kArrowW = 17.0f, kGap = 2.0f;

    // the LED grid of the display: square dots on a fixed pitch, text rows 2..8
    constexpr float kPitch = 1.8f, kDot = 1.3f;
    constexpr int   kCols = 109, kRows = 12, kTextRow = 2, kTextCol = 3, kSlots = 17;

    // classic 5 x 7 LED matrix font: one byte per row, bit 4 = leftmost dot
    const std::array<juce::uint8, 7>& glyph (juce::juce_wchar c)
    {
        static const std::map<juce::juce_wchar, std::array<juce::uint8, 7>> font {
            { 'A', { 0b01110, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001 } },
            { 'B', { 0b11110, 0b10001, 0b10001, 0b11110, 0b10001, 0b10001, 0b11110 } },
            { 'C', { 0b01110, 0b10001, 0b10000, 0b10000, 0b10000, 0b10001, 0b01110 } },
            { 'D', { 0b11100, 0b10010, 0b10001, 0b10001, 0b10001, 0b10010, 0b11100 } },
            { 'E', { 0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b11111 } },
            { 'F', { 0b11111, 0b10000, 0b10000, 0b11110, 0b10000, 0b10000, 0b10000 } },
            { 'G', { 0b01110, 0b10001, 0b10000, 0b10111, 0b10001, 0b10001, 0b01111 } },
            { 'H', { 0b10001, 0b10001, 0b10001, 0b11111, 0b10001, 0b10001, 0b10001 } },
            { 'I', { 0b01110, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 } },
            { 'J', { 0b00111, 0b00010, 0b00010, 0b00010, 0b00010, 0b10010, 0b01100 } },
            { 'K', { 0b10001, 0b10010, 0b10100, 0b11000, 0b10100, 0b10010, 0b10001 } },
            { 'L', { 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b10000, 0b11111 } },
            { 'M', { 0b10001, 0b11011, 0b10101, 0b10101, 0b10001, 0b10001, 0b10001 } },
            { 'N', { 0b10001, 0b10001, 0b11001, 0b10101, 0b10011, 0b10001, 0b10001 } },
            { 'O', { 0b01110, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110 } },
            { 'P', { 0b11110, 0b10001, 0b10001, 0b11110, 0b10000, 0b10000, 0b10000 } },
            { 'Q', { 0b01110, 0b10001, 0b10001, 0b10001, 0b10101, 0b10010, 0b01101 } },
            { 'R', { 0b11110, 0b10001, 0b10001, 0b11110, 0b10100, 0b10010, 0b10001 } },
            { 'S', { 0b01111, 0b10000, 0b10000, 0b01110, 0b00001, 0b00001, 0b11110 } },
            { 'T', { 0b11111, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00100 } },
            { 'U', { 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01110 } },
            { 'V', { 0b10001, 0b10001, 0b10001, 0b10001, 0b10001, 0b01010, 0b00100 } },
            { 'W', { 0b10001, 0b10001, 0b10001, 0b10101, 0b10101, 0b10101, 0b01010 } },
            { 'X', { 0b10001, 0b10001, 0b01010, 0b00100, 0b01010, 0b10001, 0b10001 } },
            { 'Y', { 0b10001, 0b10001, 0b10001, 0b01010, 0b00100, 0b00100, 0b00100 } },
            { 'Z', { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b10000, 0b11111 } },
            { '0', { 0b01110, 0b10001, 0b10011, 0b10101, 0b11001, 0b10001, 0b01110 } },
            { '1', { 0b00100, 0b01100, 0b00100, 0b00100, 0b00100, 0b00100, 0b01110 } },
            { '2', { 0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b01000, 0b11111 } },
            { '3', { 0b11111, 0b00010, 0b00100, 0b00010, 0b00001, 0b10001, 0b01110 } },
            { '4', { 0b00010, 0b00110, 0b01010, 0b10010, 0b11111, 0b00010, 0b00010 } },
            { '5', { 0b11111, 0b10000, 0b11110, 0b00001, 0b00001, 0b10001, 0b01110 } },
            { '6', { 0b00110, 0b01000, 0b10000, 0b11110, 0b10001, 0b10001, 0b01110 } },
            { '7', { 0b11111, 0b00001, 0b00010, 0b00100, 0b01000, 0b01000, 0b01000 } },
            { '8', { 0b01110, 0b10001, 0b10001, 0b01110, 0b10001, 0b10001, 0b01110 } },
            { '9', { 0b01110, 0b10001, 0b10001, 0b01111, 0b00001, 0b00010, 0b01100 } },
            { '*', { 0b00000, 0b00100, 0b10101, 0b01110, 0b10101, 0b00100, 0b00000 } },
            { '-', { 0b00000, 0b00000, 0b00000, 0b11111, 0b00000, 0b00000, 0b00000 } },
            { '!', { 0b00100, 0b00100, 0b00100, 0b00100, 0b00100, 0b00000, 0b00100 } },
            { '\'', { 0b00100, 0b00100, 0b01000, 0b00000, 0b00000, 0b00000, 0b00000 } },
            { '.', { 0b00000, 0b00000, 0b00000, 0b00000, 0b00000, 0b01100, 0b01100 } },
            { '&', { 0b01100, 0b10010, 0b10100, 0b01000, 0b10101, 0b10010, 0b01101 } },
            { '?', { 0b01110, 0b10001, 0b00001, 0b00010, 0b00100, 0b00000, 0b00100 } },
        };
        static const std::array<juce::uint8, 7> blank {};
        const auto it = font.find (c);
        return it != font.end() ? it->second : blank;
    }
}

PresetBar::PresetBar (PresetManager& manager)
    : presets (manager)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    refresh();
}

void PresetBar::refresh()
{
    const bool nowMuted = isMuted != nullptr && isMuted();
    const bool nowBlink = (juce::Time::getMillisecondCounter() / 450) % 2 == 0;
    if (nowMuted != muted || (nowMuted && nowBlink != blinkOn))
    {
        muted = nowMuted;
        blinkOn = nowBlink;
        repaint();
    }

    const int i = presets.currentIndex();
    const auto number = i >= 0 ? juce::String (i + 1).paddedLeft ('0', 2) : juce::String ("--");
    const auto newText = (number + " " + presets.currentName()).toUpperCase().substring (0, kSlots - 1);
    const bool newModified = presets.isModified();
    if (newText != text || newModified != modified)
    {
        text = newText;
        modified = newModified;
        repaint();
    }
}

juce::Rectangle<float> PresetBar::zoneRect (Zone z) const
{
    const auto b = getLocalBounds().toFloat();
    switch (z)
    {
        case Zone::prev:    return b.withWidth (kArrowW);
        case Zone::next:    return b.withTrimmedLeft (b.getWidth() - kArrowW);
        case Zone::display: return b.reduced (kArrowW + kGap, 0.0f);
        case Zone::none:    break;
    }
    return {};
}

PresetBar::Zone PresetBar::zoneAt (juce::Point<float> p) const
{
    for (auto z : { Zone::prev, Zone::display, Zone::next })
        if (zoneRect (z).contains (p))
            return z;
    return Zone::none;
}

void PresetBar::paint (juce::Graphics& g)
{
    paintArrowButton (g, Zone::prev);
    paintArrowButton (g, Zone::next);
    paintDisplay (g, zoneRect (Zone::display));
}

void PresetBar::paintArrowButton (juce::Graphics& g, Zone z)
{
    const auto r = zoneRect (z);
    ui::drawRecess (g, r);
    if (hover == z)
    {
        g.setGradientFill (juce::ColourGradient (ui::purple.withAlpha (0.25f), r.getCentreX(), r.getCentreY(),
                                                 ui::purple.withAlpha (0.0f), r.getCentreX() + r.getWidth() * 0.7f, r.getCentreY(), true));
        g.fillRect (r.reduced (1.0f));
    }
    const auto c = r.getCentre();
    const float d = z == Zone::prev ? -1.0f : 1.0f;   // pointing left or right
    juce::Path tri;
    tri.addTriangle (c.x + 2.0f * d, c.y, c.x - 2.0f * d, c.y - 3.0f, c.x - 2.0f * d, c.y + 3.0f);
    g.setColour (hover == z ? ui::purpleHi : juce::Colour (0xff9a9da6));
    g.fillPath (tri);
}

void PresetBar::paintDisplay (juce::Graphics& g, juce::Rectangle<float> r)
{
    // dark glass
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff07060a), 0.0f, r.getY(), juce::Colour (0xff120e17), 0.0f, r.getBottom(), false));
    g.fillRect (r);

    // which cells are lit: "NN NAME" from the left, the star in the last slot
    std::vector<bool> lit ((size_t) (kCols * kRows), false);
    auto put = [&lit] (juce::juce_wchar ch, int slot)
    {
        const auto& rows = glyph (ch);
        for (int row = 0; row < 7; ++row)
            for (int col = 0; col < 5; ++col)
                if ((rows[(size_t) row] >> (4 - col)) & 1)
                    lit[(size_t) ((kTextRow + row) * kCols + kTextCol + slot * 6 + col)] = true;
    };
    if (muted)
    {
        if (blinkOn)
        {
            const juce::String warning ("! OUTPUT MUTED !");
            for (int i = 0; i < warning.length(); ++i)
                put (warning[i], i);
        }
    }
    else
    {
        for (int i = 0; i < text.length() && i < kSlots - 1; ++i)
            put (text[i], i);
        if (modified)
            put ('*', kSlots - 1);
    }

    // the grid, centred in the glass: every cell is a square LED, lit or not
    const float gridW = (float) (kCols - 1) * kPitch + kDot, gridH = (float) (kRows - 1) * kPitch + kDot;
    const float x0 = r.getX() + (r.getWidth() - gridW) * 0.5f, y0 = r.getY() + (r.getHeight() - gridH) * 0.5f;
    juce::Path off, on;
    for (int row = 0; row < kRows; ++row)
        for (int col = 0; col < kCols; ++col)
            (lit[(size_t) (row * kCols + col)] ? on : off).addRectangle (x0 + (float) col * kPitch, y0 + (float) row * kPitch, kDot, kDot);

    g.setColour (juce::Colour (0xff2a2232));
    g.fillPath (off);

    // glow: the lit dots blurred outwards in two soft passes, then the dots themselves
    const auto glow = muted ? juce::Colour (0xffff6e32) : ui::purple;   // safety mute: orange-red
    for (auto pass : { std::pair<float, float> { 2.6f, 0.10f }, { 1.4f, 0.22f } })
    {
        g.setColour (glow.withAlpha (pass.second));
        g.strokePath (on, juce::PathStrokeType (pass.first, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (muted ? juce::Colour (0xffffd2b0) : juce::Colour (0xfff1e9ff));
    g.fillPath (on);

    // reflection on the glass and the frame (lavender while the list is open)
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.05f), 0.0f, r.getY(),
                                             juce::Colours::white.withAlpha (0.0f), 0.0f, r.getCentreY(), false));
    g.fillRect (r.withHeight (r.getHeight() * 0.45f));
    g.setColour (muted ? juce::Colour (0xffff7832).withAlpha (0.8f) : menuOpen ? ui::purple.withAlpha (0.7f) : juce::Colours::black);
    g.drawRect (r, 1.0f);
}

void PresetBar::mouseMove (const juce::MouseEvent& e)
{
    const auto z = zoneAt (e.position);
    if (z != hover) { hover = z; repaint(); }
}

void PresetBar::mouseExit (const juce::MouseEvent&)
{
    hover = Zone::none;
    repaint();
}

juce::String PresetBar::getTooltip()
{
    if (muted)
        return "Output muted: absurd level or broken audio was detected. Click to resume.";
    switch (hover)
    {
        case Zone::prev:    return "Previous preset";
        case Zone::next:    return "Next preset";
        case Zone::display: return "Presets: click for the list, Save, Rename and Delete";
        case Zone::none:    break;
    }
    return {};
}

void PresetBar::mouseDown (const juce::MouseEvent& e)
{
    if (muted)   // any click resumes
    {
        if (resume != nullptr)
            resume();
        return;
    }
    switch (zoneAt (e.position))
    {
        case Zone::prev:    presets.step (-1); refresh(); break;
        case Zone::next:    presets.step (+1); refresh(); break;
        case Zone::display: showMenu(); break;
        case Zone::none:    break;
    }
}

void PresetBar::showMenu()
{
    enum { saveId = 1, saveAsId, renameId, deleteId, folderId, presetBase = 1000 };

    presets.rescan();   // pick up files added in the folder meanwhile
    const auto& all = presets.all();
    const int current = presets.currentIndex();
    const bool currentIsUser = current >= 0 && ! all[(size_t) current].isFactory;

    juce::PopupMenu m;
    m.setLookAndFeel (&getLookAndFeel());
    bool userHeader = false;
    m.addSectionHeader ("Factory");
    for (size_t i = 0; i < all.size(); ++i)
    {
        if (! all[i].isFactory && ! userHeader)
        {
            m.addSectionHeader ("User");
            userHeader = true;
        }
        juce::PopupMenu::Item item (juce::String ((int) i + 1).paddedLeft ('0', 2) + "  " + all[i].name);
        item.itemID = presetBase + (int) i;
        item.isTicked = (int) i == current;
        item.shortcutKeyDescription = all[i].modeTag;   // drawn small on the right
        m.addItem (item);
    }
    m.addSeparator();
    m.addItem (saveId, currentIsUser ? "Save" : "Save...");
    m.addItem (saveAsId, "Save As...");
    m.addItem (renameId, "Rename...", currentIsUser);
    m.addItem (deleteId, "Delete", currentIsUser);
    m.addItem (folderId, "Show Presets Folder");

    menuOpen = true;
    repaint();
    juce::Component::SafePointer<PresetBar> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth ((int) zoneRect (Zone::display).getWidth()),
                     [safe, current, currentIsUser] (int id)
    {
        if (safe == nullptr)
            return;
        auto& self = *safe;
        self.menuOpen = false;
        const auto name = self.presets.currentName();
        auto saveAs = [safe, name]
        {
            safe->askForName ("Save Preset", name, [safe] (juce::String n) { if (safe != nullptr) safe->presets.save (n); });
        };

        if (id >= presetBase)
            self.presets.load (id - presetBase);
        else if (id == saveId)
        {
            if (currentIsUser) self.presets.save (name);
            else saveAs();
        }
        else if (id == saveAsId)
            saveAs();
        else if (id == renameId)
            self.askForName ("Rename Preset", name, [safe, current] (juce::String n) { if (safe != nullptr) safe->presets.rename (current, n); });
        else if (id == deleteId)
            self.confirm ("Delete Preset", "Delete \"" + name + "\"? This cannot be undone.",
                          [safe, current] { if (safe != nullptr) safe->presets.remove (current); });
        else if (id == folderId)
        {
            PresetManager::folder().createDirectory();
            PresetManager::folder().startAsProcess();
        }
        self.refresh();
        self.repaint();
    });
}

void PresetBar::askForName (const juce::String& title, const juce::String& initial, std::function<void (juce::String)> done)
{
    auto* w = new juce::AlertWindow (title, {}, juce::MessageBoxIconType::NoIcon);
    w->setLookAndFeel (&dialogLook.get());
    w->addTextEditor ("name", initial, "Name:");
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    if (auto* editor = w->getTextEditor ("name"))
        editor->selectAll();

    // the callback runs before the window is deleted, and keeps the look and feel alive until then
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, look = dialogLook, done = std::move (done), safe = juce::Component::SafePointer<PresetBar> (this)] (int result)
    {
        if (result == 1 && safe != nullptr && w->getTextEditorContents ("name").trim().isNotEmpty())
        {
            done (w->getTextEditorContents ("name").trim());
            safe->refresh();
        }
    }), true);
}

void PresetBar::confirm (const juce::String& title, const juce::String& message, std::function<void()> done)
{
    auto* w = new juce::AlertWindow (title, message, juce::MessageBoxIconType::NoIcon);
    w->setLookAndFeel (&dialogLook.get());
    w->addButton ("Delete", 1);
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([look = dialogLook, done = std::move (done), safe = juce::Component::SafePointer<PresetBar> (this)] (int result)
    {
        if (result == 1 && safe != nullptr)
        {
            done();
            safe->refresh();
        }
    }), true);
}

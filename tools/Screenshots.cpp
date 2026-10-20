// Renders the plug-in's own interface into the PNGs used by the README (docs/).
// Only built with -DHHD_SCREENSHOTS=ON, in its own build folder (see tools/make_screenshots.sh).

#include "../Source/PluginProcessor.h"
#include "../Source/ParamIDs.h"
#include "../Source/UiStyle.h"
#include <random>

namespace
{
    constexpr float kScale = 2.0f;        // pixels per point, so the images stay sharp on GitHub
    constexpr int   kCalloutH = 30;       // room below the interface for the numbered callouts

    // play a gentle, slightly stereo chord through the plug-in for a while, so the meters show something
    void play (HyperDimAudioProcessor& proc, double seconds)
    {
        constexpr double sr = 48000.0;
        constexpr int block = 512;
        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        static double phase[4] {};
        static std::mt19937 rng (7);
        std::normal_distribution<float> noise (0.0f, 0.02f);
        const double freqs[4] = { 220.0, 277.18, 329.63, 440.0 };

        for (int b = 0; b < (int) (seconds * sr / block); ++b)
        {
            for (int i = 0; i < block; ++i)
            {
                float l = 0.0f, r = 0.0f;
                for (int k = 0; k < 4; ++k)
                {
                    phase[k] += juce::MathConstants<double>::twoPi * freqs[k] / sr;
                    const float s = 0.12f * (float) std::sin (phase[k]);
                    l += s * (k % 2 == 0 ? 1.0f : 0.8f);
                    r += s * (k % 2 == 0 ? 0.8f : 1.0f);
                }
                buffer.setSample (0, i, l + noise (rng));
                buffer.setSample (1, i, r + noise (rng));
            }
            proc.processBlock (buffer, midi);
            if (b % 3 == 0)
            {
                juce::Thread::sleep (16);
                juce::Timer::callPendingTimersSynchronously();   // the meters update on the editor's timer
            }
        }
    }

    juce::Image snapshot (juce::Component& c)
    {
        return c.createComponentSnapshot (c.getLocalBounds(), true, kScale);
    }

    void save (const juce::Image& image, const juce::File& file)
    {
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat().writeImageToStream (image, out);
        std::printf ("wrote %s (%d x %d)\n", file.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());
    }

    void badge (juce::Graphics& g, juce::Point<float> c, int number)
    {
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.x - 7.5f, c.y - 6.5f, 15.0f, 15.0f);
        g.setColour (ui::purple);
        g.fillEllipse (c.x - 7.0f, c.y - 7.0f, 14.0f, 14.0f);
        g.setColour (juce::Colour (0xff141518));
        g.setFont (ui::font (8.5f, true));
        g.drawText (juce::String (number), juce::Rectangle<float> (14.0f, 14.0f).withCentre (c), juce::Justification::centred, false);
    }

    // The interface with numbered callouts. Numbers on the faceplate sit in empty spots next to what
    // they mark; the bottom strip is marked from below, with a short line up to each control.
    juce::Image annotate (const juce::Image& ui)
    {
        const int w = (int) (ui.getWidth() / kScale), h = (int) (ui.getHeight() / kScale);
        juce::Image out (juce::Image::ARGB, ui.getWidth(), (int) ((float) (h + kCalloutH) * kScale), true);
        juce::Graphics g (out);
        g.fillAll (juce::Colour (0xff0b0c0e));
        g.drawImageAt (ui, 0, 0);
        g.addTransform (juce::AffineTransform::scale (kScale));

        // on the faceplate (layout at 100 %, see MainPanel.cpp)
        const std::pair<juce::Point<float>, int> inside[] = {
            { { 338.0f, 27.0f }, 1 },    // preset browser
            { { 396.0f, 62.0f }, 2 },    // section LED
            { {  78.0f, 62.0f }, 3 },    // HYPER!
            { { 508.0f, 62.0f }, 4 },    // DIMENSION
            { { 634.0f, 62.0f }, 5 },    // SPACEY
        };
        for (auto& b : inside)
            badge (g, b.first, b.second);

        // below the bottom strip and the correlation box: x of the control, number
        const std::pair<float, int> below[] = {
            {  44.0f,  6 },   // bypass
            {  74.0f,  7 },   // dry / wet (locked in the screenshot)
            { 156.0f,  8 },   // bass mono
            { 237.0f,  9 },   // phase protect
            { 358.0f, 10 },   // oversampling
            { 422.0f, 11 },   // anti-alias filter
            { 488.0f, 12 },   // window size
            { 645.0f, 13 },   // correlation
            { 759.0f, 14 },   // goniometer
        };
        const float y = (float) h + (float) kCalloutH * 0.5f;
        for (auto& b : below)
        {
            g.setColour (ui::purple.withAlpha (0.6f));
            g.drawLine (b.first, 184.0f, b.first, y - 7.0f, 1.0f);
            badge (g, { b.first, y }, b.second);
        }
        juce::ignoreUnused (w);
        return out;
    }

    int presetIndex (const PresetManager& presets, const juce::String& name)
    {
        for (size_t i = 0; i < presets.all().size(); ++i)
            if (presets.all()[i].name == name)
                return (int) i;
        return 0;
    }
}

// called from tools/ScreenshotsMain.cpp
int hhdRenderScreenshots (const char* outputFolder)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (outputFolder));
    out.createDirectory();

    HyperDimAudioProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    proc.presets.load (presetIndex (proc.presets, "Prism Break"));
    proc.presets.setLocked (pid::dryWet, true);   // show the padlock

    {
        proc.apvts.state.setProperty (prop::scopeOpen, false, nullptr);
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        play (proc, 3.0);
        const auto image = snapshot (*editor);
        save (image, out.getChildFile ("interface.png"));
        save (annotate (image), out.getChildFile ("interface-annotated.png"));
    }
    {
        proc.apvts.state.setProperty (prop::scopeOpen, true, nullptr);
        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        play (proc, 3.0);
        save (snapshot (*editor), out.getChildFile ("goniometer.png"));
        proc.apvts.state.setProperty (prop::scopeOpen, false, nullptr);
    }
    proc.presets.setLocked (pid::dryWet, false);
    proc.releaseResources();
    return 0;
}

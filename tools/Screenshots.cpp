// Renders the plug-in's own interface into PNGs. Only built with -DHHD_SCREENSHOTS=ON, in its own
// build folder (see tools/make_screenshots.sh), never into the plug-ins.
//
//   HyperDimScreenshots [docs]          README images (with audio playing, so the meters move)
//   HyperDimScreenshots baseline <dir>  deterministic reference images: no audio, no clock
//   HyperDimScreenshots check <dir> [--pixel-tolerance N]
//                                       compares with the baseline: no pixel may differ by more than N in
//                                       any channel (default 0); differences go to build-tools/screenshot-diff/

#include "../Source/PluginProcessor.h"
#include "../Source/ParamIDs.h"
#include "../Source/Theme.h"
#include "ui/Style.h"
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
        g.setColour (hd::theme().colours.accent);
        g.fillEllipse (c.x - 7.0f, c.y - 7.0f, 14.0f, 14.0f);
        g.setColour (hd::theme().colours.onAccent);
        g.setFont (ga::ui::font (8.5f, true));
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
            { 271.0f,  9 },   // oversampling
            { 335.0f, 10 },   // anti-alias filter
            { 401.0f, 11 },   // window size
            { 560.0f, 12 },   // correlation
            { 703.0f, 13 },   // phase protect
            { 759.0f, 14 },   // goniometer
        };
        const float y = (float) h + (float) kCalloutH * 0.5f;
        for (auto& b : below)
        {
            g.setColour (hd::theme().colours.accent.withAlpha (0.6f));
            g.drawLine (b.first, 184.0f, b.first, y - 7.0f, 1.0f);
            badge (g, { b.first, y }, b.second);
        }
        juce::ignoreUnused (w);
        return out;
    }

    int presetIndex (const ga::PresetManager& presets, const juce::String& name)
    {
        for (size_t i = 0; i < presets.all().size(); ++i)
            if (presets.all()[i].name == name)
                return (int) i;
        return 0;
    }
}

//==============================================================================
// Baseline: the same images on every run. Nothing plays, and the editor is only settled through
// its timer, so the meters stay idle and nothing depends on the time of day.
namespace
{
    struct BaselineShot
    {
        juce::String name;
        float scale;
        bool scopeOpen;
        std::function<void (HyperDimAudioProcessor&)> setup;
    };

    std::vector<BaselineShot> baselineShots()
    {
        std::vector<BaselineShot> shots;
        for (bool open : { false, true })
            for (float scale : { 0.75f, 1.0f, 1.5f, 2.0f })
                shots.push_back ({ juce::String (open ? "scope-open" : "main") + "_" + juce::String (juce::roundToInt (scale * 100)),
                                   scale, open, nullptr });
        shots.push_back ({ "state_bypassed_100", 1.0f, false, [] (HyperDimAudioProcessor& p)
        {
            p.apvts.getParameter (pid::bypass)->setValueNotifyingHost (1.0f);
        } });
        shots.push_back ({ "state_dimension-off_drywet-locked_100", 1.0f, false, [] (HyperDimAudioProcessor& p)
        {
            p.apvts.getParameter (pid::dimOn)->setValueNotifyingHost (0.0f);
            p.presets.setLocked (pid::dryWet, true);
        } });
        return shots;
    }

    juce::Image renderBaseline (const BaselineShot& shot)
    {
        HyperDimAudioProcessor proc;
        proc.setPlayConfigDetails (2, 2, 48000.0, 512);
        proc.prepareToPlay (48000.0, 512);
        proc.presets.load (presetIndex (proc.presets, "Init"));
        proc.apvts.state.setProperty (prop::scopeOpen, shot.scopeOpen, nullptr);
        proc.apvts.state.setProperty (prop::uiScale, shot.scale, nullptr);
        if (shot.setup != nullptr)
            shot.setup (proc);

        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        for (int i = 0; i < 4; ++i)   // let the editor's timer apply the state once
        {
            juce::Thread::sleep (20);
            juce::Timer::callPendingTimersSynchronously();
        }
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        editor.reset();
        proc.releaseResources();
        return image;
    }

    // number of pixels whose largest channel difference is above the tolerance; largest = the
    // largest channel difference anywhere (what a tolerance would have to cover)
    int countDifferences (const juce::Image& a, const juce::Image& b, int tolerance, juce::Image& diff, int& largest)
    {
        diff = juce::Image (juce::Image::ARGB, a.getWidth(), a.getHeight(), true);
        int count = 0;
        largest = 0;
        for (int y = 0; y < a.getHeight(); ++y)
            for (int x = 0; x < a.getWidth(); ++x)
            {
                const auto p = a.getPixelAt (x, y), q = b.getPixelAt (x, y);
                const int d = juce::jmax (std::abs (p.getRed() - q.getRed()), std::abs (p.getGreen() - q.getGreen()),
                                          std::abs (p.getBlue() - q.getBlue()), std::abs (p.getAlpha() - q.getAlpha()));
                largest = juce::jmax (largest, d);
                const bool different = d > tolerance;
                count += different ? 1 : 0;
                diff.setPixelAt (x, y, different ? juce::Colours::red : p.withAlpha (0.25f));
            }
        return count;
    }

    int baseline (bool write, const juce::File& dir, int tolerance)
    {
        const auto diffDir = juce::File::getCurrentWorkingDirectory().getChildFile ("build-tools/screenshot-diff");
        int failures = 0;
        for (auto& shot : baselineShots())
        {
            const auto rendered = renderBaseline (shot);
            const auto file = dir.getChildFile (shot.name + ".png");
            if (write)
            {
                dir.createDirectory();
                save (rendered, file);
                continue;
            }
            const auto ref = juce::ImageFileFormat::loadFrom (file);
            // compare after the same PNG round trip the reference went through (semi-transparent
            // edge pixels are rounded on the way), so only real differences count
            juce::MemoryOutputStream png;
            juce::PNGImageFormat().writeImageToStream (rendered, png);
            const auto image = juce::ImageFileFormat::loadFrom (png.getData(), png.getDataSize());
            if (! ref.isValid() || ref.getWidth() != image.getWidth() || ref.getHeight() != image.getHeight())
            {
                std::printf ("FAIL  %-40s reference missing or of a different size\n", shot.name.toRawUTF8());
                ++failures;
                continue;
            }
            juce::Image diff;
            int largest = 0;
            const int count = countDifferences (ref, image, tolerance, diff, largest);
            const bool ok = count == 0;
            const auto detail = largest == 0 ? juce::String ("identical")
                              : ok ? "within tolerance (largest channel difference " + juce::String (largest) + ")"
                                   : juce::String (count) + " pixels above tolerance " + juce::String (tolerance)
                                     + " (largest channel difference " + juce::String (largest) + ")";
            std::printf ("%s  %-40s %s\n", ok ? "ok  " : "FAIL", shot.name.toRawUTF8(), detail.toRawUTF8());
            if (count > 0)
            {
                diffDir.createDirectory();
                save (image, diffDir.getChildFile (shot.name + "_new.png"));
                save (diff, diffDir.getChildFile (shot.name + "_diff.png"));
            }
            failures += ok ? 0 : 1;
        }
        std::printf ("\n%s\n", failures == 0 ? (write ? "Baseline written." : "All screenshots match.")
                                             : "Screenshots differ: see build-tools/screenshot-diff/");
        return failures == 0 ? 0 : 1;
    }
}

// called from tools/ScreenshotsMain.cpp
int hhdRenderScreenshots (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String mode (argc > 1 ? argv[1] : "docs");
    if (mode == "baseline" || mode == "check")
    {
        int tolerance = 0;
        for (int i = 3; i + 1 < argc; ++i)
            if (juce::String (argv[i]) == "--pixel-tolerance")
                tolerance = juce::String (argv[i + 1]).getIntValue();
        const juce::File dir (juce::File::getCurrentWorkingDirectory().getChildFile (argc > 2 ? argv[2] : "tests/reference/screenshots"));
        return baseline (mode == "baseline", dir, tolerance);
    }

    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (mode));
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

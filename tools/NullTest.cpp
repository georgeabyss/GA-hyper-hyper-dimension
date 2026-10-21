// Null test (refactor safety net): renders fixed, generated test audio through the processor and
// compares the result with the reference files in tests/reference/. Only built with
// -DHHD_NULLTEST=ON (see tools/null_test.sh), never into the plug-ins.
//
//   HyperDimNullTest write <referenceDir>                       writes the references
//   HyperDimNullTest check <referenceDir> [--tolerance-db <dB> [--tolerance-match <text>]]
//                                             compares; by default any difference fails. With --tolerance-match,
//                                             the tolerance only applies to renders whose name contains <text>.
//
// Checks: 3 presets x 7 oversampling settings, variable block sizes, 96 kHz, automation of Bypass and
// Dry/Wet, the safety mute, a saved session that must load with identical values, and the latency of
// every oversampling setting.

#include "../Source/PluginProcessor.h"
#include "../Source/ParamIDs.h"

namespace
{
    constexpr double kSeconds = 6.0;
    constexpr int kChannels = 2;

    //==========================================================================
    // the same numbers on every platform and compiler (std:: distributions differ between them)
    struct Rng
    {
        juce::uint32 s;
        explicit Rng (juce::uint32 seed) : s (seed != 0 ? seed : 1) {}
        juce::uint32 next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        float bipolar() { return (float) (next() >> 8) * (2.0f / 16777216.0f) - 1.0f; }
        int range (int lo, int hi) { return lo + (int) (next() % (juce::uint32) (hi - lo + 1)); }
    };

    struct TestSignal
    {
        juce::AudioBuffer<float> audio;
        std::vector<int> noteOns;   // sample positions: every kick also sends a MIDI note (Retrig)
    };

    // 0.0-0.5 s silence | 0.5-1.7 noise bursts | 1.7-2.7 kicks | 2.7-5.0 sine sweep | 5.0-6.0 silence (tails)
    TestSignal makeSignal (double sr)
    {
        TestSignal t;
        const int n = (int) (kSeconds * sr);
        t.audio.setSize (kChannels, n);
        t.audio.clear();
        auto* l = t.audio.getWritePointer (0);
        auto* r = t.audio.getWritePointer (1);
        auto at = [sr] (double seconds) { return (int) (seconds * sr); };

        // noise bursts, partly decorrelated between the channels, with raised-cosine fades
        Rng rng (12345);
        float lpA = 0.0f, lpB = 0.0f;
        for (double start : { 0.5, 0.9, 1.3 })
        {
            const int s0 = at (start), len = at (0.3), fade = at (0.02);
            for (int i = 0; i < len; ++i)
            {
                lpA += 0.3f * (rng.bipolar() - lpA);
                lpB += 0.3f * (rng.bipolar() - lpB);
                const float env = i < fade ? 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (float) i / (float) fade)
                                : i > len - fade ? 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * (float) (len - i) / (float) fade)
                                : 1.0f;
                l[s0 + i] = 0.6f * env * lpA;
                r[s0 + i] = 0.6f * env * (0.7f * lpA + 0.3f * lpB);
            }
        }

        // kicks: a pitch-dropping sine with a short click
        for (double start : { 1.8, 2.05, 2.3, 2.55 })
        {
            const int s0 = at (start);
            t.noteOns.push_back (s0);
            double phase = 0.0;
            for (int i = 0; i < at (0.24); ++i)
            {
                const double time = (double) i / sr;
                phase += juce::MathConstants<double>::twoPi * (45.0 + 75.0 * std::exp (-time / 0.03)) / sr;
                float v = 0.8f * (float) (std::exp (-time / 0.08) * std::sin (phase));
                if (time < 0.002)
                    v += 0.3f * rng.bipolar();
                l[s0 + i] += v;
                r[s0 + i] += v;
            }
        }

        // logarithmic sine sweep, 30 Hz to 16 kHz, right channel a little quieter
        {
            const int s0 = at (2.7), len = at (2.3), fade = at (0.01);
            const double f0 = 30.0, f1 = 16000.0, k = std::log (f1 / f0);
            double phase = 0.0;
            for (int i = 0; i < len; ++i)
            {
                const double x = (double) i / (double) len;
                phase += juce::MathConstants<double>::twoPi * f0 * std::exp (k * x) / sr;
                const float env = i < fade ? (float) i / (float) fade : i > len - fade ? (float) (len - i) / (float) fade : 1.0f;
                const float v = 0.25f * env * (float) std::sin (phase);
                l[s0 + i] += v;
                r[s0 + i] += 0.9f * v;
            }
        }
        return t;
    }

    //==========================================================================
    // 32-bit float WAV, written and read here so the test build needs no extra JUCE module
    bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& audio, double sr)
    {
        juce::MemoryOutputStream m;
        const int n = audio.getNumSamples(), ch = audio.getNumChannels();
        const int dataBytes = n * ch * 4;
        m.write ("RIFF", 4); m.writeInt (36 + dataBytes); m.write ("WAVE", 4);
        m.write ("fmt ", 4); m.writeInt (16); m.writeShort (3); m.writeShort ((short) ch);   // 3 = IEEE float
        m.writeInt ((int) sr); m.writeInt ((int) sr * ch * 4); m.writeShort ((short) (ch * 4)); m.writeShort (32);
        m.write ("data", 4); m.writeInt (dataBytes);
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < ch; ++c)
                m.writeFloat (audio.getSample (c, i));
        file.getParentDirectory().createDirectory();
        return file.replaceWithData (m.getData(), m.getDataSize());
    }

    bool readWav (const juce::File& file, juce::AudioBuffer<float>& audio)
    {
        juce::MemoryBlock data;
        if (! file.loadFileAsData (data) || data.getSize() < 44)
            return false;
        juce::MemoryInputStream in (data, false);
        char id[5] {};
        in.read (id, 4); in.readInt(); in.read (id, 4);
        int channels = 0;
        while (! in.isExhausted())
        {
            in.read (id, 4);
            const int size = in.readInt();
            const auto next = in.getPosition() + size;
            if (juce::String (id) == "fmt ")
            {
                in.readShort();
                channels = in.readShort();
            }
            else if (juce::String (id) == "data" && channels > 0)
            {
                const int n = size / (channels * 4);
                audio.setSize (channels, n);
                for (int i = 0; i < n; ++i)
                    for (int c = 0; c < channels; ++c)
                        audio.setSample (c, i, in.readFloat());
                return true;
            }
            in.setPosition (next);
        }
        return false;
    }

    //==========================================================================
    void setParam (HyperDimAudioProcessor& p, const char* id, float value)
    {
        auto* param = p.apvts.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    int presetIndex (const PresetManager& presets, const juce::String& name)
    {
        for (size_t i = 0; i < presets.all().size(); ++i)
            if (presets.all()[i].isFactory && presets.all()[i].name == name)
                return (int) i;
        jassertfalse;
        return 0;
    }

    struct OsSetting { const char* name; int stages, filter; };
    const OsSetting kOsSettings[] = { { "off", 0, 0 },
                                      { "2x-linear", 1, 0 }, { "2x-lowlatency", 1, 1 },
                                      { "4x-linear", 2, 0 }, { "4x-lowlatency", 2, 1 },
                                      { "8x-linear", 3, 0 }, { "8x-lowlatency", 3, 1 } };

    struct Render
    {
        juce::String name, preset;
        OsSetting os = kOsSettings[1];
        double sr = 44100.0;
        bool variableBlocks = false, safetyBurst = false;
        std::function<void (HyperDimAudioProcessor&, double seconds)> automation;
    };

    struct Result
    {
        juce::AudioBuffer<float> audio;
        double sr = 44100.0;
        bool mutedAtEnd = false;
        int firstMutedSample = -1;   // safety render: where the muted block starts
    };

    Result render (const Render& spec)
    {
        constexpr int kBlock = 512, kMaxBlock = 1024;
        auto signal = makeSignal (spec.sr);
        const int burstAt = (int) (3.0 * spec.sr);
        if (spec.safetyBurst)   // +26 dB for 64 samples
            for (int c = 0; c < kChannels; ++c)
                for (int i = 0; i < 64; ++i)
                    signal.audio.setSample (c, burstAt + i, 20.0f);

        HyperDimAudioProcessor proc;
        proc.setPlayConfigDetails (kChannels, kChannels, spec.sr, kMaxBlock);
        setParam (proc, pid::osFactor, (float) spec.os.stages);
        setParam (proc, pid::osFilter, (float) spec.os.filter);
        proc.presets.load (presetIndex (proc.presets, spec.preset));
        proc.prepareToPlay (spec.sr, kMaxBlock);

        Result res;
        res.sr = spec.sr;
        res.audio.makeCopyOf (signal.audio);
        Rng blocks (777);
        const int total = res.audio.getNumSamples();
        for (int pos = 0; pos < total;)
        {
            const int n = juce::jmin (spec.variableBlocks ? blocks.range (1, kMaxBlock) : kBlock, total - pos);
            juce::AudioBuffer<float> block (res.audio.getArrayOfWritePointers(), kChannels, pos, n);
            juce::MidiBuffer midi;
            for (int s : signal.noteOns)
                if (s >= pos && s < pos + n)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), s - pos);
            if (spec.automation != nullptr)
                spec.automation (proc, (double) pos / spec.sr);
            proc.processBlock (block, midi);
            if (spec.safetyBurst && res.firstMutedSample < 0 && proc.isSafetyMuted())
                res.firstMutedSample = pos;
            pos += n;
        }
        res.mutedAtEnd = proc.isSafetyMuted();
        proc.releaseResources();
        return res;
    }

    std::vector<Render> allRenders()
    {
        std::vector<Render> r;
        for (const char* preset : { "Hairdryer", "Space Cadet", "Zap Zap Zap" })
            for (auto& os : kOsSettings)
            {
                Render x;
                x.name = juce::String (preset).toLowerCase().replace (" ", "-") + "_" + os.name;
                x.preset = preset;
                x.os = os;
                r.push_back (x);
            }

        Render varBlocks;
        varBlocks.name = "variable-blocks_space-cadet_2x-linear";
        varBlocks.preset = "Space Cadet";
        varBlocks.variableBlocks = true;
        r.push_back (varBlocks);

        Render hiRate;
        hiRate.name = "96khz_zap-zap-zap_2x-linear";
        hiRate.preset = "Zap Zap Zap";
        hiRate.sr = 96000.0;
        r.push_back (hiRate);

        // Bypass on 1.0-2.0 s and 3.5-4.0 s; Dry/Wet swept 100 -> 0 -> 100 % from 2.5 to 4.5 s
        Render automation;
        automation.name = "automation_init_2x-linear";
        automation.preset = "Init";
        automation.automation = [] (HyperDimAudioProcessor& p, double t)
        {
            const bool bypassed = (t >= 1.0 && t < 2.0) || (t >= 3.5 && t < 4.0);
            setParam (p, pid::bypass, bypassed ? 1.0f : 0.0f);
            const double x = juce::jlimit (0.0, 1.0, (t - 2.5) / 2.0);
            setParam (p, pid::dryWet, (float) (100.0 * std::abs (1.0 - 2.0 * x)));
        };
        r.push_back (automation);

        Render safety;
        safety.name = "safety-mute_hairdryer_2x-linear";
        safety.preset = "Hairdryer";
        safety.safetyBurst = true;
        r.push_back (safety);
        return r;
    }

    //==========================================================================
    // session state: every parameter at a seeded non-default value, plus the editor settings
    std::vector<std::pair<juce::String, juce::String>> sessionProperties()
    {
        return { { prop::lockDryWet, "1" }, { prop::lockBassMono, "1" }, { prop::preset, "Seasick" },
                 { prop::presetIsUser, "0" }, { prop::scopeOpen, "1" }, { prop::uiScale, "1.25" } };
    }

    juce::String formatValue (float v) { return juce::String::formatted ("%.9g", (double) v); }

    void writeSession (const juce::File& dir)
    {
        HyperDimAudioProcessor proc;
        Rng rng (2024);
        juce::StringArray expected;
        for (auto* param : proc.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            {
                // a value the parameter can really hold: switches flipped against their default,
                // everything else random, snapped to the parameter's steps, and never the default
                auto legal = [ranged] (float v) { return ranged->convertTo0to1 (ranged->convertFrom0to1 (v)); };
                float v;
                if (dynamic_cast<juce::AudioParameterBool*> (ranged) != nullptr)
                    v = ranged->getDefaultValue() > 0.5f ? 0.0f : 1.0f;
                else
                {
                    const float r = (rng.bipolar() + 1.0f) * 0.5f;
                    v = legal (r);
                    if (std::abs (v - ranged->getDefaultValue()) < 1.0e-6f)
                        v = legal (1.0f - r);
                }
                ranged->setValueNotifyingHost (v);
                expected.add (ranged->getParameterID() + " " + formatValue (ranged->getValue()));
            }
        for (auto& [key, value] : sessionProperties())
        {
            proc.apvts.state.setProperty (juce::Identifier (key), value, nullptr);
            expected.add ("@" + key + " " + value);
        }
        juce::MemoryBlock state;
        proc.getStateInformation (state);
        dir.getChildFile ("session-0.9.0.state").replaceWithData (state.getData(), state.getSize());
        dir.getChildFile ("session-0.9.0.txt").replaceWithText (expected.joinIntoString ("\n") + "\n");
    }

    int checkSession (const juce::File& dir)
    {
        juce::MemoryBlock state;
        juce::StringArray expected;
        if (! dir.getChildFile ("session-0.9.0.state").loadFileAsData (state))
        {
            std::printf ("FAIL  session: reference missing\n");
            return 1;
        }
        expected.addLines (dir.getChildFile ("session-0.9.0.txt").loadFileAsString());
        expected.removeEmptyStrings();

        HyperDimAudioProcessor proc;
        proc.setStateInformation (state.getData(), (int) state.getSize());
        int wrong = 0;
        for (auto& line : expected)
        {
            const auto key = line.upToFirstOccurrenceOf (" ", false, false);
            const auto want = line.fromFirstOccurrenceOf (" ", false, false);
            juce::String got;
            if (key.startsWith ("@"))
                got = proc.apvts.state.getProperty (juce::Identifier (key.substring (1))).toString();
            else if (auto* p = proc.apvts.getParameter (key))
                got = formatValue (p->getValue());
            if (got != want)
            {
                std::printf ("      %s: expected %s, got %s\n", key.toRawUTF8(), want.toRawUTF8(), got.toRawUTF8());
                ++wrong;
            }
        }
        std::printf ("%s  session-0.9.0: %d values loaded, %d different\n", wrong == 0 ? "ok  " : "FAIL", expected.size(), wrong);
        return wrong == 0 ? 0 : 1;
    }

    //==========================================================================
    juce::StringArray latencies()
    {
        juce::StringArray lines;
        for (auto& os : kOsSettings)
        {
            HyperDimAudioProcessor proc;
            proc.setPlayConfigDetails (kChannels, kChannels, 44100.0, 512);
            setParam (proc, pid::osFactor, (float) os.stages);
            setParam (proc, pid::osFilter, (float) os.filter);
            proc.prepareToPlay (44100.0, 512);
            lines.add (juce::String (os.name) + " " + juce::String (proc.getLatencySamples()));
            proc.releaseResources();
        }
        return lines;
    }

    juce::String describeDiff (double maxDiff)
    {
        return maxDiff == 0.0 ? juce::String ("identical") : juce::String (20.0 * std::log10 (maxDiff), 1) + " dB";
    }
}

int hhdNullTest (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 3 || (juce::String (argv[1]) != "write" && juce::String (argv[1]) != "check"))
    {
        std::printf ("usage: HyperDimNullTest write|check <referenceDir> [--tolerance-db <dB> [--tolerance-match <text>]]\n");
        return 2;
    }
    const bool write = juce::String (argv[1]) == "write";
    const juce::File dir = juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]);
    const juce::File audioDir = dir.getChildFile ("audio");
    double toleranceDb = -1000.0;   // default: any difference fails
    juce::String toleranceMatch;    // empty: the tolerance applies to every render
    for (int i = 3; i + 1 < argc; ++i)
    {
        if (juce::String (argv[i]) == "--tolerance-db")    toleranceDb = juce::String (argv[i + 1]).getDoubleValue();
        if (juce::String (argv[i]) == "--tolerance-match") toleranceMatch = argv[i + 1];
    }
    const juce::File outDir = juce::File::getCurrentWorkingDirectory().getChildFile ("build-tools/nulltest-out");

    int failures = 0;
    std::printf ("%s %s\n", write ? "Writing references to" : "Checking against", dir.getFullPathName().toRawUTF8());
    if (! write)
        std::printf (toleranceDb <= -1000.0 ? "Every render must be identical.\n"
                     : toleranceMatch.isEmpty() ? "Tolerance %.1f dB for every render.\n"
                     : "Tolerance %.1f dB for renders containing '%s'; all others must be identical.\n",
                     toleranceDb, toleranceMatch.toRawUTF8());
    std::printf ("\n");

    for (auto& spec : allRenders())
    {
        const auto res = render (spec);
        const auto file = audioDir.getChildFile (spec.name + ".wav");

        if (spec.safetyBurst)   // muted from the burst's block on, exactly silent, and stays muted
        {
            const int burstAt = (int) (3.0 * spec.sr);
            bool silent = res.firstMutedSample >= 0 && res.firstMutedSample <= burstAt;
            for (int c = 0; silent && c < kChannels; ++c)
                for (int i = juce::jmax (0, res.firstMutedSample); silent && i < res.audio.getNumSamples(); ++i)
                    silent = res.audio.getSample (c, i) == 0.0f;
            const bool ok = silent && res.mutedAtEnd;
            std::printf ("%s  safety mute: engaged at %.3f s, silent afterwards: %s, still muted at the end: %s\n",
                         ok ? "ok  " : "FAIL", res.firstMutedSample / spec.sr, silent ? "yes" : "no", res.mutedAtEnd ? "yes" : "no");
            failures += ok ? 0 : 1;
        }

        if (write)
        {
            writeWav (file, res.audio, res.sr);
            std::printf ("wrote %s\n", file.getFileName().toRawUTF8());
            continue;
        }

        juce::AudioBuffer<float> ref;
        if (! readWav (file, ref) || ref.getNumSamples() != res.audio.getNumSamples() || ref.getNumChannels() != res.audio.getNumChannels())
        {
            std::printf ("FAIL  %-40s reference missing or of a different length\n", spec.name.toRawUTF8());
            ++failures;
            continue;
        }
        double maxDiff = 0.0;
        for (int c = 0; c < ref.getNumChannels(); ++c)
            for (int i = 0; i < ref.getNumSamples(); ++i)
                maxDiff = juce::jmax (maxDiff, (double) std::abs (ref.getSample (c, i) - res.audio.getSample (c, i)));
        const double tolerance = toleranceMatch.isEmpty() || spec.name.contains (toleranceMatch) ? toleranceDb : -1000.0;
        const bool ok = maxDiff == 0.0 || 20.0 * std::log10 (maxDiff) <= tolerance;
        std::printf ("%s  %-40s %s\n", ok ? "ok  " : "FAIL", spec.name.toRawUTF8(), describeDiff (maxDiff).toRawUTF8());
        if (! ok)
        {
            ++failures;
            writeWav (outDir.getChildFile (spec.name + ".wav"), res.audio, res.sr);
        }
    }

    // latency of every oversampling setting
    const auto lat = latencies();
    std::printf ("\nLatency at 44.1 kHz (samples):\n");
    for (auto& l : lat)
        std::printf ("      %s\n", l.toRawUTF8());
    if (write)
        dir.getChildFile ("latency.txt").replaceWithText (lat.joinIntoString ("\n") + "\n");
    else
    {
        juce::StringArray want;
        want.addLines (dir.getChildFile ("latency.txt").loadFileAsString());
        want.removeEmptyStrings();
        const bool ok = want == lat;
        std::printf ("%s  latency unchanged\n", ok ? "ok  " : "FAIL");
        failures += ok ? 0 : 1;
    }

    // saved session
    std::printf ("\n");
    if (write)
    {
        writeSession (dir);
        std::printf ("wrote session-0.9.0.state and session-0.9.0.txt\n");
    }
    else
        failures += checkSession (dir);

    std::printf ("\n%s\n", failures == 0 ? (write ? "References written." : "All checks passed.")
                                         : (juce::String (failures) + " check(s) failed. Failing renders: build-tools/nulltest-out/").toRawUTF8());
    return failures == 0 ? 0 : 1;
}

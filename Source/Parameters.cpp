#include "Parameters.h"

namespace et
{
const std::array<SyncDivision, kNumDivisions>& syncDivisions()
{
    static const std::array<SyncDivision, kNumDivisions> divs {{
        { "1/32",   0.125 },
        { "1/16 T", 1.0 / 6.0 },
        { "1/16",   0.25 },
        { "1/8 T",  1.0 / 3.0 },
        { "1/16 D", 0.375 },
        { "1/8",    0.5 },
        { "1/4 T",  2.0 / 3.0 },
        { "1/8 D",  0.75 },
        { "1/4",    1.0 },
        { "1/2 T",  4.0 / 3.0 },
        { "1/4 D",  1.5 },
        { "1/2",    2.0 },
        { "1/2 D",  3.0 },
        { "1 BAR",  4.0 }
    }};
    return divs;
}

double divisionSeconds (int index, double bpm)
{
    index = juce::jlimit (0, kNumDivisions - 1, index);
    bpm = juce::jlimit (20.0, 400.0, bpm);
    return syncDivisions()[(size_t) index].beats * 60.0 / bpm;
}

//==============================================================================
juce::String formatHz (float hz)
{
    if (hz >= 10000.0f) return juce::String (hz / 1000.0f, 1) + " kHz";
    if (hz >= 1000.0f)  return juce::String (hz / 1000.0f, 2) + " kHz";
    if (hz >= 100.0f)   return juce::String (juce::roundToInt (hz)) + " Hz";
    return juce::String (hz, 1) + " Hz";
}

juce::String formatMs (float ms)
{
    if (ms >= 1000.0f) return juce::String (ms / 1000.0f, 2) + " s";
    if (ms >= 100.0f)  return juce::String (juce::roundToInt (ms)) + " ms";
    return juce::String (ms, 1) + " ms";
}

juce::String formatDb (float db)
{
    if (std::abs (db) < 0.05f) return "0.0 dB";
    return (db > 0 ? "+" : "") + juce::String (db, 1) + " dB";
}

juce::String formatPercent (float n)
{
    return juce::String (juce::roundToInt (n * 100.0f)) + "%";
}

//==============================================================================
namespace
{
    using Attr = juce::AudioParameterFloatAttributes;
    using FloatParam = juce::AudioParameterFloat;

    constexpr int kVersion = 1;

    juce::NormalisableRange<float> skewed (float lo, float hi, float centre, float step = 0.0f)
    {
        juce::NormalisableRange<float> r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    }

    std::unique_ptr<FloatParam> makeFloat (const char* id, const char* name,
                                           juce::NormalisableRange<float> range, float def,
                                           std::function<juce::String (float, int)> text)
    {
        return std::make_unique<FloatParam> (juce::ParameterID { id, kVersion }, name, range, def,
                                             Attr().withStringFromValueFunction (std::move (text)));
    }

    std::unique_ptr<FloatParam> makeFreq (const char* id, const char* name, float lo, float hi, float def)
    {
        return makeFloat (id, name, skewed (lo, hi, std::sqrt (lo * hi)), def,
                          [] (float v, int) { return formatHz (v); });
    }

    std::unique_ptr<FloatParam> makeGain (const char* id, const char* name, float range)
    {
        return makeFloat (id, name, juce::NormalisableRange<float> (-range, range, 0.1f), 0.0f,
                          [] (float v, int) { return formatDb (v); });
    }

    std::unique_ptr<FloatParam> makeQ (const char* id, const char* name, float def)
    {
        return makeFloat (id, name, skewed (0.3f, 8.0f, 1.2f), def,
                          [] (float v, int) { return juce::String (v, 2); });
    }

    std::unique_ptr<FloatParam> makePercent (const char* id, const char* name, float def, float maxValue = 1.0f)
    {
        return makeFloat (id, name, juce::NormalisableRange<float> (0.0f, maxValue, 0.001f), def,
                          [] (float v, int) { return formatPercent (v); });
    }

    std::unique_ptr<juce::AudioParameterBool> makeBool (const char* id, const char* name, bool def)
    {
        return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, kVersion }, name, def);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray divNames;
    for (auto& d : syncDivisions()) divNames.add (d.name);

    // ---- delay time ----
    layout.add (makeBool (pid::sync, "Sync", true));
    layout.add (makeBool (pid::link, "Link L/R", true));
    layout.add (makeFloat (pid::timeL, "Time L", skewed (1.0f, 2000.0f, 220.0f, 0.01f), 300.0f,
                           [] (float v, int) { return formatMs (v); }));
    layout.add (makeFloat (pid::timeR, "Time R", skewed (1.0f, 2000.0f, 220.0f, 0.01f), 300.0f,
                           [] (float v, int) { return formatMs (v); }));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { pid::divL, kVersion }, "Division L", divNames, kDefaultDivision));
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { pid::divR, kVersion }, "Division R", divNames, kDefaultDivision));

    // ---- feedback / character ----
    layout.add (makePercent (pid::feedback, "Feedback", 0.40f));
    layout.add (makeBool (pid::pingPong, "Ping-Pong", false));
    layout.add (makePercent (pid::width, "Width", 1.0f, 2.0f));
    layout.add (makeFloat (pid::lowCut, "Low Cut", skewed (20.0f, 2000.0f, 200.0f), 200.0f,
                           [] (float v, int) { return v <= 20.5f ? juce::String ("OFF") : formatHz (v); }));
    layout.add (makeFloat (pid::highCut, "High Cut", skewed (500.0f, 20000.0f, 4000.0f), 6000.0f,
                           [] (float v, int) { return v >= 19900.0f ? juce::String ("OFF") : formatHz (v); }));
    layout.add (makePercent (pid::drive, "Drive", 0.15f));
    layout.add (makePercent (pid::mod, "Mod Depth", 0.20f));
    layout.add (makeFloat (pid::modRate, "Mod Rate", skewed (0.05f, 8.0f, 0.8f), 0.8f,
                           [] (float v, int) { return juce::String (v, 2) + " Hz"; }));
    layout.add (makePercent (pid::diffusion, "Diffusion", 0.0f));
    layout.add (makeFloat (pid::wetLevel, "Echo Level", juce::NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f,
                           [] (float v, int) { return formatDb (v); }));
    layout.add (makeFloat (pid::tail, "Max Tail", juce::NormalisableRange<float> (0.5f, 15.0f, 0.1f), 6.0f,
                           [] (float v, int) { return juce::String (v, 1) + " s"; }));

    // ---- behaviour ----
    layout.add (makeBool (pid::hearDry, "Hear Dry", true));
    layout.add (makeBool (pid::exportDry, "Export Dry", false));
    layout.add (makeBool (pid::muteLive, "Mute Live", true));
    layout.add (makeBool (pid::loop, "Loop", false));

    // ---- EQ ----
    layout.add (makeBool (pid::eqOn, "EQ On", true));
    layout.add (makeFreq (pid::eqHp, "EQ Low Cut", 20.0f, 1000.0f, 20.0f));
    layout.add (makeFreq (pid::eqLp, "EQ High Cut", 1000.0f, 20000.0f, 20000.0f));
    layout.add (makeFreq (pid::eqLsF, "EQ Low Shelf Freq", 40.0f, 800.0f, 120.0f));
    layout.add (makeGain (pid::eqLsG, "EQ Low Shelf Gain", 15.0f));
    layout.add (makeFreq (pid::eq1F, "EQ Bell 1 Freq", 100.0f, 3000.0f, 450.0f));
    layout.add (makeGain (pid::eq1G, "EQ Bell 1 Gain", 15.0f));
    layout.add (makeQ (pid::eq1Q, "EQ Bell 1 Q", 1.0f));
    layout.add (makeFreq (pid::eq2F, "EQ Bell 2 Freq", 800.0f, 10000.0f, 3500.0f));
    layout.add (makeGain (pid::eq2G, "EQ Bell 2 Gain", 15.0f));
    layout.add (makeQ (pid::eq2Q, "EQ Bell 2 Q", 1.0f));
    layout.add (makeFreq (pid::eqHsF, "EQ High Shelf Freq", 2000.0f, 16000.0f, 8000.0f));
    layout.add (makeGain (pid::eqHsG, "EQ High Shelf Gain", 15.0f));

    return layout;
}

} // namespace et

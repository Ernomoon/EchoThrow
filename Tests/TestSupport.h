#pragma once
// Shared helpers for the processor / UI tests: a fake host that feeds a synthetic vocal through the plugin.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "DSP/Eq.h"

namespace testsupport
{
inline juce::AudioBuffer<float> makeVoice (double sr, double seconds)
{
    const int n = (int) (seconds * sr);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    et::Biquad q1 { et::BiquadCoefs::peak (sr, 700, 6, 18) }, q2 { et::BiquadCoefs::peak (sr, 1200, 8, 14) }, q3 { et::BiquadCoefs::peak (sr, 2600, 8, 12) };
    double ph = 0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        ph += 200.0 * (1.0 + 0.012 * std::sin (2 * et::kPi * 5.5 * t)) / sr;
        ph -= std::floor (ph);
        const double s = q3.process (q2.process (q1.process (ph * 2.0 - 1.0)));
        const double env = std::min (1.0, t / 0.02) * std::min (1.0, (seconds - t) / 0.12);
        b.setSample (0, i, (float) (0.12 * s * env));
        b.setSample (1, i, (float) (0.12 * s * env));
    }
    return b;
}

/** Plays a buffer through the processor block by block; returns what came out. */
inline juce::AudioBuffer<float> run (et::EchoThrowProcessor& proc, const juce::AudioBuffer<float>& in, int block = 512)
{
    juce::AudioBuffer<float> out (2, in.getNumSamples());
    juce::MidiBuffer midi;
    for (int pos = 0; pos < in.getNumSamples(); pos += block)
    {
        const int n = std::min (block, in.getNumSamples() - pos);
        juce::AudioBuffer<float> b (2, n);
        for (int ch = 0; ch < 2; ++ch) b.copyFrom (ch, 0, in, ch, pos, n);
        proc.processBlock (b, midi);
        for (int ch = 0; ch < 2; ++ch) out.copyFrom (ch, pos, b, ch, 0, n);
    }
    return out;
}

inline juce::AudioBuffer<float> silence (double sr, double seconds)
{
    juce::AudioBuffer<float> b (2, (int) (seconds * sr));
    b.clear();
    return b;
}

inline void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

inline bool waitForRender (et::EchoThrowProcessor& proc, uint64_t notId = 0, int timeoutMs = 5000)
{
    for (int waited = 0; waited < timeoutMs; waited += 20)
    {
        pump (20);
        if (auto c = proc.getRenderedClip())
            if (c->id != notId) return true;
    }
    return false;
}

/** REC -> play a vocal through it -> REC again. Returns when the clip has been collected and rendered. */
inline void recordTake (et::EchoThrowProcessor& proc, double sr, double voiceSeconds = 0.8)
{
    proc.toggleRecord();                                   // arm
    run (proc, silence (sr, 0.3));                         // nothing yet: must stay armed
    run (proc, makeVoice (sr, voiceSeconds));
    run (proc, silence (sr, 0.4));
    proc.toggleRecord();                                   // stop
    run (proc, silence (sr, 0.05));
    waitForRender (proc);
}
}

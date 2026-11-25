#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <functional>
#include "Eq.h"

namespace et
{
constexpr int kSpectrumPoints = 256;

/** Everything the offline delay renderer needs. All values are plain numbers so the renderer is easy to test. */
struct DelayParams
{
    double sampleRate = 48000.0;
    double timeLSec = 0.25, timeRSec = 0.25;
    float  feedback  = 0.4f;     // 0..1
    bool   pingPong  = false;
    float  loopLowCutHz = 200.0f;     // inside the feedback loop (<= 20.5 = off)
    float  loopHighCutHz = 6000.0f;   // inside the feedback loop (>= 19900 = off)
    float  drive     = 0.15f;    // 0..1  tape-style saturation in the loop
    float  modDepth  = 0.2f;     // 0..1  wow / flutter / chorus amount
    float  modRateHz = 0.8f;
    float  diffusion = 0.0f;     // 0..1
    float  width     = 1.0f;     // 0..2
    float  wetGainDb = 0.0f;
    float  tailMaxSec = 6.0f;
    EqSettings eq;
};

/** The result of one render. wet and dry are separate so "hear dry" / "export dry" need no re-render. */
struct RenderedClip
{
    juce::AudioBuffer<float> wet;     // 2 ch, outLen samples
    juce::AudioBuffer<float> dry;     // 2 ch, srcLen samples (trimmed + faded source)
    int srcLen = 0;
    int outLen = 0;
    double sampleRate = 48000.0;
    uint64_t id = 0;
    float wetPeak = 0.0f;
    bool truncatedTail = false;       // true if the tail hit Max Tail while still audible
    std::array<float, kSpectrumPoints> spectrumDb {};   // pre-EQ echo spectrum, dB relative to its own max (<= 0)
};

class DelayEngine
{
public:
    /** Renders `src[start, end)` through the delay.
        Returns false if aborted (shouldAbort() returned true). */
    static bool render (const juce::AudioBuffer<float>& src, int start, int end,
                        const DelayParams& params, RenderedClip& out,
                        const std::function<bool()>& shouldAbort = {});
};

} // namespace et

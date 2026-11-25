#include "DelayEngine.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

namespace et
{
namespace
{
    //==========================================================================
    /** Circular delay line with 4-point Hermite interpolation. Read happens before write. */
    struct DelayLine
    {
        std::vector<float> buf;
        int mask = 0, w = 0;

        void init (int minSize)
        {
            int s = 16;
            while (s < minSize) s <<= 1;
            buf.assign ((size_t) s, 0.0f);
            mask = s - 1;
            w = 0;
        }

        inline void write (float x) noexcept
        {
            buf[(size_t) w] = x;
            w = (w + 1) & mask;
        }

        /** Delay `d` samples, d >= 2. */
        inline float read (double d) const noexcept
        {
            const int   i = (int) d;
            const float f = (float) (d - (double) i);
            const float ym1 = buf[(size_t) ((w - i + 1) & mask)];
            const float y0  = buf[(size_t) ((w - i)     & mask)];
            const float y1  = buf[(size_t) ((w - i - 1) & mask)];
            const float y2  = buf[(size_t) ((w - i - 2) & mask)];
            const float c1 = 0.5f * (y1 - ym1);
            const float c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2;
            const float c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
            return ((c3 * f + c2) * f + c1) * f + y0;
        }
    };

    /** Schroeder all-pass used for diffusion inside the feedback loop. */
    struct Allpass
    {
        std::vector<float> buf;
        int idx = 0;
        float g = 0.0f;

        void init (int n) { buf.assign ((size_t) std::max (1, n), 0.0f); idx = 0; }

        inline float process (float x) noexcept
        {
            const float d = buf[(size_t) idx];
            const float v = x - g * d;
            const float y = g * v + d;
            buf[(size_t) idx] = v;
            if (++idx >= (int) buf.size()) idx = 0;
            return y;
        }
    };

    /** Everything that happens to the signal each time it goes round the loop. */
    struct LoopProcessor
    {
        Allpass ap[4];
        bool  hpOn = false, lpOn = false, driveOn = false, diffOn = false;
        float hpA = 0.0f, hpState = 0.0f;
        Biquad lp;
        float driveGain = 1.0f;

        void prepare (double sr, const DelayParams& p, bool rightChannel)
        {
            static const float lenL[4] = { 3.1f, 5.3f, 8.9f, 13.7f };
            static const float lenR[4] = { 3.7f, 6.1f, 10.3f, 15.1f };
            const float* lens = rightChannel ? lenR : lenL;
            diffOn = p.diffusion > 0.005f;
            for (int i = 0; i < 4; ++i)
            {
                ap[i].init ((int) std::round (lens[i] * 0.001 * sr));
                ap[i].g = 0.72f * p.diffusion;
            }

            hpOn = p.loopLowCutHz > 20.5f;
            const double hpF = std::clamp ((double) p.loopLowCutHz, 10.0, sr * 0.45);
            hpA = (float) (1.0 - std::exp (-2.0 * kPi * hpF / sr));
            hpState = 0.0f;

            lpOn = p.loopHighCutHz < 19900.0f;
            lp.c = BiquadCoefs::lowPass (sr, std::clamp ((double) p.loopHighCutHz, 100.0, sr * 0.45), 0.7071);
            lp.reset();

            driveOn = p.drive > 0.002f;
            driveGain = 1.0f + 3.0f * p.drive;
        }

        inline float process (float x) noexcept
        {
            if (diffOn)
                for (auto& a : ap) x = a.process (x);

            if (hpOn)
            {
                hpState += hpA * (x - hpState);      // one-pole low-pass ...
                x -= hpState;                        // ... subtracted = one-pole high-pass
            }
            if (lpOn) x = (float) lp.process ((double) x);

            if (driveOn)                             // small-signal gain stays 1, so feedback behaviour is unchanged
                x = std::tanh (x * driveGain) / driveGain;

            // transparent safety limiter: the loop can never run away, even at 100 % feedback
            const float ax = std::abs (x);
            if (ax > 0.8f)
                x = std::copysign (0.8f + 0.2f * std::tanh ((ax - 0.8f) / 0.2f), x);

            return x;
        }
    };

    inline float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }
}

//==============================================================================
bool DelayEngine::render (const juce::AudioBuffer<float>& src, int start, int end,
                          const DelayParams& p, RenderedClip& out,
                          const std::function<bool()>& shouldAbort)
{
    juce::ScopedNoDenormals noDenormals;

    const double sr = p.sampleRate;
    const int srcTotal = src.getNumSamples();
    start = juce::jlimit (0, std::max (0, srcTotal - 1), start);
    end   = juce::jlimit (start + 1, std::max (start + 1, srcTotal), end);
    const int srcLen = end - start;

    out.sampleRate = sr;
    out.srcLen = srcLen;
    out.truncatedTail = false;

    // ---- dry copy with edge fades (so a hard trim never clicks) -----------
    out.dry.setSize (2, srcLen, false, true, false);
    for (int ch = 0; ch < 2; ++ch)
    {
        const int sc = std::min (ch, src.getNumChannels() - 1);
        if (sc >= 0 && srcTotal > 0)
            out.dry.copyFrom (ch, 0, src, sc, start, srcLen);
    }
    {
        const int fadeIn  = std::min (srcLen / 2, (int) std::round ((start > 0 ? 0.004 : 0.001) * sr));
        const int fadeOut = std::min (srcLen / 2, (int) std::round ((end < srcTotal ? 0.010 : 0.002) * sr));
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = out.dry.getWritePointer (ch);
            for (int i = 0; i < fadeIn; ++i)
                d[i] *= 0.5f - 0.5f * std::cos ((float) kPi * (float) i / (float) fadeIn);
            for (int i = 0; i < fadeOut; ++i)
                d[srcLen - 1 - i] *= 0.5f - 0.5f * std::cos ((float) kPi * (float) i / (float) fadeOut);
        }
    }

    // ---- set-up -----------------------------------------------------------
    const double maxTimeSec = 8.0;
    const double tL = std::clamp (p.timeLSec, 0.002, maxTimeSec);
    const double tR = std::clamp (p.timeRSec, 0.002, maxTimeSec);
    const float  fb = juce::jlimit (0.0f, 1.0f, p.feedback);

    const double modDepthSamples = (double) (p.modDepth * p.modDepth) * 0.006 * sr;   // up to 6 ms
    const double modRate = std::clamp ((double) p.modRateHz, 0.01, 20.0);
    const int lineSize = (int) std::ceil ((maxTimeSec + 0.02) * sr) + 16;

    DelayLine lineL, lineR;
    lineL.init (lineSize);
    lineR.init (lineSize);

    LoopProcessor loopL, loopR;
    loopL.prepare (sr, p, false);
    loopR.prepare (sr, p, true);

    EqChain eq;
    eq.prepare (sr, p.eq);

    const float wetGain = dbToGain (p.wetGainDb);
    const float width = juce::jlimit (0.0f, 2.0f, p.width);

    const int maxOut = srcLen + (int) std::ceil (std::clamp ((double) p.tailMaxSec, 0.2, 30.0) * sr);

    out.wet.setSize (2, maxOut, false, true, false);
    auto* wetL = out.wet.getWritePointer (0);
    auto* wetR = out.wet.getWritePointer (1);
    const auto* dryL = out.dry.getReadPointer (0);
    const auto* dryR = out.dry.getReadPointer (1);

    std::vector<float> preEq ((size_t) maxOut, 0.0f);   // mono pre-EQ echoes, for the spectrum

    // modulation state
    double lfoPhase = 0.0;
    const double lfoInc = 2.0 * kPi * modRate / sr;
    uint32_t rng = 0x1234567u;
    float noiseL = 0.0f, noiseR = 0.0f;
    const float noiseCoef = (float) (1.0 - std::exp (-2.0 * kPi * 2.5 / sr));

    // end-of-tail detection
    const int blockSize = 512;
    const float silenceThreshold = 0.00025f;   // about -72 dBFS
    // The gap between two echoes can be as long as the longest delay time, so silence must last longer than that
    // before we can be sure the tail is really over.
    const int minEnd = srcLen + (int) std::ceil ((tL + tR) * sr) + (int) (0.3 * sr);
    const int quietBlocksNeeded = std::max (3, (int) std::ceil ((std::max (tL, tR) * 1.15 + 0.06) * sr / blockSize));
    int quietBlocks = 0;
    float blockPeak = 0.0f;
    int outLen = maxOut;
    bool endedEarly = false;
    float peak = 0.0f;

    const double maxD = (double) (lineSize - 8);

    for (int n = 0; n < maxOut; ++n)
    {
        if ((n & 4095) == 0 && shouldAbort && shouldAbort())
            return false;

        const float inL = n < srcLen ? dryL[n] : 0.0f;
        const float inR = n < srcLen ? dryR[n] : 0.0f;

        // --- modulation: slow sine + smoothed random drift, different phase per side ---
        float modL = 0.0f, modR = 0.0f;
        if (modDepthSamples > 0.0)
        {
            rng = rng * 1664525u + 1013904223u;
            const float rl = (float) (rng >> 8) * (1.0f / 8388608.0f) - 1.0f;
            rng = rng * 1664525u + 1013904223u;
            const float rr = (float) (rng >> 8) * (1.0f / 8388608.0f) - 1.0f;
            noiseL += noiseCoef * (rl - noiseL);
            noiseR += noiseCoef * (rr - noiseR);
            const double sL = std::sin (lfoPhase), sR = std::sin (lfoPhase + 1.3);
            modL = (float) (modDepthSamples * (0.8 * sL + 3.0 * (double) noiseL));
            modR = (float) (modDepthSamples * (0.8 * sR + 3.0 * (double) noiseR));
            lfoPhase += lfoInc;
            if (lfoPhase > 2.0 * kPi) lfoPhase -= 2.0 * kPi;
        }

        const double dL = std::clamp (tL * sr + (double) modL, 2.0, maxD);
        const double dR = std::clamp (tR * sr + (double) modR, 2.0, maxD);
        const float echoL = lineL.read (dL);
        const float echoR = lineR.read (dR);

        float wL, wR;
        if (p.pingPong)
        {
            const float mono = 0.5f * (inL + inR);
            wL = mono + fb * echoR;
            wR = fb * echoL;
        }
        else
        {
            wL = inL + fb * echoL;
            wR = inR + fb * echoR;
        }
        lineL.write (loopL.process (wL));
        lineR.write (loopR.process (wR));

        preEq[(size_t) n] = 0.5f * (echoL + echoR);

        // --- output stage: EQ -> width -> level ---
        float oL = (float) eq.process (0, (double) echoL);
        float oR = (float) eq.process (1, (double) echoR);
        const float mid = 0.5f * (oL + oR), side = 0.5f * (oL - oR) * width;
        oL = (mid + side) * wetGain;
        oR = (mid - side) * wetGain;
        wetL[n] = oL;
        wetR[n] = oR;

        const float a = std::max (std::abs (oL), std::abs (oR));
        peak = std::max (peak, a);

        // --- find the end of the tail ---
        blockPeak = std::max (blockPeak, a);
        if (((n + 1) % blockSize) == 0)
        {
            if (n > minEnd && n >= srcLen)
            {
                quietBlocks = blockPeak < silenceThreshold ? quietBlocks + 1 : 0;
                if (quietBlocks >= quietBlocksNeeded)
                {
                    outLen = std::min (maxOut, n + 1);
                    endedEarly = true;
                    break;
                }
            }
            blockPeak = 0.0f;
        }
    }

    // Cut the silence off the end: keep everything up to the last audible sample plus a short pad.
    {
        int lastLoud = srcLen;
        const int scanFrom = std::min (outLen, maxOut) - 1;
        for (int i = scanFrom; i >= srcLen; --i)
            if (std::abs (wetL[i]) > silenceThreshold || std::abs (wetR[i]) > silenceThreshold) { lastLoud = i; break; }

        const int pad = (int) (0.03 * sr);
        out.truncatedTail = ! endedEarly && lastLoud + (int) (0.05 * sr) >= maxOut;
        outLen = std::min (outLen, lastLoud + 1 + pad);
    }
    outLen = std::max (outLen, srcLen);

    // ---- finish: trim, fade the end (long fade only if we had to cut an audible tail) ----
    out.outLen = outLen;
    out.wet.setSize (2, outLen, true, false, true);

    const int tailLen = outLen - srcLen;
    const int fadeLen = std::max (1, out.truncatedTail ? std::min (tailLen, (int) (0.12 * sr))
                                                       : std::min (tailLen, (int) (0.015 * sr)));
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* d = out.wet.getWritePointer (ch);
        for (int i = 0; i < fadeLen && i < outLen; ++i)
        {
            const float g = 0.5f + 0.5f * std::cos ((float) kPi * (float) i / (float) fadeLen);
            d[outLen - fadeLen + i] *= g;
        }
    }
    out.wetPeak = peak;

    // ---- spectrum of the echoes (before the output EQ) --------------------
    {
        constexpr int order = 12, fftSize = 1 << order;
        juce::dsp::FFT fft (order);
        juce::dsp::WindowingFunction<float> window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann);
        std::vector<float> frame ((size_t) fftSize * 2, 0.0f);
        std::vector<double> acc ((size_t) fftSize / 2, 0.0);
        int frames = 0;

        for (int pos = 0; pos + fftSize <= outLen || (pos == 0 && outLen > 0); pos += fftSize / 2)
        {
            std::fill (frame.begin(), frame.end(), 0.0f);
            const int count = std::min (fftSize, outLen - pos);
            double energy = 0.0;
            for (int i = 0; i < count; ++i)
            {
                frame[(size_t) i] = preEq[(size_t) (pos + i)];
                energy += (double) frame[(size_t) i] * frame[(size_t) i];
            }
            if (energy / (double) fftSize < 1.0e-10)
                continue;   // skip silence so it doesn't dilute the average

            window.multiplyWithWindowingTable (frame.data(), (size_t) fftSize);
            fft.performFrequencyOnlyForwardTransform (frame.data());
            for (int k = 0; k < fftSize / 2; ++k)
                acc[(size_t) k] += (double) frame[(size_t) k] * frame[(size_t) k];
            ++frames;
            if (outLen < fftSize) break;
        }

        out.spectrumDb.fill (-90.0f);
        if (frames > 0)
        {
            const double binHz = sr / (double) fftSize;
            std::array<double, kSpectrumPoints> db {};
            double maxDb = -200.0;
            for (int i = 0; i < kSpectrumPoints; ++i)
            {
                const double f0 = 20.0 * std::pow (1000.0, (double) i / kSpectrumPoints);
                const double f1 = 20.0 * std::pow (1000.0, (double) (i + 1) / kSpectrumPoints);
                int k0 = std::max (1, (int) std::floor (f0 / binHz));
                int k1 = std::min (fftSize / 2 - 1, std::max (k0, (int) std::ceil (f1 / binHz)));
                double sum = 0.0; int cnt = 0;
                for (int k = k0; k <= k1; ++k) { sum += acc[(size_t) k]; ++cnt; }
                const double mean = cnt > 0 ? sum / cnt / frames : 0.0;
                db[(size_t) i] = 10.0 * std::log10 (mean + 1.0e-18);
                maxDb = std::max (maxDb, db[(size_t) i]);
            }
            // light smoothing across neighbouring points
            for (int i = 0; i < kSpectrumPoints; ++i)
            {
                const double a = db[(size_t) std::max (0, i - 1)], b = db[(size_t) i], c = db[(size_t) std::min (kSpectrumPoints - 1, i + 1)];
                out.spectrumDb[(size_t) i] = (float) std::clamp ((0.25 * a + 0.5 * b + 0.25 * c) - maxDb, -90.0, 0.0);
            }
        }
    }

    return true;
}

} // namespace et

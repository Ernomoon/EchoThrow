// Offline tests for the delay renderer. Run: DspTest <output-dir>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "DSP/DelayEngine.h"
#include <cstdio>
#include <random>
#include <chrono>

using namespace et;

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("  FAIL: " __VA_ARGS__); std::printf ("   [%s:%d]\n", __FILE__, __LINE__); } } while (0)
#define INFO(...) do { std::printf ("  " __VA_ARGS__); std::printf ("\n"); } while (0)

static const double SR = 48000.0;

/** A vaguely vocal-like "ahh": 200 Hz glottal pulse through three formants, with vibrato and an envelope. */
static juce::AudioBuffer<float> makeVoice (double seconds, double padSeconds = 0.0)
{
    const int n = (int) (seconds * SR), pad = (int) (padSeconds * SR);
    juce::AudioBuffer<float> b (2, n + pad);
    b.clear();
    BiquadCoefs f1 = BiquadCoefs::peak (SR, 700, 6, 18), f2 = BiquadCoefs::peak (SR, 1200, 8, 14), f3 = BiquadCoefs::peak (SR, 2600, 8, 12);
    Biquad q1 { f1 }, q2 { f2 }, q3 { f3 };
    double ph = 0;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / SR;
        const double f0 = 200.0 * (1.0 + 0.012 * std::sin (2 * kPi * 5.5 * t));
        ph += f0 / SR; ph -= std::floor (ph);
        double s = (ph * 2.0 - 1.0);                      // saw
        s = q3.process (q2.process (q1.process (s)));
        const double env = std::min (1.0, t / 0.02) * std::min (1.0, (seconds - t) / 0.12);
        const float v = (float) (0.12 * s * env);
        b.setSample (0, i, v);
        b.setSample (1, i, v);
    }
    return b;
}

static juce::AudioBuffer<float> makeNoise (double seconds)
{
    juce::AudioBuffer<float> b (2, (int) (seconds * SR));
    std::mt19937 rng (7);
    std::uniform_real_distribution<float> d (-0.3f, 0.3f);
    for (int i = 0; i < b.getNumSamples(); ++i) { const float v = d (rng); b.setSample (0, i, v); b.setSample (1, i, v); }
    return b;
}

static void writeWav (const juce::File& f, const juce::AudioBuffer<float>& wet, const juce::AudioBuffer<float>* dry)
{
    juce::AudioBuffer<float> mix (2, wet.getNumSamples());
    mix.clear();
    for (int ch = 0; ch < 2; ++ch)
    {
        mix.copyFrom (ch, 0, wet, ch, 0, wet.getNumSamples());
        if (dry) mix.addFrom (ch, 0, *dry, ch, 0, std::min (dry->getNumSamples(), mix.getNumSamples()));
    }
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), SR, 2, 32, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (mix, 0, mix.getNumSamples());
}

static double rms (const juce::AudioBuffer<float>& b, int ch, int s0, int len)
{
    s0 = std::max (0, s0); len = std::min (len, b.getNumSamples() - s0);
    if (len <= 0) return 0;
    double a = 0; const float* d = b.getReadPointer (ch);
    for (int i = 0; i < len; ++i) a += (double) d[s0 + i] * d[s0 + i];
    return std::sqrt (a / len);
}

static bool allFinite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i))) return false;
    return true;
}

static DelayParams plain()      // no colouring: easy to reason about
{
    DelayParams p; p.sampleRate = SR; p.feedback = 0.5f; p.loopLowCutHz = 20.0f; p.loopHighCutHz = 20000.0f;
    p.drive = 0; p.modDepth = 0; p.diffusion = 0; p.width = 1; p.wetGainDb = 0; p.tailMaxSec = 8;
    p.timeLSec = p.timeRSec = 0.3;
    return p;
}

/** Band energy in dB using a simple DFT-free approach: filter with RBJ band-pass-ish via biquads. */
static double bandRmsDb (const juce::AudioBuffer<float>& b, double lo, double hi)
{
    Biquad h { BiquadCoefs::highPass (SR, lo, 0.7071) }, l { BiquadCoefs::lowPass (SR, hi, 0.7071) };
    Biquad h2 = h, l2 = l;
    double a = 0;
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double y = l2.process (l.process (h2.process (h.process ((double) b.getSample (0, i)))));
        a += y * y;
    }
    return 10.0 * std::log10 (a / std::max (1, b.getNumSamples()) + 1e-20);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::File outDir = argc > 1 ? juce::File (argv[1]) : juce::File::getCurrentWorkingDirectory();
    outDir.createDirectory();

    auto voice = makeVoice (0.8);

    //------------------------------------------------------------------ 1
    std::printf ("[1] first echo arrives exactly one delay time later\n");
    {
        auto p = plain(); p.timeLSec = p.timeRSec = 0.25;
        RenderedClip c; CHECK (DelayEngine::render (voice, 0, voice.getNumSamples(), p, c), "render");
        const int T = (int) (0.25 * SR);
        // cross-correlate dry with wet over a range of lags
        double best = -1; int bestLag = 0;
        for (int lag = T - 200; lag <= T + 200; ++lag)
        {
            double a = 0; const float* d = c.dry.getReadPointer (0); const float* w = c.wet.getReadPointer (0);
            for (int i = 0; i < c.srcLen && i + lag < c.outLen; i += 2) a += (double) d[i] * w[i + lag];
            if (a > best) { best = a; bestLag = lag; }
        }
        INFO ("expected lag %d samples, measured %d (len %d, tail %d)", T, bestLag, c.outLen, c.outLen - c.srcLen);
        CHECK (std::abs (bestLag - T) <= 1, "echo lag off by %d samples", bestLag - T);
        CHECK (allFinite (c.wet), "NaN/Inf in output");
        writeWav (outDir.getChildFile ("t1_plain.wav"), c.wet, &c.dry);
    }

    //------------------------------------------------------------------ 2
    std::printf ("[2] feedback gives geometric decay\n");
    {
        auto p = plain(); p.feedback = 0.5f; p.timeLSec = p.timeRSec = 0.3;
        RenderedClip c; DelayEngine::render (voice, 0, voice.getNumSamples(), p, c);
        // echoes are as long as the source (0.8 s) and the delay is 0.3 s, so they overlap. Use a short burst instead.
        auto burst = makeVoice (0.05);
        RenderedClip b; DelayEngine::render (burst, 0, burst.getNumSamples(), p, b);
        double e[5];
        for (int k = 0; k < 5; ++k) e[k] = rms (b.wet, 0, (int) ((0.3 * (k + 1)) * SR), (int) (0.05 * SR));
        for (int k = 0; k < 4; ++k)
        {
            const double r = e[k + 1] / e[k];
            INFO ("echo %d -> %d ratio %.3f (expected 0.5)", k + 1, k + 2, r);
            CHECK (r > 0.45 && r < 0.55, "decay ratio %.3f", r);
        }
        INFO ("render length %.2f s (tail ended early: %s)", b.outLen / SR, b.truncatedTail ? "no" : "yes");
        CHECK (! b.truncatedTail, "tail should end by itself at 50 %% feedback");
        CHECK (b.outLen < (int) (6.0 * SR), "tail too long: %.2f s", b.outLen / SR);
    }

    //------------------------------------------------------------------ 3
    std::printf ("[3] zero feedback = a single echo\n");
    {
        auto p = plain(); p.feedback = 0.0f; p.timeLSec = p.timeRSec = 0.2;
        auto burst = makeVoice (0.05);
        RenderedClip b; DelayEngine::render (burst, 0, burst.getNumSamples(), p, b);
        const double e1 = rms (b.wet, 0, (int) (0.2 * SR), (int) (0.05 * SR));
        const double e2 = rms (b.wet, 0, (int) (0.4 * SR), (int) (0.05 * SR));
        INFO ("echo1 %.5f, echo2 %.6f, length %.2f s", e1, e2, b.outLen / SR);
        CHECK (e1 > 0.003, "first echo missing");
        CHECK (e2 < e1 * 0.001, "unexpected second echo");
        CHECK (b.outLen < (int) (0.8 * SR), "should end soon after the first echo");
    }

    //------------------------------------------------------------------ 4
    std::printf ("[4] 100%% feedback is bounded, tail is cut at Max Tail and faded\n");
    {
        auto p = plain(); p.feedback = 1.0f; p.tailMaxSec = 3.0f; p.drive = 0.4f;
        RenderedClip c; DelayEngine::render (voice, 0, voice.getNumSamples(), p, c);
        INFO ("length %.2f s, peak %.3f, truncated=%d", c.outLen / SR, c.wetPeak, (int) c.truncatedTail);
        CHECK (allFinite (c.wet), "NaN/Inf");
        CHECK (c.wetPeak < 1.2f, "runaway level %.2f", c.wetPeak);
        CHECK (c.truncatedTail, "should report a cut tail");
        CHECK (std::abs (c.wet.getSample (0, c.outLen - 1)) < 1e-3f, "no fade at the end (click)");
        CHECK (std::abs (c.outLen - (c.srcLen + (int) (3.0 * SR))) <= 1, "length should be src + max tail");
    }

    //------------------------------------------------------------------ 5
    std::printf ("[5] ping-pong alternates sides\n");
    {
        auto p = plain(); p.pingPong = true; p.feedback = 0.6f; p.timeLSec = p.timeRSec = 0.25;
        auto burst = makeVoice (0.05);
        RenderedClip c; DelayEngine::render (burst, 0, burst.getNumSamples(), p, c);
        double L[4], R[4];
        for (int k = 0; k < 4; ++k)
        {
            L[k] = rms (c.wet, 0, (int) (0.25 * (k + 1) * SR), (int) (0.05 * SR));
            R[k] = rms (c.wet, 1, (int) (0.25 * (k + 1) * SR), (int) (0.05 * SR));
            INFO ("echo %d  L %.5f  R %.5f", k + 1, L[k], R[k]);
        }
        CHECK (L[0] > 0.003 && R[0] < L[0] * 0.01, "echo 1 should be left only");
        CHECK (R[1] > 0.002 && L[1] < R[1] * 0.01, "echo 2 should be right only");
        CHECK (L[2] > 0.001 && R[2] < L[2] * 0.01, "echo 3 should be left only");
        writeWav (outDir.getChildFile ("t5_pingpong.wav"), c.wet, &c.dry);
    }

    //------------------------------------------------------------------ 6
    std::printf ("[6] loop filters darken every repeat\n");
    {
        auto p = plain(); p.feedback = 0.6f; p.loopLowCutHz = 300; p.loopHighCutHz = 3000; p.timeLSec = p.timeRSec = 0.4;
        auto noise = makeNoise (0.1);
        RenderedClip c; DelayEngine::render (noise, 0, noise.getNumSamples(), p, c);
        double prev = 1e9;
        for (int k = 1; k <= 3; ++k)
        {
            juce::AudioBuffer<float> seg (1, (int) (0.1 * SR));
            seg.copyFrom (0, 0, c.wet, 0, (int) (0.4 * k * SR), seg.getNumSamples());
            const double hi = bandRmsDb (seg, 6000, 12000), mid = bandRmsDb (seg, 500, 2000);
            INFO ("echo %d: 6-12 kHz %.1f dB, 0.5-2 kHz %.1f dB (hi-mid %.1f)", k, hi, mid, hi - mid);
            CHECK (hi - mid < prev, "high end should drop relative to mids on each repeat");
            prev = hi - mid;
        }
    }

    //------------------------------------------------------------------ 7
    std::printf ("[7] output EQ\n");
    {
        auto noise = makeNoise (0.5);
        auto p = plain(); p.feedback = 0.0f; p.timeLSec = p.timeRSec = 0.05;
        RenderedClip flat; DelayEngine::render (noise, 0, noise.getNumSamples(), p, flat);
        p.eq.hpF = 1000.0f; p.eq.hsG = -12.0f; p.eq.hsF = 6000.0f;
        RenderedClip eq; DelayEngine::render (noise, 0, noise.getNumSamples(), p, eq);
        const double lowA = bandRmsDb (flat.wet, 60, 400), lowB = bandRmsDb (eq.wet, 60, 400);
        const double hiA = bandRmsDb (flat.wet, 9000, 14000), hiB = bandRmsDb (eq.wet, 9000, 14000);
        const double midA = bandRmsDb (flat.wet, 2000, 4000), midB = bandRmsDb (eq.wet, 2000, 4000);
        INFO ("low band %.1f dB change, mid band %.1f dB change, high shelf %.1f dB change", lowB - lowA, midB - midA, hiB - hiA);
        CHECK (lowB - lowA < -15.0, "HP at 1 kHz should remove lows");
        CHECK (std::abs (midB - midA) < 3.0, "mids should be roughly unchanged");
        CHECK (hiB - hiA < -9.0 && hiB - hiA > -15.0, "high shelf -12 dB");
        // analytic response sanity
        EqSettings s; s.b1G = 6; s.b1F = 1000; s.b1Q = 1;
        const double r = EqChain::responseDb (s, SR, 1000.0);
        CHECK (std::abs (r - 6.0) < 0.05, "bell centre gain %.2f", r);
    }

    //------------------------------------------------------------------ 8
    std::printf ("[8] trimming: only the selected region feeds the delay\n");
    {
        auto p = plain(); p.feedback = 0.0f; p.timeLSec = p.timeRSec = 0.25;
        auto v = makeVoice (0.8);
        const int s = (int) (0.5 * SR), e = (int) (0.7 * SR);
        RenderedClip c; DelayEngine::render (v, s, e, p, c);
        INFO ("srcLen %d (expected %d) outLen %d", c.srcLen, e - s, c.outLen);
        CHECK (c.srcLen == e - s, "srcLen");
        CHECK (std::abs (c.dry.getSample (0, 0)) < 1e-4f, "dry should fade in (no click)");
        CHECK (std::abs (c.dry.getSample (0, c.srcLen - 1)) < 1e-4f, "dry should fade out (no click)");
        // wet should start at 0.25 s
        CHECK (rms (c.wet, 0, 0, (int) (0.2 * SR)) < 1e-6, "silence before first echo");
        CHECK (rms (c.wet, 0, (int) (0.26 * SR), (int) (0.1 * SR)) > 1e-3, "echo present");
    }

    //------------------------------------------------------------------ 9
    std::printf ("[9] stress: 400 random parameter sets, extreme values\n");
    {
        std::mt19937 rng (99);
        auto U = [&] (float a, float b) { return std::uniform_real_distribution<float> (a, b) (rng); };
        auto burst = makeVoice (0.4);
        int bad = 0; float worst = 0;
        for (int k = 0; k < 400; ++k)
        {
            DelayParams p; p.sampleRate = SR;
            p.timeLSec = U (0.002f, 2.0f); p.timeRSec = U (0.002f, 2.0f);
            p.feedback = U (0.0f, 1.0f); p.pingPong = rng() & 1;
            p.loopLowCutHz = U (20, 2000); p.loopHighCutHz = U (500, 20000);
            p.drive = U (0, 1); p.modDepth = U (0, 1); p.modRateHz = U (0.05f, 8); p.diffusion = U (0, 1);
            p.width = U (0, 2); p.wetGainDb = U (-24, 12); p.tailMaxSec = U (0.5f, 4.0f);
            p.eq.on = rng() & 1; p.eq.hpF = U (20, 1000); p.eq.lpF = U (1000, 20000);
            p.eq.lsF = U (40, 800); p.eq.lsG = U (-15, 15); p.eq.b1F = U (100, 3000); p.eq.b1G = U (-15, 15); p.eq.b1Q = U (0.3f, 8);
            p.eq.b2F = U (800, 10000); p.eq.b2G = U (-15, 15); p.eq.b2Q = U (0.3f, 8); p.eq.hsF = U (2000, 16000); p.eq.hsG = U (-15, 15);
            RenderedClip c;
            DelayEngine::render (burst, 0, burst.getNumSamples(), p, c);
            const float gainLin = std::pow (10.0f, p.wetGainDb / 20.0f);
            worst = std::max (worst, c.wetPeak / gainLin);
            if (! allFinite (c.wet) || c.wetPeak / gainLin > 12.0f) { ++bad; INFO ("bad set #%d peak %.2f", k, c.wetPeak); }
        }
        INFO ("worst peak (before level knob) %.2f", worst);
        CHECK (bad == 0, "%d unstable parameter sets", bad);
    }

    //------------------------------------------------------------------ 10
    std::printf ("[10] performance: 30 s clip, 15 s tail, everything on, 96 kHz-equivalent length\n");
    {
        juce::AudioBuffer<float> big (2, (int) (30 * SR));
        big.clear();
        for (int i = 0; i < big.getNumSamples(); ++i) big.setSample (0, i, 0.1f * std::sin (i * 0.05f));
        DelayParams p; p.sampleRate = SR; p.feedback = 1.0f; p.tailMaxSec = 15; p.diffusion = 0.8f; p.modDepth = 0.6f; p.drive = 0.5f;
        p.eq.b1G = 3; p.eq.b2G = -3; p.eq.hsG = 2; p.eq.lsG = 2; p.eq.hpF = 100; p.eq.lpF = 12000;
        const auto t0 = std::chrono::steady_clock::now();
        RenderedClip c; DelayEngine::render (big, 0, big.getNumSamples(), p, c);
        const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
        INFO ("%.0f ms for %.1f s of audio", ms, c.outLen / SR);
        CHECK (ms < 3000, "too slow: %.0f ms", ms);
    }

    //------------------------------------------------------------------ musical demo renders for listening / plotting
    {
        auto p = plain();
        p.timeLSec = p.timeRSec = 0.375; p.feedback = 0.45f; p.loopLowCutHz = 250; p.loopHighCutHz = 4500; p.drive = 0.2f; p.modDepth = 0.25f;
        p.eq.hpF = 180; p.eq.b2F = 3500; p.eq.b2G = -2.5f;
        auto v = makeVoice (0.6);
        const int s = (int) (0.3 * SR);
        RenderedClip c; DelayEngine::render (v, s, v.getNumSamples(), p, c);
        writeWav (outDir.getChildFile ("demo_throw_wet.wav"), c.wet, nullptr);
        writeWav (outDir.getChildFile ("demo_throw_with_dry.wav"), c.wet, &c.dry);
        std::FILE* f = std::fopen (outDir.getChildFile ("demo_spectrum.txt").getFullPathName().toRawUTF8(), "w");
        for (float v2 : c.spectrumDb) std::fprintf (f, "%.2f\n", v2);
        std::fclose (f);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL DSP TESTS PASSED" : "DSP TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}

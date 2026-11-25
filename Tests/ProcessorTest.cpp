// End-to-end test of the plugin processor with a fake host (no GUI window).
#include "TestSupport.h"
#include <cstdio>

using namespace et;
using namespace testsupport;

static int failures = 0;
#define CHECK(cond, ...) do { if (! (cond)) { ++failures; std::printf ("  FAIL: " __VA_ARGS__); std::printf ("   [line %d]\n", __LINE__); } } while (0)
#define INFO(...) do { std::printf ("  " __VA_ARGS__); std::printf ("\n"); } while (0)

static void setParam (EchoThrowProcessor& p, const char* id, float real)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (real));
}

struct FakeHost : juce::AudioPlayHead
{
    double bpm = 90.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        info.setPpqPosition (16.0);
        info.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
        return info;
    }
};

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    const double SR = 48000.0;
    auto outDir = juce::File::getCurrentWorkingDirectory();

    EchoThrowProcessor proc;
    FakeHost host;
    proc.setPlayHead (&host);
    proc.setPlayConfigDetails (2, 2, SR, 512);
    proc.prepareToPlay (SR, 512);
    pump (100);

    std::printf ("[1] pass-through is untouched, and the take is captured\n");
    {
        const auto voice = makeVoice (SR, 0.8);
        proc.toggleRecord();
        const auto silent = run (proc, silence (SR, 0.3));
        CHECK (proc.getCaptureState() == CaptureEngine::State::Armed, "should stay armed during silence");
        const auto out = run (proc, voice);
        CHECK (proc.getCaptureState() == CaptureEngine::State::Recording, "should be recording after the voice starts");
        double maxDiff = 0;
        for (int i = 0; i < voice.getNumSamples(); ++i)
            maxDiff = std::max (maxDiff, (double) std::abs (out.getSample (0, i) - voice.getSample (0, i)));
        INFO ("max difference between input and output while recording: %g", maxDiff);
        CHECK (maxDiff == 0.0, "the live signal must pass through bit-exact");
        run (proc, silence (SR, 0.4));
        proc.toggleRecord();
        run (proc, silence (SR, 0.05));
        CHECK (waitForRender (proc), "render never arrived");

        auto src = proc.getSource();
        CHECK (src != nullptr, "no source");
        if (src)
        {
            INFO ("captured %.3f s (voice 0.8 s + pre-roll + 0.4 s silence); trimmed to [%d, %d] = %.3f s",
                  src->getNumSamples() / SR, proc.getTrimStart(), proc.getTrimEnd(), (proc.getTrimEnd() - proc.getTrimStart()) / SR);
            CHECK (src->getNumSamples() > (int) (1.15 * SR) && src->getNumSamples() < (int) (1.40 * SR), "unexpected take length");
            const double trimmed = (proc.getTrimEnd() - proc.getTrimStart()) / SR;
            CHECK (trimmed > 0.78 && trimmed < 1.05, "auto-trim should keep the voice and drop the silence (%.3f s)", trimmed);
        }
        INFO ("position hint: '%s'", proc.describePosition (proc.getTrimStart()).toRawUTF8());
        INFO ("position of the take start: '%s'", proc.describePosition (0).toRawUTF8());
        CHECK (proc.describePosition (0).startsWith ("BAR 4  BEAT 4.8"), "capture starts 0.12 s of pre-roll (0.18 beat) before ppq 16 -> bar 4, beat 4.8x");
    }

    std::printf ("[2] LISTEN plays the render, LOOP repeats it, STOP fades out\n");
    {
        auto clip = proc.getRenderedClip();
        CHECK (clip != nullptr, "no clip");
        const int len = clip->outLen;
        INFO ("render: %.2f s, tail %.2f s", len / SR, (len - clip->srcLen) / SR);
        proc.startListening();
        const auto out = run (proc, silence (SR, (len + 2000) / SR));
        double peak = 0; for (int i = 0; i < out.getNumSamples(); ++i) peak = std::max (peak, (double) std::abs (out.getSample (0, i)));
        INFO ("preview peak %.3f, playing afterwards: %d", peak, (int) proc.isListening());
        CHECK (peak > 0.02, "preview is silent");
        CHECK (! proc.isListening(), "should stop at the end when not looping");

        setParam (proc, pid::loop, 1.0f);
        proc.startListening();
        run (proc, silence (SR, (len * 2.2) / SR));
        CHECK (proc.isListening(), "loop should keep playing");
        proc.stopListening();
        const auto tail = run (proc, silence (SR, 0.05));
        CHECK (! proc.isListening(), "stop should end playback");
        CHECK (std::abs (tail.getSample (0, tail.getNumSamples() - 1)) < 1e-6f, "silent after stop fade");
        setParam (proc, pid::loop, 0.0f);
    }

    std::printf ("[3] changing a parameter re-renders\n");
    {
        const auto before = proc.getRenderedClip();
        setParam (proc, pid::feedback, 0.7f);
        CHECK (waitForRender (proc, before->id), "no re-render after a parameter change");
        auto after = proc.getRenderedClip();
        INFO ("tail %.2f s -> %.2f s", (before->outLen - before->srcLen) / SR, (after->outLen - after->srcLen) / SR);
        CHECK (after->outLen > before->outLen, "more feedback should give a longer tail");
        setParam (proc, pid::feedback, 0.4f);
        waitForRender (proc, after->id);
    }

    std::printf ("[4] tempo sync follows the host BPM\n");
    {
        setParam (proc, pid::sync, 1.0f);
        setParam (proc, pid::link, 1.0f);
        setParam (proc, pid::divL, 8.0f);        // 1/4 note
        setParam (proc, pid::feedback, 0.0f);
        pump (150); waitForRender (proc, 0, 200);
        // process a block so the processor sees the host tempo, then change it
        host.bpm = 90.0; run (proc, silence (SR, 0.05)); pump (150);
        auto c1 = proc.getRenderedClip();
        host.bpm = 120.0; run (proc, silence (SR, 0.05));
        CHECK (waitForRender (proc, c1->id), "tempo change should re-render");
        auto c2 = proc.getRenderedClip();
        auto firstEcho = [&] (const RenderedClip& c)
        {
            for (int i = 0; i < c.outLen; ++i)
                if (std::abs (c.wet.getSample (0, i)) > 1e-4f) return i;
            return -1;
        };
        const int e1 = firstEcho (*c1), e2 = firstEcho (*c2);
        INFO ("first echo at %.3f s (90 bpm) and %.3f s (120 bpm); expected 0.667 and 0.500 plus the filter onset", e1 / SR, e2 / SR);
        CHECK (e1 > (int) (0.66 * SR) && e1 < (int) (0.72 * SR), "1/4 at 90 bpm");
        CHECK (e2 > (int) (0.49 * SR) && e2 < (int) (0.56 * SR), "1/4 at 120 bpm");
        setParam (proc, pid::feedback, 0.4f);
    }

    std::printf ("[5] export writes a 32-bit float stereo WAV\n");
    {
        pump (200);
        auto clip = proc.getRenderedClip();
        auto file = proc.exportCurrentClip();
        CHECK (file.existsAsFile(), "no file written");
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> r (wav.createReaderFor (file.createInputStream().release(), true));
        CHECK (r != nullptr, "unreadable wav");
        if (r)
        {
            INFO ("%s: %d ch, %.0f Hz, %d bit, float=%d, %.2f s", file.getFileName().toRawUTF8(), (int) r->numChannels, r->sampleRate,
                  (int) r->bitsPerSample, (int) r->usesFloatingPointData, r->lengthInSamples / r->sampleRate);
            CHECK (r->numChannels == 2 && r->sampleRate == SR && r->usesFloatingPointData, "format");
            CHECK (r->lengthInSamples == clip->outLen, "length %lld vs %d", (long long) r->lengthInSamples, clip->outLen);
        }
        auto again = proc.exportCurrentClip();
        CHECK (again == file, "the same render should not be written twice");
        setParam (proc, pid::exportDry, 1.0f);
        auto withDry = proc.exportCurrentClip();
        CHECK (withDry != file && withDry.existsAsFile(), "export with dry should write a new file");
        setParam (proc, pid::exportDry, 0.0f);
    }

    std::printf ("[6] project state round-trip keeps parameters, clip and trim\n");
    {
        setParam (proc, pid::feedback, 0.63f);
        proc.presetName = "My Throw";
        proc.setTrim (proc.getTrimStart() + 4800, proc.getTrimEnd() - 2400);
        pump (100);
        juce::MemoryBlock state;
        proc.getStateInformation (state);
        INFO ("state size %.1f KB", state.getSize() / 1024.0);

        EchoThrowProcessor other;
        other.setPlayConfigDetails (2, 2, SR, 512);
        other.prepareToPlay (SR, 512);
        other.setStateInformation (state.getData(), (int) state.getSize());
        CHECK (waitForRender (other), "restored plugin never rendered");
        CHECK (std::abs (other.apvts.getRawParameterValue (pid::feedback)->load() - 0.63f) < 1e-4f, "feedback not restored");
        CHECK (other.presetName == "My Throw", "preset name");
        CHECK (other.getTrimStart() == proc.getTrimStart() && other.getTrimEnd() == proc.getTrimEnd(), "trim not restored");
        CHECK (other.getSource() && other.getSource()->getNumSamples() == proc.getSource()->getNumSamples(), "clip length");
        double maxErr = 0;
        if (other.getSource())
            for (int i = 0; i < other.getSource()->getNumSamples(); i += 7)
                maxErr = std::max (maxErr, (double) std::abs (other.getSource()->getSample (0, i) - proc.getSource()->getSample (0, i)));
        INFO ("clip max error after FLAC round trip: %g", maxErr);
        CHECK (maxErr < 1e-5, "FLAC should be 24-bit lossless");
        other.releaseResources();
    }

    std::printf ("[7] host sample-rate change resamples the clip\n");
    {
        auto before = proc.getRenderedClip();
        const int oldLen = proc.getSource()->getNumSamples();
        proc.prepareToPlay (44100.0, 512);
        CHECK (waitForRender (proc, before->id), "no render after rate change");
        auto after = proc.getRenderedClip();
        INFO ("clip %d -> %d samples, render rate %.0f", oldLen, proc.getSource()->getNumSamples(), after->sampleRate);
        CHECK (after->sampleRate == 44100.0, "render should be at the new rate");
        CHECK (std::abs (proc.getSource()->getNumSamples() - (int) std::round (oldLen * 44100.0 / SR)) <= 1, "resampled length");
        proc.prepareToPlay (SR, 512);
        pump (200);
    }

    std::printf ("[8] mono layout and tiny blocks do not crash\n");
    {
        EchoThrowProcessor mono;
        mono.setPlayConfigDetails (1, 1, SR, 64);
        mono.prepareToPlay (SR, 64);
        mono.toggleRecord();
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> b (1, 64);
        for (int i = 0; i < 3000; ++i)
        {
            for (int s = 0; s < 64; ++s) b.setSample (0, s, i > 100 && i < 1000 ? 0.2f * std::sin ((float) (i * 64 + s) * 0.05f) : 0.0f);
            mono.processBlock (b, midi);
        }
        mono.toggleRecord();
        mono.processBlock (b, midi);
        waitForRender (mono);
        mono.startListening();
        for (int i = 0; i < 400; ++i) mono.processBlock (b, midi);
        CHECK (mono.getRenderedClip() != nullptr, "mono render");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PROCESSOR TESTS PASSED" : "PROCESSOR TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}

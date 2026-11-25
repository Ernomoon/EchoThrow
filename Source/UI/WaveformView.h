#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"

namespace et
{
/** Shows the captured clip (cyan), the rendered echoes (amber), the trim handles and the play-head.
    While recording it shows the live take growing. */
class WaveformView : public juce::Component
{
public:
    explicit WaveformView (EchoThrowProcessor&);

    /** Called ~30 times a second by the editor. */
    void refresh();

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { dragHandle = 0; }
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    static constexpr int kBin = 256;
    static constexpr int kLiveBin = 1024;

    void computePeaks (const juce::AudioBuffer<float>& buf, int numSamples, std::vector<float>& out) const;
    juce::Rectangle<float> waveArea() const;
    float xForSample (double s) const;
    double sampleForX (float x) const;
    int hitHandle (float x) const;
    float peakInRange (const std::vector<float>& peaks, int bin, double s0, double s1) const;

    EchoThrowProcessor& proc;

    std::shared_ptr<const juce::AudioBuffer<float>> src;
    std::shared_ptr<const RenderedClip> clip;
    std::vector<float> srcPeaks, wetPeaks, livePeaks;
    float peakMax = 0.001f;
    int liveSamplesDone = 0;

    double viewEnd = 48000.0 * 3.0;    // animated
    double viewTarget = viewEnd;
    bool animating = false;
    int dragHandle = 0;                // 1 = start, 2 = end
    CaptureEngine::State lastState = CaptureEngine::State::Idle;
    int lastPlayPos = -1;
    int cachedTrimStart = 0, cachedTrimEnd = 0;
    double sampleRate = 48000.0;
};

} // namespace et

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../PluginProcessor.h"

namespace et
{
/** Interactive 6-band EQ for the echoes: drag nodes (frequency / gain), wheel = Q, double-click = reset.
    The faint shape behind the curve is the spectrum of the current echoes (before the EQ). */
class EqView : public juce::Component
{
public:
    explicit EqView (EchoThrowProcessor&);
    ~EqView() override;

    void refresh();

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    enum Node { HP = 0, LS, B1, B2, HS, LP, NumNodes };

    struct Binding
    {
        const char* label = "";
        juce::Colour colour;
        juce::RangedAudioParameter *f = nullptr, *g = nullptr, *q = nullptr;
        std::unique_ptr<juce::ParameterAttachment> attF, attG, attQ;
    };

    juce::Rectangle<float> plotArea() const;
    float xForFreq (float f) const;
    float freqForX (float x) const;
    float yForGain (float db) const;
    float gainForY (float y) const;
    juce::Point<float> nodePosition (int n) const;
    int nodeAt (juce::Point<float> p) const;
    EqSettings readSettings() const;
    static float real (juce::RangedAudioParameter* p) { return p->convertFrom0to1 (p->getValue()); }

    EchoThrowProcessor& proc;
    std::array<Binding, NumNodes> nodes;
    std::atomic<float>* eqOn = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attOn;
    std::shared_ptr<const RenderedClip> clip;
    uint64_t shownClipId = 0;

    int hoverNode = -1, dragNode = -1;
    juce::Point<float> dragStartMouse, dragStartNode;
};

} // namespace et

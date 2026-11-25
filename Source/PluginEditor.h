#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "UI/Widgets.h"
#include "UI/WaveformView.h"
#include "UI/EqView.h"

namespace et
{
class EchoThrowEditor : public juce::AudioProcessorEditor,
                        private juce::Timer
{
public:
    explicit EchoThrowEditor (EchoThrowProcessor&);
    ~EchoThrowEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class Content;
    class Backdrop;
    class StatusText;

    void timerCallback() override;
    void toast (const juce::String&, int ms = 6000);
    void updateTimeKnobs();
    void onRecClicked();
    void onListenClicked();

    // presets
    juce::StringArray allPresetNames() const;
    void applyPresetByIndex (int index);
    void stepPreset (int delta);
    void showPresetMenu();
    void promptSavePreset();
    void showMainMenu();
    void setUiScale (float scale);

    EchoThrowProcessor& proc;

    std::unique_ptr<Content> content;
    std::unique_ptr<Backdrop> backdrop;
    std::unique_ptr<StatusText> status;

    PresetBar presetBar;
    IconButton saveButton { IconButton::Kind::Save }, menuButton { IconButton::Kind::Menu }, resetButton { IconButton::Kind::Reset, "RESET" };
    Segment modeSwitch;
    WaveformView wave;
    RecButton recButton;
    ListenButton listenButton;
    ParamToggle loopToggle;
    DragValue tailValue;
    DragPill dragPill;

    Knob timeKnob, timeRKnob;
    ParamToggle syncToggle, linkToggle, muteToggle, hearDryToggle, exportDryToggle;
    Knob feedbackKnob, widthKnob, driveKnob, modKnob, rateKnob, diffKnob, lowCutKnob, highCutKnob, levelKnob;
    EqView eqView;
    ParamToggle eqToggle;

    std::unique_ptr<juce::ParameterAttachment> syncAttachment, linkAttachment;

    bool syncOn = true, linkOn = true;
    bool constructed = false;
    bool lastReady = false;
    double lastBpm = 0.0;

    juce::String toastText;
    juce::uint32 toastUntil = 0;
    bool awaitingListenStart = false;
    juce::uint32 listenRequestedAt = 0;
    uint32_t heartbeatAtRequest = 0;
    CaptureEngine::State lastCaptureState = CaptureEngine::State::Idle;
    bool lastListening = false, lastHadClip = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoThrowEditor)
};

} // namespace et

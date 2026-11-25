#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Parameters.h"
#include "DSP/CaptureEngine.h"
#include "DSP/RenderManager.h"

namespace et
{
constexpr const char* kPluginName = "EchoThrow";
constexpr int kDesignWidth = 1000;
constexpr int kDesignHeight = 690;

class EchoThrowProcessor : public juce::AudioProcessor,
                           private juce::Timer,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    EchoThrowProcessor();
    ~EchoThrowProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return kPluginName; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    // ---- API used by the editor (message thread) ----
    juce::AudioProcessorValueTreeState apvts;

    CaptureEngine::State getCaptureState() const noexcept { return capture.getState(); }
    CaptureEngine& getCapture() noexcept { return capture; }
    void toggleRecord();                    // REC button
    void startListening();
    void stopListening();
    bool isListening() const noexcept { return playPosition.load() >= 0; }
    int  getPlayPosition() const noexcept { return playPosition.load(); }   // in samples of the rendered clip, -1 = stopped

    bool hasClip() const { const juce::ScopedLock sl (clipLock); return source != nullptr; }
    std::shared_ptr<const juce::AudioBuffer<float>> getSource() const { const juce::ScopedLock sl (clipLock); return source; }
    std::shared_ptr<const RenderedClip> getRenderedClip() const { return renderer.latest(); }
    bool isRendering() const noexcept { return renderer.isBusy(); }
    double getSampleRate() const noexcept { return currentSampleRate; }

    int getTrimStart() const { const juce::ScopedLock sl (clipLock); return trimStart; }
    int getTrimEnd() const { const juce::ScopedLock sl (clipLock); return trimEnd; }
    void setTrim (int start, int end);
    void resetTrim();
    void clearClip();

    /** Loads audio from a file (drag a WAV onto the window). Returns false if it can't be read. */
    bool loadClipFromFile (const juce::File& file);

    /** Writes the current render to a WAV file in <Documents>/EchoThrow/Exports and returns it (message thread). */
    juce::File exportCurrentClip();

    /** "BAR 12 BEAT 3.50" for a position (in samples) inside the source clip, or empty if the host didn't tell us. */
    juce::String describePosition (int sampleInSource) const;

    uint32_t getHeartbeat() const noexcept { return heartbeat.load(); }
    bool wasHostTempoSeen() const noexcept { return hostBpmValid.load(); }

    int uiWidth = kDesignWidth;   // remembered editor size
    juce::String presetName { "Default" };
    double getHostBpm() const noexcept { return hostBpm.load(); }

private:
    void timerCallback() override;
    void parameterChanged (const juce::String& id, float newValue) override;

    void pollCapture();
    void setSource (juce::AudioBuffer<float>&& audio, double rate, bool autoTrim);
    void resampleSourceIfNeeded();
    void requestRender();
    DelayParams makeDelayParams() const;
    void updateEncodedClip();

    //==========================================================================
    CaptureEngine capture;
    RenderManager renderer;

    // clip state - guarded by clipLock (message thread; the host may also call get/setStateInformation)
    mutable juce::CriticalSection clipLock;
    std::shared_ptr<const juce::AudioBuffer<float>> source;
    double sourceRate = 0.0;
    int trimStart = 0, trimEnd = 0;
    double captureStartPpq = std::nan (""), captureBpm = 0.0, captureBeatsPerBar = 4.0;
    juce::MemoryBlock encodedClip;
    bool encodedDirty = false;
    juce::File lastExportFile;
    uint64_t lastExportId = 0;
    bool lastExportDry = false;

    // cached raw parameter pointers (thread-safe atomics)
    struct Raw
    {
        std::atomic<float> *sync, *link, *timeL, *timeR, *divL, *divR, *feedback, *pingPong, *width, *lowCut, *highCut,
                           *drive, *mod, *modRate, *diffusion, *wetLevel, *tail, *hearDry, *exportDry, *muteLive, *loop,
                           *eqOn, *eqHp, *eqLp, *eqLsF, *eqLsG, *eq1F, *eq1G, *eq1Q, *eq2F, *eq2G, *eq2Q, *eqHsF, *eqHsG;
    } raw {};

    // host / transport
    double currentSampleRate = 0.0;
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<double> hostBeatsPerBar { 4.0 };
    std::atomic<bool> hostBpmValid { false };
    std::atomic<bool> paramsDirty { false };
    std::atomic<uint32_t> heartbeat { 0 };

    // playback (audio thread owns playing/pos/fade; others only post commands)
    std::atomic<int> playCommand { 0 };        // 1 = start, 2 = stop
    std::atomic<int> playPosition { -1 };
    bool playing = false;
    int playPos = 0;
    int fadeOutLeft = -1;
    static constexpr int kFadeOutSamples = 384;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoThrowProcessor)
};

} // namespace et

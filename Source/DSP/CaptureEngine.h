#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace et
{
/** Records a piece of the incoming audio.

    Click REC  -> Armed: a short pre-roll is kept, nothing is stored yet.
    First sample above the threshold -> Recording (the pre-roll is included, so the attack is never clipped).
    Click REC again -> Finished. The message thread then collects the take with takeResult().

    process() is real-time safe (no allocation, no locks). Everything else is for the message thread. */
class CaptureEngine
{
public:
    enum class State : int { Idle = 0, Armed = 1, Recording = 2, Finished = 3 };

    static constexpr double kMaxSeconds = 30.0;
    static constexpr double kPreRollSeconds = 0.12;

    void prepare (double sampleRate);   // allocates - not real-time safe

    // ---- message thread ----
    void arm() noexcept          { command.store ((int) Cmd::Arm); }
    void stop() noexcept         { command.store ((int) Cmd::Stop); }
    void cancel() noexcept       { command.store ((int) Cmd::Cancel); }
    State getState() const noexcept { return (State) state.load (std::memory_order_acquire); }
    double getRecordedSeconds() const noexcept;
    int  getRecordedSamples() const noexcept { return writePos.load (std::memory_order_acquire); }
    /** Peak (0..1) of the most recent samples, for the live meter. */
    float getRecentPeak() const noexcept { return recentPeak.load (std::memory_order_relaxed); }

    /** If a take has finished, copies it out and returns true. */
    bool collectTake (juce::AudioBuffer<float>& dest, double& startPpq, double& bpm);

    /** Copies the samples [from, to) recorded so far (used for the live waveform). Message thread only. */
    void peekRecorded (int from, int to, juce::AudioBuffer<float>& dest) const;

    // ---- audio thread ----
    /** ppqAtStart: host position at the first sample of this block, or NaN if unknown. */
    void process (const juce::AudioBuffer<float>& input, double ppqAtStart, double hostBpm) noexcept;

private:
    enum class Cmd : int { None = 0, Arm, Stop, Cancel };

    double sampleRate = 48000.0;
    int maxSamples = 0, preRollSamples = 0;

    juce::AudioBuffer<float> take;       // 2 x maxSamples
    juce::AudioBuffer<float> ring;       // 2 x preRollSamples
    int ringPos = 0, ringFilled = 0;

    std::atomic<int>  command { (int) Cmd::None };
    std::atomic<int>  state { (int) State::Idle };
    std::atomic<int>  writePos { 0 };
    std::atomic<float> recentPeak { 0.0f };
    std::atomic<bool> takeReady { false };

    double takeStartPpq = 0.0, takeBpm = 0.0;     // written by the audio thread before Finished is published
    static constexpr float kTriggerLevel = 0.003f;   // about -50 dBFS (above typical room noise)
};

} // namespace et

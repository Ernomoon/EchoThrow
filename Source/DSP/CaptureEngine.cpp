#include "CaptureEngine.h"
#include <cmath>

namespace et
{
void CaptureEngine::prepare (double sr)
{
    sampleRate = sr;
    maxSamples = (int) std::ceil (kMaxSeconds * sr);
    preRollSamples = (int) std::ceil (kPreRollSeconds * sr);
    take.setSize (2, maxSamples, false, true, false);
    ring.setSize (2, preRollSamples, false, true, false);
    ringPos = ringFilled = 0;
    writePos.store (0);
    state.store ((int) State::Idle);
    command.store ((int) Cmd::None);
    takeReady.store (false);
}

double CaptureEngine::getRecordedSeconds() const noexcept
{
    return sampleRate > 0 ? (double) writePos.load (std::memory_order_acquire) / sampleRate : 0.0;
}

bool CaptureEngine::collectTake (juce::AudioBuffer<float>& dest, double& startPpq, double& bpm)
{
    if (! takeReady.load (std::memory_order_acquire))
        return false;

    const int n = writePos.load (std::memory_order_acquire);
    dest.setSize (2, std::max (1, n), false, false, true);
    dest.clear();
    for (int ch = 0; ch < 2; ++ch)
        dest.copyFrom (ch, 0, take, ch, 0, n);
    startPpq = takeStartPpq;
    bpm = takeBpm;

    takeReady.store (false, std::memory_order_release);
    state.store ((int) State::Idle, std::memory_order_release);
    return true;
}

void CaptureEngine::peekRecorded (int from, int to, juce::AudioBuffer<float>& dest) const
{
    const int n = std::min (to, writePos.load (std::memory_order_acquire));
    from = std::max (0, from);
    const int len = std::max (0, n - from);
    dest.setSize (2, std::max (1, len), false, false, true);
    dest.clear();
    for (int ch = 0; ch < 2; ++ch)
        if (len > 0) dest.copyFrom (ch, 0, take, ch, from, len);
}

void CaptureEngine::process (const juce::AudioBuffer<float>& input, double ppqAtStart, double hostBpm) noexcept
{
    const int numSamples = input.getNumSamples();
    const int inCh = input.getNumChannels();
    if (numSamples == 0 || inCh == 0 || maxSamples == 0)
        return;

    const float* inL = input.getReadPointer (0);
    const float* inR = input.getReadPointer (inCh > 1 ? 1 : 0);

    // ---- commands from the message thread ----
    State s = (State) state.load (std::memory_order_acquire);
    switch ((Cmd) command.exchange ((int) Cmd::None))
    {
        case Cmd::Arm:
            if (s == State::Idle)
            {
                ringPos = ringFilled = 0;
                writePos.store (0, std::memory_order_release);
                s = State::Armed;
                state.store ((int) s, std::memory_order_release);
            }
            break;
        case Cmd::Stop:
            if (s == State::Recording)
            {
                takeReady.store (true, std::memory_order_release);
                s = State::Finished;
                state.store ((int) s, std::memory_order_release);
            }
            else if (s == State::Armed)
            {
                s = State::Idle;
                state.store ((int) s, std::memory_order_release);
            }
            break;
        case Cmd::Cancel:
            if (s == State::Armed || s == State::Recording)
            {
                s = State::Idle;
                writePos.store (0, std::memory_order_release);
                state.store ((int) s, std::memory_order_release);
            }
            break;
        case Cmd::None: break;
    }

    float peak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        peak = std::max (peak, std::max (std::abs (inL[i]), std::abs (inR[i])));
    recentPeak.store (peak, std::memory_order_relaxed);

    if (s == State::Armed)
    {
        float* rl = ring.getWritePointer (0);
        float* rr = ring.getWritePointer (1);

        for (int i = 0; i < numSamples; ++i)
        {
            // keep the pre-roll ring up to date
            rl[ringPos] = inL[i];
            rr[ringPos] = inR[i];
            ringPos = (ringPos + 1) % preRollSamples;
            ringFilled = std::min (ringFilled + 1, preRollSamples);

            if (std::max (std::abs (inL[i]), std::abs (inR[i])) >= kTriggerLevel)
            {
                // trigger: copy the pre-roll (which already ends with this sample) into the take
                float* tl = take.getWritePointer (0);
                float* tr = take.getWritePointer (1);
                const int pre = ringFilled;
                int rp = (ringPos - pre + preRollSamples) % preRollSamples;
                for (int k = 0; k < pre; ++k)
                {
                    tl[k] = rl[rp];
                    tr[k] = rr[rp];
                    rp = (rp + 1) % preRollSamples;
                }
                int pos = pre;

                // remember where the take starts on the host timeline (for the "bar / beat" hint)
                if (std::isfinite (ppqAtStart) && hostBpm > 0.0)
                {
                    const double ppqPerSample = hostBpm / 60.0 / sampleRate;
                    takeStartPpq = ppqAtStart + (double) (i + 1 - pre) * ppqPerSample;
                    takeBpm = hostBpm;
                }
                else
                {
                    takeStartPpq = std::nan ("");
                    takeBpm = 0.0;
                }

                // the rest of this block
                const int rest = numSamples - (i + 1);
                const int n = std::min (rest, maxSamples - pos);
                for (int k = 0; k < n; ++k)
                {
                    tl[pos + k] = inL[i + 1 + k];
                    tr[pos + k] = inR[i + 1 + k];
                }
                pos += n;
                writePos.store (pos, std::memory_order_release);
                s = State::Recording;
                state.store ((int) s, std::memory_order_release);

                if (pos >= maxSamples)
                {
                    takeReady.store (true, std::memory_order_release);
                    state.store ((int) State::Finished, std::memory_order_release);
                }
                return;
            }
        }
        return;
    }

    if (s == State::Recording)
    {
        int pos = writePos.load (std::memory_order_relaxed);
        const int n = std::min (numSamples, maxSamples - pos);
        float* tl = take.getWritePointer (0);
        float* tr = take.getWritePointer (1);
        for (int k = 0; k < n; ++k)
        {
            tl[pos + k] = inL[k];
            tr[pos + k] = inR[k];
        }
        pos += n;
        writePos.store (pos, std::memory_order_release);

        if (pos >= maxSamples)      // 30 seconds reached: finish automatically
        {
            takeReady.store (true, std::memory_order_release);
            state.store ((int) State::Finished, std::memory_order_release);
        }
    }
}

} // namespace et

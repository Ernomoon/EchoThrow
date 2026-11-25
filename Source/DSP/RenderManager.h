#pragma once

#include <juce_core/juce_core.h>
#include <memory>
#include <mutex>
#include <atomic>
#include <vector>
#include "DelayEngine.h"

namespace et
{
/** Re-renders the clip on a background thread whenever something changes, and hands finished renders to the
    audio thread without locks or allocation.

    Only the newest request is ever rendered; a render that is still running is aborted when a newer one arrives,
    so dragging a knob stays smooth even for long clips. */
class RenderManager : private juce::Thread
{
public:
    RenderManager();
    ~RenderManager() override;

    struct Request
    {
        std::shared_ptr<const juce::AudioBuffer<float>> source;   // null = "no clip": clears the output
        int start = 0, end = 0;
        DelayParams params;
    };

    /** Message thread (or any non-audio thread). */
    void submit (Request request);

    /** Message thread: the newest finished render, or null. The clip stays valid as long as you hold the pointer. */
    std::shared_ptr<const RenderedClip> latest() const;

    bool isBusy() const noexcept { return busy.load(); }

    /** Audio thread: returns the newest clip (or null) and keeps it alive until release(). */
    const RenderedClip* acquire() noexcept;
    void release() noexcept;

private:
    void run() override;
    void publish (std::shared_ptr<RenderedClip> clip);

    juce::CriticalSection requestLock;
    Request pending;
    bool hasPending = false;
    std::atomic<uint64_t> generation { 0 };
    std::atomic<bool> busy { false };

    mutable std::mutex clipsMutex;
    std::vector<std::shared_ptr<RenderedClip>> alive;
    std::shared_ptr<RenderedClip> latestClip;
    uint64_t nextId = 1;

    std::atomic<const RenderedClip*> published { nullptr };
    std::atomic<const RenderedClip*> inUse { nullptr };
};

} // namespace et

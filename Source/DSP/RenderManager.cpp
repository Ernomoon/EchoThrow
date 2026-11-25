#include "RenderManager.h"

namespace et
{
RenderManager::RenderManager() : juce::Thread ("EchoThrow render")
{
    startThread (juce::Thread::Priority::normal);
}

RenderManager::~RenderManager()
{
    signalThreadShouldExit();
    notify();
    stopThread (5000);
}

void RenderManager::submit (Request request)
{
    {
        const juce::ScopedLock sl (requestLock);
        pending = std::move (request);
        hasPending = true;
        generation.fetch_add (1);
    }
    notify();
}

std::shared_ptr<const RenderedClip> RenderManager::latest() const
{
    std::lock_guard<std::mutex> lock (clipsMutex);
    return latestClip;
}

const RenderedClip* RenderManager::acquire() noexcept
{
    // Hazard-pointer style hand-over: announce what we are about to use, then make sure it is still current.
    const RenderedClip* c = published.load (std::memory_order_seq_cst);
    inUse.store (c, std::memory_order_seq_cst);
    const RenderedClip* again = published.load (std::memory_order_seq_cst);
    if (again != c)
    {
        c = again;
        inUse.store (c, std::memory_order_seq_cst);
    }
    return c;
}

void RenderManager::release() noexcept
{
    inUse.store (nullptr, std::memory_order_seq_cst);
}

void RenderManager::publish (std::shared_ptr<RenderedClip> clip)
{
    std::vector<std::shared_ptr<RenderedClip>> toFree;   // destroyed outside the lock
    {
        std::lock_guard<std::mutex> lock (clipsMutex);
        if (clip) alive.push_back (clip);
        latestClip = clip;
        published.store (clip.get(), std::memory_order_seq_cst);

        const RenderedClip* busyClip = inUse.load (std::memory_order_seq_cst);
        for (auto it = alive.begin(); it != alive.end();)
        {
            if (*it != latestClip && it->get() != busyClip)
            {
                toFree.push_back (std::move (*it));
                it = alive.erase (it);
            }
            else
                ++it;
        }
    }
}

void RenderManager::run()
{
    while (! threadShouldExit())
    {
        wait (-1);
        if (threadShouldExit())
            break;

        for (;;)
        {
            Request req;
            uint64_t gen = 0;
            {
                const juce::ScopedLock sl (requestLock);
                if (! hasPending)
                    break;
                req = std::move (pending);
                pending = {};
                hasPending = false;
                gen = generation.load();
            }

            busy.store (true);

            if (! req.source)
            {
                publish (nullptr);
            }
            else
            {
                auto clip = std::make_shared<RenderedClip>();
                const bool ok = DelayEngine::render (*req.source, req.start, req.end, req.params, *clip,
                                                     [this, gen] { return threadShouldExit() || generation.load() != gen; });
                if (ok)
                {
                    clip->id = nextId++;
                    publish (std::move (clip));
                }
            }

            busy.store (false);
        }
    }
}

} // namespace et

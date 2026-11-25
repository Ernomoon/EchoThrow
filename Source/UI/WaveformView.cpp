#include "WaveformView.h"
#include "Theme.h"

namespace et
{
using namespace theme;

WaveformView::WaveformView (EchoThrowProcessor& p) : proc (p) {}

void WaveformView::computePeaks (const juce::AudioBuffer<float>& buf, int numSamples, std::vector<float>& out) const
{
    const int bins = (numSamples + kBin - 1) / kBin;
    out.assign ((size_t) std::max (0, bins), 0.0f);
    for (int ch = 0; ch < std::min (2, buf.getNumChannels()); ++ch)
    {
        const float* d = buf.getReadPointer (ch);
        for (int b = 0; b < bins; ++b)
        {
            const int s0 = b * kBin, s1 = std::min (numSamples, s0 + kBin);
            float m = out[(size_t) b];
            for (int i = s0; i < s1; ++i) m = std::max (m, std::abs (d[i]));
            out[(size_t) b] = m;
        }
    }
}

float WaveformView::peakInRange (const std::vector<float>& peaks, int bin, double s0, double s1) const
{
    if (peaks.empty() || s1 < 0.0) return 0.0f;
    int b0 = (int) std::floor (std::max (0.0, s0) / bin);
    int b1 = (int) std::floor (std::max (0.0, s1) / bin);
    if (b0 >= (int) peaks.size()) return 0.0f;
    b1 = std::min (b1, (int) peaks.size() - 1);
    float m = 0.0f;
    for (int b = b0; b <= b1; ++b) m = std::max (m, peaks[(size_t) b]);
    return m;
}

//==============================================================================
void WaveformView::refresh()
{
    bool needsRepaint = false;
    const auto state = proc.getCaptureState();
    sampleRate = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;

    auto newSrc = proc.getSource();
    if (newSrc != src)
    {
        src = newSrc;
        if (src) computePeaks (*src, src->getNumSamples(), srcPeaks); else srcPeaks.clear();
        needsRepaint = true;
    }

    auto newClip = proc.getRenderedClip();
    if (newClip != clip)
    {
        clip = newClip;
        if (clip) computePeaks (clip->wet, clip->outLen, wetPeaks); else wetPeaks.clear();
        needsRepaint = true;
    }

    if (needsRepaint || peakMax <= 0.0011f)
    {
        float m = 0.0f;
        for (float v : srcPeaks) m = std::max (m, v);
        for (float v : wetPeaks) m = std::max (m, v);
        peakMax = std::max (0.001f, m);
    }

    // live take
    if (state == CaptureEngine::State::Recording || state == CaptureEngine::State::Armed)
    {
        const int n = proc.getCapture().getRecordedSamples();
        if (n < liveSamplesDone) { livePeaks.clear(); liveSamplesDone = 0; }
        if (n - liveSamplesDone >= kLiveBin)
        {
            juce::AudioBuffer<float> tmp;
            const int upTo = liveSamplesDone + ((n - liveSamplesDone) / kLiveBin) * kLiveBin;
            proc.getCapture().peekRecorded (liveSamplesDone, upTo, tmp);
            for (int s = 0; s + kLiveBin <= tmp.getNumSamples(); s += kLiveBin)
            {
                float m = 0.0f;
                for (int ch = 0; ch < 2; ++ch)
                    m = std::max (m, tmp.findMinMax (ch, s, kLiveBin).getEnd());
                for (int ch = 0; ch < 2; ++ch)
                    m = std::max (m, -tmp.findMinMax (ch, s, kLiveBin).getStart());
                livePeaks.push_back (m);
            }
            liveSamplesDone = upTo;
        }
        needsRepaint = true;
    }
    else if (! livePeaks.empty() && src != nullptr)
    {
        livePeaks.clear();
        liveSamplesDone = 0;
        needsRepaint = true;
    }

    // view range
    const int trimStart = proc.getTrimStart(), trimEnd = proc.getTrimEnd();
    if (trimStart != cachedTrimStart || trimEnd != cachedTrimEnd)
    {
        cachedTrimStart = trimStart;
        cachedTrimEnd = trimEnd;
        needsRepaint = true;
    }

    double target = sampleRate * 3.0;
    if (state == CaptureEngine::State::Recording)
        target = std::max (target, (double) liveSamplesDone * 1.1);
    else if (src)
    {
        target = (double) src->getNumSamples();
        if (clip) target = std::max (target, (double) (trimStart + clip->outLen));
        target = std::max (target, sampleRate * 0.5);
    }
    viewTarget = target * 1.02;

    if (std::abs (viewTarget - viewEnd) > 1.0)
    {
        viewEnd += (viewTarget - viewEnd) * 0.28;
        if (std::abs (viewTarget - viewEnd) < sampleRate * 0.002) viewEnd = viewTarget;
        animating = true;
        needsRepaint = true;
    }
    else
        animating = false;

    const int pp = proc.getPlayPosition();
    if (pp != lastPlayPos) { lastPlayPos = pp; needsRepaint = true; }
    if (state != lastState) { lastState = state; needsRepaint = true; }
    if (state == CaptureEngine::State::Armed) needsRepaint = true;   // pulsing text

    if (needsRepaint)
        repaint();
}

//==============================================================================
juce::Rectangle<float> WaveformView::waveArea() const
{
    return getLocalBounds().toFloat().reduced (10.0f, 6.0f).withTrimmedBottom (16.0f);
}

float WaveformView::xForSample (double s) const
{
    const auto a = waveArea();
    return a.getX() + (float) (s / viewEnd) * a.getWidth();
}

double WaveformView::sampleForX (float x) const
{
    const auto a = waveArea();
    return (double) ((x - a.getX()) / a.getWidth()) * viewEnd;
}

int WaveformView::hitHandle (float x) const
{
    if (! src || proc.getCaptureState() != CaptureEngine::State::Idle) return 0;
    const float xs = xForSample (cachedTrimStart), xe = xForSample (cachedTrimEnd);
    const float ds = std::abs (x - xs), de = std::abs (x - xe);
    if (ds <= 9.0f && ds <= de) return 1;
    if (de <= 9.0f) return 2;
    return 0;
}

void WaveformView::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (hitHandle (e.position.x) != 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    dragHandle = hitHandle (e.position.x);
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragHandle == 0 || ! src) return;
    const double s = juce::jlimit (0.0, (double) src->getNumSamples(), sampleForX (e.position.x));
    if (dragHandle == 1) proc.setTrim ((int) s, proc.getTrimEnd());
    else                 proc.setTrim (proc.getTrimStart(), (int) s);
}

void WaveformView::mouseDoubleClick (const juce::MouseEvent&)
{
    if (src && proc.getCaptureState() == CaptureEngine::State::Idle)
        proc.resetTrim();
}

//==============================================================================
void WaveformView::paint (juce::Graphics& g)
{
    const auto area = waveArea();
    const float cy = area.getCentreY();
    const float halfH = area.getHeight() * 0.5f - 2.0f;
    const auto state = proc.getCaptureState();
    const bool recordingView = state == CaptureEngine::State::Recording || state == CaptureEngine::State::Armed;

    constexpr float pitch = 3.0f, barW = 2.0f;
    const int numBars = std::max (1, (int) (area.getWidth() / pitch));
    const double perBar = viewEnd / (double) numBars;
    const float scale = 1.0f / peakMax;

    auto barHeight = [&] (float peak)
    {
        if (peak < 0.0004f) return 0.0f;
        return std::pow (std::min (1.0f, peak * scale), 0.6f) * halfH * 0.96f;
    };

    // ---- selection band + centre line ----
    if (src && ! recordingView)
    {
        const float xs = xForSample (cachedTrimStart), xe = xForSample (cachedTrimEnd);
        g.setColour (cyan.withAlpha (0.045f));
        g.fillRect (juce::Rectangle<float> (xs, area.getY(), xe - xs, area.getHeight()));
    }
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillRect (area.getX(), cy - 0.5f, area.getWidth(), 1.0f);

    // ---- bars ----
    juce::Path wetBars, wetGlow, dryBars, dryGlow, dryDim;
    auto addBar = [&] (juce::Path& p, float x, float h, float grow)
    {
        if (h <= 0.0f) return;
        p.addRoundedRectangle (x - grow, cy - h - grow, barW + grow * 2.0f, h * 2.0f + grow * 2.0f, (barW + grow * 2.0f) * 0.5f);
    };

    if (recordingView)
    {
        const double liveView = std::max (sampleRate * 3.0, (double) liveSamplesDone * 1.1);
        const double perLiveBar = liveView / (double) numBars;
        for (int k = 0; k < numBars; ++k)
        {
            const double s0 = (double) k * perLiveBar, s1 = s0 + perLiveBar;
            const float peak = peakInRange (livePeaks, kLiveBin, s0, s1);
            const float x = area.getX() + (float) k * pitch;
            const float h = barHeight (peak * 4.0f * peakMax);   // live view is auto-gained so a quiet vocal is still visible
            addBar (dryBars, x, h, 0.0f);
            addBar (dryGlow, x, h, 2.0f);
        }
    }
    else if (src)
    {
        for (int k = 0; k < numBars; ++k)
        {
            const double s0 = (double) k * perBar, s1 = s0 + perBar;
            const float x = area.getX() + (float) k * pitch;

            if (clip)
            {
                const float wp = peakInRange (wetPeaks, kBin, s0 - cachedTrimStart, s1 - cachedTrimStart);
                const float h = barHeight (wp);
                addBar (wetBars, x, h, 0.0f);
                addBar (wetGlow, x, h, 2.2f);
            }

            const float sp = peakInRange (srcPeaks, kBin, s0, s1);
            const float h = barHeight (sp);
            const bool inside = s1 > cachedTrimStart && s0 < cachedTrimEnd;
            if (inside) { addBar (dryBars, x, h, 0.0f); addBar (dryGlow, x, h, 2.0f); }
            else        addBar (dryDim, x, h, 0.0f);
        }
    }
    else
    {
        // empty: a row of faint dots, like an idle meter
        for (int k = 0; k < numBars; k += 2)
        {
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.fillRect (area.getX() + (float) k * pitch, cy - 0.5f, barW, 1.0f);
        }
    }

    g.setColour (amber.withAlpha (0.10f));  g.fillPath (wetGlow);
    g.setColour (amber.withAlpha (0.92f));  g.fillPath (wetBars);
    g.setColour (cyan.withAlpha (0.12f));   g.fillPath (dryGlow);
    g.setColour (cyan.withAlpha (0.28f));   g.fillPath (dryDim);
    g.setColour (cyan);                     g.fillPath (dryBars);

    // ---- trim handles ----
    if (src && ! recordingView)
    {
        auto drawHandle = [&] (double sample, bool isStart)
        {
            const float x = xForSample (sample);
            g.setColour (cyan.withAlpha (0.18f));
            g.fillRect (x - 2.0f, area.getY(), 4.0f, area.getHeight());
            g.setColour (cyan.withAlpha (0.85f));
            g.fillRect (x - 0.75f, area.getY(), 1.5f, area.getHeight());
            const float dir = isStart ? 1.0f : -1.0f;
            juce::Path grip;
            grip.addRoundedRectangle (x - (isStart ? 1.5f : 6.5f), cy - 13.0f, 8.0f, 26.0f, 3.0f);
            g.setColour (juce::Colour (0xff16161a));
            g.fillPath (grip);
            g.setColour (cyan);
            g.strokePath (grip, juce::PathStrokeType (1.2f));
            g.setColour (cyan.withAlpha (0.8f));
            g.fillRect (x + dir * 2.5f - 0.5f, cy - 5.0f, 1.0f, 10.0f);
        };
        drawHandle ((double) cachedTrimStart, true);
        drawHandle ((double) cachedTrimEnd, false);
    }

    // ---- play-head ----
    if (lastPlayPos >= 0 && clip)
    {
        const float x = xForSample ((double) (cachedTrimStart + lastPlayPos));
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.fillRect (x - 3.0f, area.getY(), 6.0f, area.getHeight());
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.fillRect (x - 0.75f, area.getY(), 1.5f, area.getHeight());
    }

    // ---- ruler ----
    {
        const double viewSeconds = (recordingView ? std::max (3.0, (double) liveSamplesDone * 1.1 / sampleRate) : viewEnd / sampleRate);
        static const double steps[] = { 0.1, 0.25, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0 };
        double step = steps[7];
        for (double s : steps)
            if (area.getWidth() * s / viewSeconds >= 64.0) { step = s; break; }

        g.setFont (font (9.5f, false, 0.06f));
        for (double t = 0.0; t <= viewSeconds; t += step)
        {
            const float x = area.getX() + (float) (t / viewSeconds) * area.getWidth();
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.fillRect (x, area.getBottom() + 3.0f, 1.0f, 4.0f);
            g.setColour (textDim);
            g.drawText (juce::String (t, step >= 1.0 ? 0 : (step >= 0.5 ? 1 : 2)) + "s",
                        juce::Rectangle<float> (x + 3.0f, area.getBottom() + 2.0f, 50.0f, 12.0f), juce::Justification::centredLeft, false);
        }
    }

    // ---- overlays ----
    if (state == CaptureEngine::State::Armed)
    {
        const bool pulse = ((juce::Time::getMillisecondCounter() / 450) % 2) == 0;
        drawCapsText (g, "WAITING FOR AUDIO  -  PRESS PLAY IN YOUR DAW", area, amber.withAlpha (pulse ? 1.0f : 0.5f), 12.0f,
                      juce::Justification::centred, true, 0.2f);
    }
    else if (! src && state == CaptureEngine::State::Idle)
    {
        drawCapsText (g, "PRESS REC, THEN PLAY YOUR VOCAL", area.translated (0, -9.0f), textMid, 12.0f, juce::Justification::centred, true, 0.2f);
        drawCapsText (g, "or drop an audio file here", area.translated (0, 11.0f), textDim, 10.5f, juce::Justification::centred, false, 0.1f);
    }
}

} // namespace et

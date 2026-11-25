#include "EqView.h"
#include "Theme.h"

namespace et
{
using namespace theme;

namespace
{
    constexpr float kMinHz = 20.0f, kMaxHz = 20000.0f, kRangeDb = 15.0f, kInset = 12.0f;
    constexpr double kDisplayRate = 48000.0;
}

EqView::EqView (EchoThrowProcessor& p) : proc (p)
{
    auto& ap = proc.apvts;
    auto get = [&] (const char* id) { return static_cast<juce::RangedAudioParameter*> (ap.getParameter (id)); };

    auto setup = [&] (int n, const char* label, juce::Colour colour, const char* f, const char* g, const char* q)
    {
        auto& nd = nodes[(size_t) n];
        nd.label = label;
        nd.colour = colour;
        nd.f = get (f);
        nd.g = g != nullptr ? get (g) : nullptr;
        nd.q = q != nullptr ? get (q) : nullptr;
    };
    const juce::Colour grey (0xffd0d0d8);
    setup (HP, "LOW CUT",    grey,   pid::eqHp,  nullptr,    nullptr);
    setup (LS, "LOW SHELF",  blue,   pid::eqLsF, pid::eqLsG, nullptr);
    setup (B1, "BELL 1",     cyan,   pid::eq1F,  pid::eq1G,  pid::eq1Q);
    setup (B2, "BELL 2",     violet, pid::eq2F,  pid::eq2G,  pid::eq2Q);
    setup (HS, "HIGH SHELF", amber,  pid::eqHsF, pid::eqHsG, nullptr);
    setup (LP, "HIGH CUT",   grey,   pid::eqLp,  nullptr,    nullptr);

    for (auto& n : nodes)
    {
        auto make = [this] (juce::RangedAudioParameter* prm) -> std::unique_ptr<juce::ParameterAttachment>
        {
            if (prm == nullptr) return nullptr;
            auto a = std::make_unique<juce::ParameterAttachment> (*prm, [this] (float) { repaint(); });
            a->sendInitialUpdate();
            return a;
        };
        n.attF = make (n.f);
        n.attG = make (n.g);
        n.attQ = make (n.q);
    }
    eqOn = ap.getRawParameterValue (pid::eqOn);
    attOn = std::make_unique<juce::ParameterAttachment> (*ap.getParameter (pid::eqOn), [this] (float) { repaint(); });
}

EqView::~EqView() = default;

void EqView::refresh()
{
    auto c = proc.getRenderedClip();
    const uint64_t id = c ? c->id : 0;
    if (id != shownClipId)
    {
        clip = c;
        shownClipId = id;
        repaint();
    }
}

//==============================================================================
juce::Rectangle<float> EqView::plotArea() const
{
    return getLocalBounds().toFloat().reduced (12.0f, 10.0f).withTrimmedBottom (14.0f);
}

float EqView::xForFreq (float f) const
{
    const auto a = plotArea();
    return a.getX() + kInset + std::log (std::max (kMinHz, f) / kMinHz) / std::log (kMaxHz / kMinHz) * (a.getWidth() - 2.0f * kInset);
}

float EqView::freqForX (float x) const
{
    const auto a = plotArea();
    const float t = juce::jlimit (0.0f, 1.0f, (x - a.getX() - kInset) / (a.getWidth() - 2.0f * kInset));
    return kMinHz * std::pow (kMaxHz / kMinHz, t);
}

float EqView::yForGain (float db) const
{
    const auto a = plotArea();
    return a.getCentreY() - db / kRangeDb * (a.getHeight() * 0.5f);
}

float EqView::gainForY (float y) const
{
    const auto a = plotArea();
    return juce::jlimit (-kRangeDb, kRangeDb, (a.getCentreY() - y) / (a.getHeight() * 0.5f) * kRangeDb);
}

juce::Point<float> EqView::nodePosition (int n) const
{
    const auto a = plotArea();
    const float x = xForFreq (real (nodes[(size_t) n].f));
    if (n == HP || n == LP)
        return { x, a.getY() + 30.0f };
    return { x, yForGain (real (nodes[(size_t) n].g)) };
}

int EqView::nodeAt (juce::Point<float> p) const
{
    int best = -1;
    float bestDist = 14.0f;
    for (int n = 0; n < NumNodes; ++n)
    {
        const float d = nodePosition (n).getDistanceFrom (p);
        if (d < bestDist) { bestDist = d; best = n; }
    }
    return best;
}

EqSettings EqView::readSettings() const
{
    EqSettings s;
    s.on = true;
    s.hpF = real (nodes[HP].f);  s.lpF = real (nodes[LP].f);
    s.lsF = real (nodes[LS].f);  s.lsG = real (nodes[LS].g);
    s.b1F = real (nodes[B1].f);  s.b1G = real (nodes[B1].g);  s.b1Q = real (nodes[B1].q);
    s.b2F = real (nodes[B2].f);  s.b2G = real (nodes[B2].g);  s.b2Q = real (nodes[B2].q);
    s.hsF = real (nodes[HS].f);  s.hsG = real (nodes[HS].g);
    return s;
}

//==============================================================================
void EqView::paint (juce::Graphics& g)
{
    const auto a = plotArea();
    const bool on = eqOn != nullptr && eqOn->load() > 0.5f;
    const float fade = on ? 1.0f : 0.35f;

    // ---- grid ----
    const int freqs[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    g.setFont (font (9.0f, false, 0.05f));
    for (int fi : freqs)
    {
        const float f = (float) fi;
        const float x = xForFreq (f);
        const bool major = (fi == 100 || fi == 1000 || fi == 10000);
        g.setColour (juce::Colours::white.withAlpha (major ? 0.075f : 0.035f));
        g.fillRect (x, a.getY(), 1.0f, a.getHeight());
        g.setColour (textDim);
        const juce::String t = fi >= 1000 ? juce::String (fi / 1000) + "k" : juce::String (fi);
        g.drawText (t, juce::Rectangle<float> (x - 18.0f, a.getBottom() + 1.0f, 36.0f, 13.0f), juce::Justification::centred, false);
    }
    for (int db = -12; db <= 12; db += 6)
    {
        const float y = yForGain ((float) db);
        g.setColour (juce::Colours::white.withAlpha (db == 0 ? 0.10f : 0.035f));
        g.fillRect (a.getX(), y, a.getWidth(), 1.0f);
        if (db != 0)
        {
            g.setColour (textDim.withAlpha (0.8f));
            g.drawText ((db > 0 ? "+" : "") + juce::String (db), juce::Rectangle<float> (a.getX() + 2.0f, y - 12.0f, 26.0f, 11.0f),
                        juce::Justification::centredLeft, false);
        }
    }

    // ---- spectrum of the echoes ----
    if (clip)
    {
        juce::Path sp;
        const float base = a.getBottom();
        sp.startNewSubPath (a.getX(), base);
        for (int i = 0; i < kSpectrumPoints; ++i)
        {
            const float f = kMinHz * std::pow (kMaxHz / kMinHz, ((float) i + 0.5f) / (float) kSpectrumPoints);
            const float h = juce::jlimit (0.0f, 1.0f, 1.0f + clip->spectrumDb[(size_t) i] / 54.0f);
            sp.lineTo (xForFreq (f), base - h * a.getHeight() * 0.62f);
        }
        sp.lineTo (a.getRight(), base);
        sp.closeSubPath();
        g.setGradientFill (juce::ColourGradient (cyan.withAlpha (0.20f), 0, a.getBottom() - a.getHeight() * 0.62f,
                                                 cyan.withAlpha (0.0f), 0, a.getBottom(), false));
        g.fillPath (sp);
        g.setColour (cyan.withAlpha (0.35f));
        g.strokePath (sp, juce::PathStrokeType (1.0f));
    }

    // ---- response curve ----
    const auto settings = readSettings();
    juce::Path curve, fill;
    const float y0 = yForGain (0.0f);
    bool first = true;
    for (float x = a.getX(); x <= a.getRight() + 0.1f; x += 2.0f)
    {
        const double db = EqChain::responseDb (settings, kDisplayRate, (double) freqForX (x));
        const float y = juce::jlimit (a.getY() - 2.0f, a.getBottom() + 2.0f, yForGain ((float) db));
        if (first) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, y0); fill.lineTo (x, y); first = false; }
        else       { curve.lineTo (x, y); fill.lineTo (x, y); }
    }
    fill.lineTo (a.getRight(), y0);
    fill.closeSubPath();

    g.saveState();
    g.reduceClipRegion (a.getSmallestIntegerContainer().expanded (0, 2));
    g.setColour (amber.withAlpha (0.10f * fade));
    g.fillPath (fill);
    drawGlow (g, curve, juce::Colour (0xfff2e6d6).withAlpha (fade), 1.4f, 3.5f);
    g.restoreState();

    // ---- nodes ----
    for (int n = 0; n < NumNodes; ++n)
    {
        const auto& nd = nodes[(size_t) n];
        const auto p = nodePosition (n);
        const bool cut = (n == HP || n == LP);
        const bool active = (n == hoverNode || n == dragNode);
        const bool neutral = ! cut && std::abs (real (nd.g)) < 0.05f;
        const bool cutOff = (n == HP && real (nd.f) <= 20.5f) || (n == LP && real (nd.f) >= 19900.0f);

        if (cut)
        {
            g.setColour (nd.colour.withAlpha (0.18f * fade));
            const float yEnd = a.getY() + 30.0f;
            for (float y = yEnd; y < a.getBottom(); y += 6.0f)
                g.fillRect (p.x - 0.5f, y, 1.0f, 3.0f);
        }

        const float r = active ? 7.5f : (neutral || cutOff ? 5.0f : 6.0f);
        if (active || ! (neutral || cutOff))
        {
            g.setColour (nd.colour.withAlpha ((active ? 0.35f : 0.2f) * fade));
            g.fillEllipse (p.x - r * 2.0f, p.y - r * 2.0f, r * 4.0f, r * 4.0f);
        }
        g.setColour (juce::Colour (0xff111114));
        g.fillEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f);
        g.setColour (nd.colour.withAlpha ((neutral || cutOff ? 0.55f : 1.0f) * fade));
        g.drawEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f, 1.8f);
        if (! (neutral || cutOff))
        {
            g.setColour (nd.colour.withAlpha (fade));
            g.fillEllipse (p.x - 2.0f, p.y - 2.0f, 4.0f, 4.0f);
        }
    }

    // ---- read-out ----
    const int shown = dragNode >= 0 ? dragNode : hoverNode;
    juce::String line;
    if (shown >= 0)
    {
        const auto& nd = nodes[(size_t) shown];
        line = juce::String (nd.label) + "   " + formatHz (real (nd.f));
        if (nd.g != nullptr) line << "   " << formatDb (real (nd.g));
        if (nd.q != nullptr) line << "   Q " << juce::String (real (nd.q), 2);
        drawCapsText (g, line, a.withTrimmedLeft (30.0f).withHeight (16.0f).translated (0, 2.0f), nodes[(size_t) shown].colour, 10.5f,
                      juce::Justification::centredLeft, true, 0.12f);
    }
    else
    {
        drawCapsText (g, "DRAG NODES   WHEEL = Q   DOUBLE-CLICK = RESET", a.withTrimmedLeft (30.0f).withHeight (16.0f).translated (0, 2.0f),
                      textDim.withAlpha (0.8f), 9.5f, juce::Justification::centredLeft, false, 0.1f);
    }
}

//==============================================================================
void EqView::mouseMove (const juce::MouseEvent& e)
{
    const int n = nodeAt (e.position);
    if (n != hoverNode)
    {
        hoverNode = n;
        setMouseCursor (n >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void EqView::mouseExit (const juce::MouseEvent&)
{
    hoverNode = -1;
    repaint();
}

void EqView::mouseDown (const juce::MouseEvent& e)
{
    dragNode = nodeAt (e.position);
    if (dragNode < 0) return;
    auto& nd = nodes[(size_t) dragNode];
    dragStartMouse = e.position;
    dragStartNode = nodePosition (dragNode);
    if (nd.attF) nd.attF->beginGesture();
    if (nd.attG) nd.attG->beginGesture();
    repaint();
}

void EqView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragNode < 0) return;
    auto& nd = nodes[(size_t) dragNode];
    const float k = e.mods.isShiftDown() ? 0.2f : 1.0f;
    const auto target = dragStartNode + (e.position - dragStartMouse) * k;

    nd.attF->setValueAsPartOfGesture (freqForX (target.x));
    if (nd.attG)
        nd.attG->setValueAsPartOfGesture (std::round (gainForY (target.y) * 10.0f) / 10.0f);
}

void EqView::mouseUp (const juce::MouseEvent&)
{
    if (dragNode >= 0)
    {
        auto& nd = nodes[(size_t) dragNode];
        if (nd.attF) nd.attF->endGesture();
        if (nd.attG) nd.attG->endGesture();
    }
    dragNode = -1;
    repaint();
}

void EqView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int n = nodeAt (e.position);
    if (n < 0) return;
    auto& nd = nodes[(size_t) n];
    auto reset = [] (juce::RangedAudioParameter* p, juce::ParameterAttachment* a)
    {
        if (p && a) a->setValueAsCompleteGesture (p->convertFrom0to1 (p->getDefaultValue()));
    };
    reset (nd.f, nd.attF.get());
    reset (nd.g, nd.attG.get());
    reset (nd.q, nd.attQ.get());
}

void EqView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int n = dragNode >= 0 ? dragNode : nodeAt (e.position);
    if (n < 0) return;
    auto& nd = nodes[(size_t) n];
    if (nd.q == nullptr || ! nd.attQ) return;
    const float norm = juce::jlimit (0.0f, 1.0f, nd.q->getValue() + w.deltaY * (w.isReversed ? -1.0f : 1.0f) * 0.35f);
    nd.attQ->setValueAsCompleteGesture (nd.q->convertFrom0to1 (norm));
}

} // namespace et

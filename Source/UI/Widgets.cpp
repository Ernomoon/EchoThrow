#include "Widgets.h"
#include "../PluginProcessor.h"

namespace et
{
using namespace theme;

namespace
{
    constexpr float kA0 = -2.356194f;   // -135 degrees from 12 o'clock
    constexpr float kA1 =  2.356194f;   // +135 degrees
}

//==============================================================================
// Knob
//==============================================================================
Knob::Knob (juce::RangedAudioParameter& p, const juce::String& n, Style s) : name (n), style (s)
{
    setParameter (p);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

Knob::~Knob()
{
    attachment.reset();
}

void Knob::setParameter (juce::RangedAudioParameter& p)
{
    attachment.reset();
    param = &p;
    attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { paramChanged (v); });
    attachment->sendInitialUpdate();
}

void Knob::setSteps (int n)
{
    steps = n;
    repaint();
}

void Knob::setDimmed (bool d)
{
    if (dimmed != d)
    {
        dimmed = d;
        repaint();
    }
}

void Knob::paramChanged (float realValue)
{
    norm = param->convertTo0to1 (realValue);
    repaint();
}

juce::String Knob::currentText() const
{
    const float real = param->convertFrom0to1 (norm);
    return valueText ? valueText (real) : param->getText (norm, 0);
}

void Knob::paint (juce::Graphics& g)
{
    const bool hero = style == Style::Hero;
    auto b = getLocalBounds().toFloat();
    const float textH = hero ? 30.0f : 36.0f;
    auto dialArea = b.removeFromTop (b.getHeight() - textH);
    const float side = std::min (dialArea.getWidth(), dialArea.getHeight());
    const auto sq = juce::Rectangle<float> (side, side).withCentre (dialArea.getCentre());
    const auto c = sq.getCentre();
    const float R = side * 0.5f;
    const float bodyR = hero ? R * 0.72f : R * 0.66f;
    const float angle = kA0 + norm * (kA1 - kA0);
    const float fade = dimmed ? 0.35f : 1.0f;

    auto pt = [&] (float r, float a) { return juce::Point<float> (c.x + r * std::sin (a), c.y - r * std::cos (a)); };

    // ---- scale ----
    if (hero)
    {
        const int n = steps > 1 ? steps : 41;
        for (int i = 0; i < n; ++i)
        {
            const float t = n > 1 ? (float) i / (float) (n - 1) : 0.0f;
            const auto p = pt (R * 0.93f, kA0 + t * (kA1 - kA0));
            const bool active = t <= norm + 0.002f;
            const float rad = steps > 1 ? 2.5f : (i % 5 == 0 ? 2.1f : 1.4f);
            if (active)
            {
                g.setColour (cyan.withAlpha (0.25f * fade));
                g.fillEllipse (p.x - rad * 2.2f, p.y - rad * 2.2f, rad * 4.4f, rad * 4.4f);
            }
            g.setColour (active ? cyan.withAlpha (fade) : textDim.withAlpha (0.45f * fade));
            g.fillEllipse (p.x - rad, p.y - rad, rad * 2.0f, rad * 2.0f);
        }
    }
    else
    {
        juce::Path track;
        track.addCentredArc (c.x, c.y, R * 0.92f, R * 0.92f, 0.0f, kA0, kA1, true);
        g.setColour (juce::Colour (0xff2a2a30).withAlpha (fade));
        g.strokePath (track, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (norm > 0.003f)
        {
            juce::Path val;
            val.addCentredArc (c.x, c.y, R * 0.92f, R * 0.92f, 0.0f, kA0, angle, true);
            drawGlow (g, val, cyan.withAlpha (fade), 2.4f, 4.0f);
        }
    }

    // ---- body ----
    juce::Path body;
    body.addEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.8f), (int) (bodyR * 0.5f), { 0, (int) (bodyR * 0.14f) }).drawForPath (g, body);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff35353b), c.x, c.y - bodyR, juce::Colour (0xff131316), c.x, c.y + bodyR, false));
    g.fillPath (body);

    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (hover ? 0.34f : 0.2f), c.x - bodyR, c.y - bodyR,
                                             juce::Colours::black.withAlpha (0.6f), c.x + bodyR, c.y + bodyR, false));
    g.strokePath (body, juce::PathStrokeType (1.4f));

    const float faceR = bodyR * 0.87f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b2b30), c.x - faceR * 0.35f, c.y - faceR * 0.7f,
                                             juce::Colour (0xff17171a), c.x + faceR * 0.4f, c.y + faceR * 0.9f, true));
    g.fillEllipse (c.x - faceR, c.y - faceR, faceR * 2.0f, faceR * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawEllipse (c.x - faceR, c.y - faceR, faceR * 2.0f, faceR * 2.0f, 1.0f);

    // ---- indicator ----
    juce::Path ind;
    ind.startNewSubPath (pt (bodyR * 0.50f, angle));
    ind.lineTo (pt (bodyR * 0.80f, angle));
    g.setColour (cyan.withAlpha (0.22f * fade));
    g.strokePath (ind, juce::PathStrokeType (hero ? 8.0f : 6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (textBright.withAlpha (fade));
    g.strokePath (ind, juce::PathStrokeType (hero ? 2.8f : 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // ---- text ----
    auto tb = getLocalBounds().toFloat().removeFromBottom (textH);
    const auto value = currentText();
    if (hero)
    {
        juce::AttributedString as;
        as.setJustification (juce::Justification::centred);
        as.append (name, font (13.5f, true, 0.16f), textMain.withAlpha (fade));
        as.append ("  " + value, font (13.5f, false, 0.12f), textDim.withAlpha (fade));
        as.draw (g, tb.withTrimmedTop (4.0f));
    }
    else
    {
        drawCapsText (g, name, tb.removeFromTop (16.0f), textDim.withAlpha (fade), 9.5f, juce::Justification::centred, true, 0.16f);
        g.setColour ((dragging ? cyan : textMain).withAlpha (fade));
        g.setFont (font (11.5f));
        g.drawText (value, tb, juce::Justification::centredTop, false);
    }
}

void Knob::mouseDown (const juce::MouseEvent&)
{
    dragging = true;
    dragNorm = norm;
    lastDragY = 0;
    attachment->beginGesture();
    repaint();
}

void Knob::mouseDrag (const juce::MouseEvent& e)
{
    const float pixelsPerFull = e.mods.isShiftDown() ? 1200.0f : (steps > 1 ? 260.0f : 210.0f);
    const int dy = e.getDistanceFromDragStartY();
    const int step = dy - lastDragY;
    lastDragY = dy;
    dragNorm = juce::jlimit (0.0f, 1.0f, dragNorm - (float) step / pixelsPerFull);
    attachment->setValueAsPartOfGesture (param->convertFrom0to1 (dragNorm));
}

void Knob::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    attachment->endGesture();
    repaint();
}

void Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    attachment->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue()));
}

void Knob::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    float delta = w.deltaY * (w.isReversed ? -1.0f : 1.0f);
    if (steps > 1)
    {
        wheelAccum += delta;
        if (std::abs (wheelAccum) >= 0.08f)
        {
            const float stepSize = 1.0f / (float) (steps - 1);
            const float target = juce::jlimit (0.0f, 1.0f, norm + (wheelAccum > 0 ? stepSize : -stepSize));
            attachment->setValueAsCompleteGesture (param->convertFrom0to1 (target));
            wheelAccum = 0.0f;
        }
        return;
    }
    const float target = juce::jlimit (0.0f, 1.0f, norm + delta * (e.mods.isShiftDown() ? 0.05f : 0.35f));
    attachment->setValueAsCompleteGesture (param->convertFrom0to1 (target));
}

//==============================================================================
// DragValue
//==============================================================================
DragValue::DragValue (juce::RangedAudioParameter& p, const juce::String& n) : param (p), name (n)
{
    attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { norm = param.convertTo0to1 (v); repaint(); });
    attachment->sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void DragValue::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    juce::AttributedString as;
    as.setJustification (juce::Justification::centredLeft);
    as.append (name + "  ", font (10.5f, true, 0.16f), textDim);
    as.append (param.getText (norm, 0), font (12.0f), hover ? cyan : textMain);
    as.draw (g, b);
    if (hover)
    {
        g.setColour (cyan.withAlpha (0.35f));
        g.fillRect (b.getX(), b.getBottom() - 3.0f, b.getWidth(), 1.0f);
    }
}

void DragValue::mouseDown (const juce::MouseEvent&)
{
    dragNorm = norm;
    lastDragY = 0;
    attachment->beginGesture();
}

void DragValue::mouseDrag (const juce::MouseEvent& e)
{
    const int dy = e.getDistanceFromDragStartY();
    const int step = dy - lastDragY;
    lastDragY = dy;
    dragNorm = juce::jlimit (0.0f, 1.0f, dragNorm - (float) step / (e.mods.isShiftDown() ? 900.0f : 180.0f));
    attachment->setValueAsPartOfGesture (param.convertFrom0to1 (dragNorm));
}

void DragValue::mouseUp (const juce::MouseEvent&) { attachment->endGesture(); }

void DragValue::mouseDoubleClick (const juce::MouseEvent&)
{
    attachment->setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

void DragValue::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    const float target = juce::jlimit (0.0f, 1.0f, norm + w.deltaY * 0.3f);
    attachment->setValueAsCompleteGesture (param.convertFrom0to1 (target));
}

//==============================================================================
// ParamToggle
//==============================================================================
ParamToggle::ParamToggle (juce::RangedAudioParameter& p, const juce::String& label, Icon i, bool f)
    : juce::Button (label), icon (i), framed (f)
{
    attachment = std::make_unique<juce::ButtonParameterAttachment> (p, *this);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ParamToggle::paintButton (juce::Graphics& g, bool over, bool down)
{
    const bool on = getToggleState();
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    const auto col = on ? amber : (over ? textMid : textDim);

    if (framed)
    {
        g.setColour (on ? amber.withAlpha (0.07f) : juce::Colour (0xff101012));
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (on ? amber.withAlpha (0.45f) : (over ? strokeHi : stroke));
        g.drawRoundedRectangle (b, 8.0f, 1.0f);
        if (down) { g.setColour (juce::Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (b, 8.0f); }
    }

    auto content = b.reduced (framed ? 10.0f : 0.0f, 0.0f);
    juce::Rectangle<float> iconRect;
    if (icon != Icon::None)
        iconRect = content.removeFromLeft (18.0f).withSizeKeepingCentre (18.0f, 18.0f);

    if (on && icon != Icon::None)
    {
        g.setColour (amber.withAlpha (0.16f));
        g.fillEllipse (iconRect.expanded (5.0f));
    }
    if (icon == Icon::Power) iconPower (g, iconRect, col);
    if (icon == Icon::Loop)  iconLoop (g, iconRect, col);

    drawCapsText (g, getButtonText(), content.withTrimmedLeft (icon != Icon::None ? 6.0f : 0.0f), col, 10.5f, juce::Justification::centredLeft, true, 0.14f);
}

//==============================================================================
// Segment
//==============================================================================
Segment::Segment (juce::RangedAudioParameter& p, const juce::String& off, const juce::String& on)
    : param (p), offText (off), onText (on)
{
    attachment = std::make_unique<juce::ParameterAttachment> (p, [this] (float v) { value = v > 0.5f; repaint(); });
    attachment->sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Segment::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawWell (g, b, b.getHeight() * 0.5f);
    auto inner = b.reduced (3.0f);
    auto left = inner.removeFromLeft (inner.getWidth() * 0.5f);
    auto right = inner;
    auto sel = value ? right : left;

    g.setColour (juce::Colour (0xff25252a));
    g.fillRoundedRectangle (sel, sel.getHeight() * 0.5f);
    g.setColour (strokeHi);
    g.drawRoundedRectangle (sel.reduced (0.5f), sel.getHeight() * 0.5f, 1.0f);

    drawCapsText (g, offText, left, ! value ? cyan : textDim, 10.5f, juce::Justification::centred, true, 0.16f);
    drawCapsText (g, onText, right, value ? cyan : textDim, 10.5f, juce::Justification::centred, true, 0.16f);
}

void Segment::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition())) return;
    const bool wantOn = e.x > getWidth() / 2;
    if (wantOn != value)
        attachment->setValueAsCompleteGesture (wantOn ? 1.0f : 0.0f);
}

//==============================================================================
// IconButton
//==============================================================================
IconButton::IconButton (Kind k, const juce::String& t) : juce::Button (t), kind (k), label (t)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat();
    const auto col = down ? textBright : (over ? textMain : textMid);

    if (kind == Kind::Save)
    {
        g.setColour (juce::Colour (0xff101012));
        g.fillRoundedRectangle (b.reduced (0.5f), 8.0f);
        g.setColour (over ? strokeHi : stroke);
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
    }

    auto icon = b.withSizeKeepingCentre (std::min (b.getWidth(), 28.0f), std::min (b.getHeight(), 28.0f));
    if (label.isNotEmpty())
    {
        icon = b.removeFromLeft (20.0f).withSizeKeepingCentre (18.0f, 18.0f);
        drawCapsText (g, label, b.withTrimmedLeft (4.0f), col, 10.5f, juce::Justification::centredLeft, true, 0.16f);
    }

    switch (kind)
    {
        case Kind::Menu:  iconMenu (g, icon, col); break;
        case Kind::Save:  iconSave (g, icon, col); break;
        case Kind::Reset: iconReset (g, icon, col); break;
        case Kind::Left:  iconChevron (g, icon, col, true); break;
        case Kind::Right: iconChevron (g, icon, col, false); break;
    }
}

//==============================================================================
// REC / LISTEN
//==============================================================================
RecButton::RecButton (EchoThrowProcessor& p) : juce::Button ("rec"), proc (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void RecButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    using S = CaptureEngine::State;
    const auto state = proc.getCaptureState();
    const bool pulse = ((juce::Time::getMillisecondCounter() / 420) % 2) == 0;
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    const float r = b.getHeight() * 0.5f;

    juce::Colour edge = over ? strokeHi : stroke, fill = juce::Colour (0xff101012), dot = recRed.withAlpha (0.8f);
    juce::String label = "REC";

    if (state == S::Armed)
    {
        edge = amber.withAlpha (pulse ? 0.9f : 0.35f);
        fill = amber.withAlpha (0.06f);
        dot = amber.withAlpha (pulse ? 1.0f : 0.4f);
        label = "ARMED";
    }
    else if (state == S::Recording || state == S::Finished)
    {
        edge = recRed.withAlpha (0.8f);
        fill = recRed.withAlpha (0.12f);
        dot = recRed;
        label = "REC  " + juce::String (proc.getCapture().getRecordedSeconds(), 1) + " s";
    }

    g.setColour (fill);
    g.fillRoundedRectangle (b, r);
    g.setColour (edge);
    g.drawRoundedRectangle (b, r, 1.0f);
    if (down) { g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (b, r); }

    const float cx = b.getX() + 20.0f, cy = b.getCentreY();
    g.setColour (dot.withAlpha (0.22f));
    g.fillEllipse (cx - 9.0f, cy - 9.0f, 18.0f, 18.0f);
    g.setColour (dot);
    g.fillEllipse (cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);

    drawCapsText (g, label, b.withTrimmedLeft (36.0f).withTrimmedRight (8.0f),
                  state == S::Idle ? textMain : (state == S::Armed ? amber : textBright), 11.5f, juce::Justification::centredLeft, true, 0.16f);
}

ListenButton::ListenButton (EchoThrowProcessor& p) : juce::Button ("listen"), proc (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ListenButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const bool can = proc.hasClip();
    const bool playing = proc.isListening();
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    const float r = b.getHeight() * 0.5f;

    g.setColour (playing ? cyan.withAlpha (0.10f) : juce::Colour (0xff101012));
    g.fillRoundedRectangle (b, r);
    g.setColour (playing ? cyan.withAlpha (0.7f) : (over && can ? strokeHi : stroke));
    g.drawRoundedRectangle (b, r, 1.0f);
    if (down && can) { g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillRoundedRectangle (b, r); }

    const auto col = ! can ? textDim.withAlpha (0.5f) : (playing ? cyan : textMain);
    auto icon = juce::Rectangle<float> (b.getX() + 10.0f, b.getY(), 22.0f, b.getHeight()).withSizeKeepingCentre (22.0f, 22.0f);
    if (playing) iconStop (g, icon, col); else iconPlay (g, icon, col);
    drawCapsText (g, playing ? "STOP" : "LISTEN", b.withTrimmedLeft (36.0f).withTrimmedRight (8.0f), col, 11.5f,
                  juce::Justification::centredLeft, true, 0.16f);
}

//==============================================================================
// PresetBar
//==============================================================================
PresetBar::PresetBar() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

int PresetBar::zoneAt (juce::Point<int> p) const
{
    if (p.x >= getWidth() - 40) return 3;
    if (p.x >= getWidth() - 80) return 2;
    return 1;
}

void PresetBar::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    drawWell (g, b, 9.0f);

    auto next = b.removeFromRight (40.0f);
    auto prev = b.removeFromRight (40.0f);

    if (hoverZone == 1) { g.setColour (juce::Colours::white.withAlpha (0.03f)); g.fillRoundedRectangle (b.withTrimmedRight (-10.0f).reduced (1.0f), 8.0f); }
    if (hoverZone == 2) { g.setColour (juce::Colours::white.withAlpha (0.05f)); g.fillRect (prev.reduced (0.0f, 1.0f)); }
    if (hoverZone == 3) { g.setColour (juce::Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (next.reduced (1.0f), 8.0f); }

    g.setColour (stroke);
    g.fillRect (prev.getX(), prev.getY() + 7.0f, 1.0f, prev.getHeight() - 14.0f);
    g.fillRect (next.getX(), next.getY() + 7.0f, 1.0f, next.getHeight() - 14.0f);

    g.setColour (textMain);
    g.setFont (font (13.0f, false, 0.04f));
    g.drawText (name, b.withTrimmedLeft (16.0f).withTrimmedRight (6.0f), juce::Justification::centredLeft, true);

    iconChevron (g, prev, hoverZone == 2 ? textBright : textMid, true);
    iconChevron (g, next, hoverZone == 3 ? textBright : textMid, false);
}

void PresetBar::mouseMove (const juce::MouseEvent& e)
{
    const int z = zoneAt (e.getPosition());
    if (z != hoverZone) { hoverZone = z; repaint(); }
}

void PresetBar::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition())) return;
    switch (zoneAt (e.getPosition()))
    {
        case 1: if (onOpenMenu) onOpenMenu(); break;
        case 2: if (onPrev) onPrev(); break;
        case 3: if (onNext) onNext(); break;
        default: break;
    }
}

//==============================================================================
// DragPill
//==============================================================================
DragPill::DragPill() = default;

void DragPill::paint (juce::Graphics& g)
{
    const bool ready = isReady && isReady();
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    const float r = b.getHeight() * 0.5f;

    if (ready)
    {
        g.setColour (amber.withAlpha (pressed ? 0.30f : (hover ? 0.20f : 0.12f)));
        g.fillRoundedRectangle (b, r);
        g.setColour (amber.withAlpha (hover ? 0.95f : 0.6f));
    }
    else
    {
        g.setColour (juce::Colour (0xff101012));
        g.fillRoundedRectangle (b, r);
        g.setColour (stroke);
    }
    g.drawRoundedRectangle (b, r, 1.0f);

    const auto col = ready ? amber : textDim.withAlpha (0.6f);
    iconGrip (g, b.removeFromLeft (30.0f), col);
    drawCapsText (g, "DRAG TO PLAYLIST", b.withTrimmedRight (14.0f), col, 10.5f, juce::Justification::centredLeft, true, 0.16f);
}

void DragPill::mouseDrag (const juce::MouseEvent& e)
{
    if (dragStarted || e.getDistanceFromDragStart() < 6 || ! (isReady && isReady()) || ! getFile)
        return;

    const auto file = getFile();
    if (! file.existsAsFile())
        return;

    dragStarted = true;
    if (onDragStarted) onDragStarted();

    juce::Component::SafePointer<DragPill> safe (this);
    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
        [safe] { if (safe != nullptr) { safe->dragStarted = false; safe->pressed = false; safe->repaint(); } });
}

} // namespace et

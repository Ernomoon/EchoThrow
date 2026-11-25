#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"

namespace et
{
class EchoThrowProcessor;

//==============================================================================
/** Rotary knob bound to a parameter. Vertical drag, shift = fine, wheel, double-click = default. */
class Knob : public juce::Component
{
public:
    enum class Style { Hero, Small };

    Knob (juce::RangedAudioParameter& p, const juce::String& name, Style style);
    ~Knob() override;

    void setParameter (juce::RangedAudioParameter& p);   // rebind (used by the TIME knob when SYNC changes)
    void setSteps (int numSteps);                        // > 1 = discrete (ticks are drawn per step)
    void setDimmed (bool shouldBeDimmed);
    void setName (const juce::String& n) { name = n; repaint(); }

    std::function<juce::String (float)> valueText;       // optional custom formatting of the real value

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

private:
    void paramChanged (float realValue);
    juce::String currentText() const;

    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::String name;
    Style style;
    int steps = 0;
    float norm = 0.0f, dragNorm = 0.0f, wheelAccum = 0.0f;
    int lastDragY = 0;
    bool hover = false, dragging = false, dimmed = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

//==============================================================================
/** Small label with a draggable number, e.g. "TAIL 6.0 s". */
class DragValue : public juce::Component
{
public:
    DragValue (juce::RangedAudioParameter& p, const juce::String& name);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }

private:
    juce::RangedAudioParameter& param;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::String name;
    float norm = 0.0f, dragNorm = 0.0f;
    int lastDragY = 0;
    bool hover = false;
};

//==============================================================================
/** Pill toggle with a power / loop icon, bound to a bool parameter. */
class ParamToggle : public juce::Button
{
public:
    enum class Icon { Power, Loop, None };
    ParamToggle (juce::RangedAudioParameter& p, const juce::String& label, Icon icon = Icon::Power, bool framed = true);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    std::unique_ptr<juce::ButtonParameterAttachment> attachment;
    Icon icon;
    bool framed;
};

/** Two-way switch, e.g. STEREO | PING-PONG. */
class Segment : public juce::Component
{
public:
    Segment (juce::RangedAudioParameter& p, const juce::String& off, const juce::String& on);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter& param;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    juce::String offText, onText;
    bool value = false;
};

/** Plain icon button (menu, save, reset, preset arrows). */
class IconButton : public juce::Button
{
public:
    enum class Kind { Menu, Save, Reset, Left, Right };
    IconButton (Kind k, const juce::String& text = {});
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    Kind kind;
    juce::String label;
};

//==============================================================================
class RecButton : public juce::Button
{
public:
    explicit RecButton (EchoThrowProcessor& p);
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    EchoThrowProcessor& proc;
};

class ListenButton : public juce::Button
{
public:
    explicit ListenButton (EchoThrowProcessor& p);
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    EchoThrowProcessor& proc;
};

//==============================================================================
/** Preset name box with previous / next arrows. */
class PresetBar : public juce::Component
{
public:
    PresetBar();
    void setName (const juce::String& n) { name = n; repaint(); }
    std::function<void()> onOpenMenu, onPrev, onNext;

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hoverZone = 0; repaint(); }

private:
    int zoneAt (juce::Point<int>) const;
    juce::String name;
    int hoverZone = 0;   // 1 = name, 2 = prev, 3 = next
};

/** Grab this and drag it onto a DAW playlist: hands the rendered WAV over as a real file drag. */
class DragPill : public juce::Component
{
public:
    DragPill();
    std::function<juce::File()> getFile;     // renders / returns the file to drag; empty File = nothing to drag
    std::function<bool()> isReady;
    std::function<void()> onDragStarted;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { pressed = true; dragStarted = false; repaint(); }
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { pressed = false; repaint(); }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    juce::MouseCursor getMouseCursor() override { return juce::MouseCursor::DraggingHandCursor; }

private:
    bool hover = false, pressed = false, dragStarted = false;
};

} // namespace et

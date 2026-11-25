#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace et::theme
{
// ---- palette: graphite surfaces, cyan = your voice, amber = the echoes ----
inline const juce::Colour bg0        { 0xff08080a };
inline const juce::Colour bg1        { 0xff0f0f12 };
inline const juce::Colour panelTop   { 0xff1c1c20 };
inline const juce::Colour panelBot   { 0xff141417 };
inline const juce::Colour wellTop    { 0xff0c0c0e };
inline const juce::Colour wellBot    { 0xff121215 };
inline const juce::Colour stroke     { 0xff2a2a30 };
inline const juce::Colour strokeHi   { 0xff3c3c44 };
inline const juce::Colour textDim    { 0xff6b6b75 };
inline const juce::Colour textMid    { 0xff9a9aa5 };
inline const juce::Colour textMain   { 0xffd9d9df };
inline const juce::Colour textBright { 0xfff4f4f8 };
inline const juce::Colour cyan       { 0xff4fd8ee };
inline const juce::Colour amber      { 0xffffa63d };
inline const juce::Colour recRed     { 0xffff4b5c };
inline const juce::Colour violet     { 0xffc08cff };
inline const juce::Colour blue       { 0xff7aa2ff };

juce::Font font (float height, bool bold = false, float tracking = 0.0f);

/** Raised graphite card with a soft shadow and a faint top highlight. */
void drawCard (juce::Graphics&, juce::Rectangle<float>, float radius = 14.0f);
/** Recessed display well. */
void drawWell (juce::Graphics&, juce::Rectangle<float>, float radius = 10.0f);

void drawGlow (juce::Graphics&, const juce::Path&, juce::Colour, float strokeWidth, float spread = 6.0f);
void drawCapsText (juce::Graphics&, const juce::String&, juce::Rectangle<float>, juce::Colour, float size,
                   juce::Justification, bool bold = false, float tracking = 0.12f);

// ---- icons (all drawn inside the given square) ----
void iconPower   (juce::Graphics&, juce::Rectangle<float>, juce::Colour, float thickness = 1.6f);
void iconPlay    (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void iconStop    (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void iconLoop    (juce::Graphics&, juce::Rectangle<float>, juce::Colour, float thickness = 1.6f);
void iconChevron (juce::Graphics&, juce::Rectangle<float>, juce::Colour, bool left, float thickness = 1.8f);
void iconMenu    (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void iconSave    (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void iconReset   (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void iconGrip    (juce::Graphics&, juce::Rectangle<float>, juce::Colour);
void iconMark    (juce::Graphics&, juce::Rectangle<float>, juce::Colour, juce::Colour);   // the EchoThrow logo mark

} // namespace et::theme

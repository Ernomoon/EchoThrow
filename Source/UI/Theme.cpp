#include "Theme.h"

namespace et::theme
{
using juce::Colour;
using juce::Graphics;
using juce::Path;
using juce::Rectangle;

juce::Font font (float height, bool bold, float tracking)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain).withKerningFactor (tracking));
}

void drawCard (Graphics& g, Rectangle<float> r, float radius)
{
    Path p;
    p.addRoundedRectangle (r, radius);

    juce::DropShadow (juce::Colours::black.withAlpha (0.55f), 18, { 0, 6 }).drawForPath (g, p);

    g.setGradientFill (juce::ColourGradient (panelTop, r.getX(), r.getY(), panelBot, r.getX(), r.getBottom(), false));
    g.fillPath (p);

    g.setColour (stroke);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);

    // faint highlight along the top edge
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), r.getX(), r.getY(),
                                             juce::Colours::transparentWhite, r.getX(), r.getY() + 26.0f, false));
    g.drawRoundedRectangle (r.reduced (1.5f), radius - 1.0f, 1.0f);
}

void drawWell (Graphics& g, Rectangle<float> r, float radius)
{
    g.setGradientFill (juce::ColourGradient (wellTop, r.getX(), r.getY(), wellBot, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawRoundedRectangle (r.translated (0, 1.0f).reduced (0.5f), radius, 1.0f);
}

void drawGlow (Graphics& g, const Path& p, Colour c, float w, float spread)
{
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (c.withAlpha (0.07f * (float) (4 - i)));
        g.strokePath (p, juce::PathStrokeType (w + spread * (float) i * 0.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void drawCapsText (Graphics& g, const juce::String& s, Rectangle<float> r, Colour c, float size,
                   juce::Justification j, bool bold, float tracking)
{
    g.setColour (c);
    g.setFont (font (size, bold, tracking));
    g.drawText (s, r, j, false);
}

//==============================================================================
void iconPower (Graphics& g, Rectangle<float> r, Colour c, float t)
{
    const auto cen = r.getCentre();
    const float rad = r.getWidth() * 0.34f;
    Path p;
    p.addCentredArc (cen.x, cen.y + r.getHeight() * 0.04f, rad, rad, 0.0f, 0.62f, juce::MathConstants<float>::twoPi - 0.62f, true);
    p.startNewSubPath (cen.x, cen.y - rad * 1.15f);
    p.lineTo (cen.x, cen.y - rad * 0.1f);
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (t, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void iconPlay (Graphics& g, Rectangle<float> r, Colour c)
{
    r = r.reduced (r.getWidth() * 0.2f);
    Path p;
    p.addTriangle (r.getX() + r.getWidth() * 0.12f, r.getY(), r.getX() + r.getWidth() * 0.12f, r.getBottom(), r.getRight(), r.getCentreY());
    g.setColour (c);
    g.fillPath (p.createPathWithRoundedCorners (1.2f));
}

void iconStop (Graphics& g, Rectangle<float> r, Colour c)
{
    g.setColour (c);
    g.fillRoundedRectangle (r.reduced (r.getWidth() * 0.26f), 1.5f);
}

void iconLoop (Graphics& g, Rectangle<float> r, Colour c, float t)
{
    const auto cen = r.getCentre();
    const float rad = r.getWidth() * 0.32f;
    const float pi = juce::MathConstants<float>::pi;
    Path p;
    p.addCentredArc (cen.x, cen.y, rad, rad, 0.0f, 0.35f, pi - 0.35f, true);          // right half
    p.addCentredArc (cen.x, cen.y, rad, rad, 0.0f, pi + 0.35f, 2 * pi - 0.35f, true); // left half
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (t, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    auto arrow = [&] (float angle)
    {
        // arrow head at the end of an arc, pointing along the clockwise tangent
        const float x = cen.x + rad * std::sin (angle), y = cen.y - rad * std::cos (angle);
        const float tx = std::cos (angle), ty = std::sin (angle);
        const float nx = -ty, ny = tx;
        const float s = r.getWidth() * 0.16f;
        Path a;
        a.addTriangle (x + tx * s * 1.1f, y + ty * s * 1.1f,
                       x - tx * s * 0.5f + nx * s, y - ty * s * 0.5f + ny * s,
                       x - tx * s * 0.5f - nx * s, y - ty * s * 0.5f - ny * s);
        g.fillPath (a);
    };
    arrow (pi - 0.35f);
    arrow (2 * pi - 0.35f);
}

void iconChevron (Graphics& g, Rectangle<float> r, Colour c, bool left, float t)
{
    const auto cen = r.getCentre();
    const float h = r.getHeight() * 0.22f, w = r.getWidth() * 0.12f;
    const float d = left ? 1.0f : -1.0f;
    Path p;
    p.startNewSubPath (cen.x + d * w, cen.y - h);
    p.lineTo (cen.x - d * w, cen.y);
    p.lineTo (cen.x + d * w, cen.y + h);
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (t, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void iconMenu (Graphics& g, Rectangle<float> r, Colour c)
{
    const auto cen = r.getCentre();
    const float w = r.getWidth() * 0.36f;
    g.setColour (c);
    for (int i = -1; i <= 1; ++i)
        g.fillRoundedRectangle (cen.x - w, cen.y + (float) i * 5.5f - 0.9f, w * 2.0f, 1.8f, 0.9f);
}

void iconSave (Graphics& g, Rectangle<float> r, Colour c)
{
    r = r.reduced (r.getWidth() * 0.24f);
    Path p;
    p.addRoundedRectangle (r, 2.0f);
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (1.4f));
    g.drawLine (r.getX() + r.getWidth() * 0.25f, r.getY() + 0.7f, r.getX() + r.getWidth() * 0.25f, r.getY() + r.getHeight() * 0.38f, 1.4f);
    g.drawLine (r.getX() + r.getWidth() * 0.25f, r.getY() + r.getHeight() * 0.38f, r.getX() + r.getWidth() * 0.72f, r.getY() + r.getHeight() * 0.38f, 1.4f);
    g.fillRoundedRectangle (r.getX() + r.getWidth() * 0.25f, r.getY() + r.getHeight() * 0.62f, r.getWidth() * 0.5f, r.getHeight() * 0.2f, 1.0f);
}

void iconReset (Graphics& g, Rectangle<float> r, Colour c)
{
    const auto cen = r.getCentre();
    const float rad = r.getWidth() * 0.3f;
    Path p;
    p.addCentredArc (cen.x, cen.y, rad, rad, 0.0f, 0.5f, juce::MathConstants<float>::twoPi - 0.2f, true);
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    Path a;
    const float x = cen.x + rad * std::sin (0.5f), y = cen.y - rad * std::cos (0.5f);
    a.addTriangle (x - 3.6f, y - 0.5f, x + 3.2f, y - 3.0f, x + 3.4f, y + 3.6f);
    g.fillPath (a);
}

void iconGrip (Graphics& g, Rectangle<float> r, Colour c)
{
    const auto cen = r.getCentre();
    g.setColour (c);
    for (int row = -1; row <= 1; ++row)
        for (int col = 0; col < 2; ++col)
            g.fillEllipse (cen.x + ((float) col - 0.5f) * 5.0f - 1.1f, cen.y + (float) row * 5.0f - 1.1f, 2.2f, 2.2f);
}

void iconMark (Graphics& g, Rectangle<float> r, Colour a, Colour b)
{
    // a source dot followed by three fading echo arcs
    const float cx = r.getX() + r.getWidth() * 0.22f, cy = r.getCentreY();
    g.setColour (a);
    g.fillEllipse (cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
    for (int i = 1; i <= 3; ++i)
    {
        Path p;
        const float rad = 5.5f + (float) i * 4.6f;
        p.addCentredArc (cx, cy, rad, rad, 0.0f, 0.62f, juce::MathConstants<float>::pi - 0.62f, true);
        g.setColour (b.withAlpha (1.0f - 0.27f * (float) (i - 1)));
        g.strokePath (p, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

} // namespace et::theme

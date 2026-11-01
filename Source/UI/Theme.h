#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <functional>
#if ORG_HAS_FONTS
 #include "BinaryData.h"
#endif

namespace theme
{
using juce::Colour;

inline const Colour ground      { 0xff0f120c };
inline const Colour bark        { 0xff181c13 };
inline const Colour bark2       { 0xff21261a };
inline const Colour bark3       { 0xff2b3122 };
inline const Colour wood        { 0xff3a2b1e };
inline const Colour woodHi      { 0xff7a5a3c };
inline const Colour line        { 0xff353b2b };
inline const Colour bone        { 0xffe8e2cf };
inline const Colour boneDim     { 0xffa7a38d };
inline const Colour boneFaint   { 0xff6f6d5c };
inline const Colour moss        { 0xff8fb35a };
inline const Colour stepBeat    { 0xff4a3a29 };
inline const Colour stepBeatEdge{ 0xff8d6c47 };
inline const Colour stepOff     { 0xff272c20 };
inline const Colour stepOffEdge { 0xff363d2c };
inline const Colour screen      { 0xff0b0e08 };

inline const std::array<Colour, 5> rowColours { Colour (0xffc9924f), Colour (0xff5fa7a3), Colour (0xff8fb35a),
                                                Colour (0xffc9b862), Colour (0xffc47c6e) };
inline const std::array<Colour, 5> groupColours { Colour (0xff5fa7a3), Colour (0xffc9924f), Colour (0xffb08560),
                                                  Colour (0xff8fb35a), Colour (0xffc47c6e) };

//==============================================================================
struct Resources
{
    Resources()
    {
       #if ORG_HAS_FONTS
        display = juce::Typeface::createSystemTypefaceFor (BinaryData::display_ttf, (size_t) BinaryData::display_ttfSize);
        body    = juce::Typeface::createSystemTypefaceFor (BinaryData::body_ttf, (size_t) BinaryData::body_ttfSize);
        mono    = juce::Typeface::createSystemTypefaceFor (BinaryData::mono_ttf, (size_t) BinaryData::mono_ttfSize);
       #endif

        // fine grain over every panel
        grain = juce::Image (juce::Image::ARGB, 160, 160, true);
        juce::Random r (2024);
        for (int y = 0; y < 160; ++y)
            for (int x = 0; x < 160; ++x)
            {
                const auto v = (juce::uint8) (150 + r.nextInt (106));
                grain.setPixelAt (x, y, juce::Colour (v, (juce::uint8) juce::jmin (255, v + 8), (juce::uint8) (v - 20), (juce::uint8) r.nextInt (14)));
            }

        // wood grain for the frame: stretched value noise bent into rings
        wood = juce::Image (juce::Image::RGB, 600, 160, false);
        float lattice[24][8];
        for (auto& row : lattice)
            for (auto& v : row)
                v = r.nextFloat();
        auto noise = [&lattice] (float x, float y)
        {
            const int xi = (int) x, yi = (int) y;
            const float fx = x - (float) xi, fy = y - (float) yi;
            auto at = [&lattice] (int a, int b) { return lattice[a % 24][b % 8]; };
            const float sx = fx * fx * (3.0f - 2.0f * fx), sy = fy * fy * (3.0f - 2.0f * fy);
            const float a = at (xi, yi) + (at (xi + 1, yi) - at (xi, yi)) * sx;
            const float b = at (xi, yi + 1) + (at (xi + 1, yi + 1) - at (xi, yi + 1)) * sx;
            return a + (b - a) * sy;
        };
        for (int y = 0; y < 160; ++y)
            for (int x = 0; x < 600; ++x)
            {
                const float n = noise ((float) x / 600.0f * 23.0f, (float) y / 160.0f * 7.0f);
                const float ring = 0.5f + 0.5f * std::sin (((float) y * 0.22f + n * 14.0f));
                const float fine = r.nextFloat() * 0.06f;
                const float t = juce::jlimit (0.0f, 1.0f, 0.35f + ring * 0.45f + fine);
                wood.setPixelAt (x, y, Colour (0xff2a1f15).interpolatedWith (Colour (0xff57402b), t));
            }
    }

    juce::Typeface::Ptr display, body, mono;
    juce::Image grain, wood;
};

inline juce::Font makeFont (const juce::Typeface::Ptr& tf, const juce::String& fallback, float h, int style)
{
    if (tf != nullptr)
        return juce::Font (juce::FontOptions().withTypeface (tf).withHeight (h));
    return juce::Font (juce::FontOptions (fallback, h, style));
}

inline juce::Font display (float h, float kern = 0.1f)
{
    juce::SharedResourcePointer<Resources> r;
    return makeFont (r->display, "Palatino Linotype", h, juce::Font::bold).withExtraKerningFactor (kern);
}

inline juce::Font body (float h, float kern = 0.0f)
{
    juce::SharedResourcePointer<Resources> r;
    return makeFont (r->body, "Segoe UI", h, juce::Font::plain).withExtraKerningFactor (kern);
}

// small caps labels
inline juce::Font label (float h = 10.0f, float kern = 0.2f) { return body (h, kern); }

inline juce::Font mono (float h)
{
    juce::SharedResourcePointer<Resources> r;
    return makeFont (r->mono, "Consolas", h, juce::Font::plain);
}

//==============================================================================
inline void fillGrain (juce::Graphics& g, juce::Rectangle<float> r, float opacity, float corner = 0.0f)
{
    juce::SharedResourcePointer<Resources> res;
    g.setTiledImageFill (res->grain, 0, 0, opacity);
    if (corner > 0.0f)
        g.fillRoundedRectangle (r, corner);
    else
        g.fillRect (r);
}

inline void fillWood (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    juce::SharedResourcePointer<Resources> res;
    g.setTiledImageFill (res->wood, 0, 0, 1.0f);
    g.fillRoundedRectangle (r, corner);
}

// a raised card: soft gradient, hairline edge, title in small caps
inline void card (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, const juce::String& sub = {})
{
    g.setGradientFill (juce::ColourGradient (bark2, 0.0f, r.getY(), Colour (0xff1a1f14), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (line);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    if (title.isNotEmpty())
    {
        g.setColour (boneDim);
        g.setFont (display (13.0f, 0.2f));
        g.drawText (title, r.reduced (14.0f, 0.0f).withHeight (32.0f).withY (r.getY() + 4.0f), juce::Justification::centredLeft);
        if (sub.isNotEmpty())
        {
            g.setColour (boneFaint);
            g.setFont (body (11.5f));
            g.drawText (sub, r.reduced (14.0f, 0.0f).withHeight (32.0f).withY (r.getY() + 4.0f), juce::Justification::centredRight);
        }
    }
}

inline void screenBack (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (screen);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (Colour (0xff2a3022));
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
}

inline void faintGrid (juce::Graphics& g, juce::Rectangle<float> r, int cols, int rows = 4)
{
    g.setColour (Colour (0xff1d2318));
    for (int i = 1; i < cols; ++i)
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / (float) cols), r.getY() + 2.0f, r.getBottom() - 2.0f);
    for (int i = 1; i < rows; ++i)
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / (float) rows), r.getX() + 2.0f, r.getRight() - 2.0f);
}

inline float textWidth (const juce::Font& f, const juce::String& s)
{
    juce::GlyphArrangement ga;
    ga.addLineOfText (f, s, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, true).getWidth();
}

// the mycelium knot used as logo
inline void drawLogo (juce::Graphics& g, juce::Point<float> c, float size)
{
    juce::Random r (9);
    const float u = size / 80.0f;
    std::function<void (float, float, float, float, float, int)> branch = [&] (float px, float py, float a, float len, float w, int depth)
    {
        if (depth > 5 || len < 3.0f)
            return;
        const float nx = px + std::cos (a) * len, ny = py + std::sin (a) * len;
        juce::Path p;
        p.startNewSubPath (c.x + px * u, c.y + py * u);
        p.quadraticTo (c.x + (px + std::cos (a + 0.4f) * len * 0.6f) * u, c.y + (py + std::sin (a + 0.4f) * len * 0.6f) * u, c.x + nx * u, c.y + ny * u);
        g.setColour (moss.withAlpha (0.4f + 0.12f * (float) (6 - depth) * 0.5f));
        g.strokePath (p, juce::PathStrokeType (w * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        branch (nx, ny, a + (r.nextFloat() - 0.5f) * 1.2f, len * 0.72f, w * 0.7f, depth + 1);
        if (r.nextFloat() < 0.7f)
            branch (nx, ny, a + (r.nextFloat() - 0.5f) * 2.0f, len * 0.6f, w * 0.6f, depth + 1);
    };
    for (int i = 0; i < 7; ++i)
        branch (0.0f, 0.0f, (float) i / 7.0f * juce::MathConstants<float>::twoPi, 13.0f, 2.6f, 0);
    g.setColour (rowColours[3]);
    g.fillEllipse (c.x - 5.0f * u, c.y - 5.0f * u, 10.0f * u, 10.0f * u);
    g.setColour (bark);
    g.fillEllipse (c.x - 2.0f * u, c.y - 2.0f * u, 4.0f * u, 4.0f * u);
}
} // namespace theme

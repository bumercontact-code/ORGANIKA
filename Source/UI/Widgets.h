#pragma once

#include <functional>
#include <memory>
#include <vector>
#include "Theme.h"
#include "../PluginProcessor.h"

namespace ui
{
using juce::Colour;
using APVTS = juce::AudioProcessorValueTreeState;

inline std::unique_ptr<juce::ParameterAttachment> makeAttachment (APVTS& s, const juce::String& id, std::function<void (float)> cb)
{
    auto* p = s.getParameter (id);
    jassert (p != nullptr);
    auto a = std::make_unique<juce::ParameterAttachment> (*p, std::move (cb), nullptr);
    a->sendInitialUpdate();
    return a;
}

//==============================================================================
// Arc knob with a wooden cap; the arc takes the row colour.
class KnobLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const float size = (float) juce::jmin (w, h);
        const juce::Point<float> c ((float) x + (float) w * 0.5f, (float) y + (float) h * 0.5f);
        const float rad = size * 0.5f - 4.0f, thick = juce::jmax (3.0f, size * 0.1f);
        const float ang = a0 + pos * (a1 - a0);
        const auto accent = s.findColour (juce::Slider::rotarySliderFillColourId);

        juce::Path track;
        track.addCentredArc (c.x, c.y, rad, rad, 0.0f, a0, a1, true);
        g.setColour (theme::screen);
        g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc (c.x, c.y, rad, rad, 0.0f, a0, ang, true);
        g.setColour (accent.withAlpha (0.25f));
        g.strokePath (val, juce::PathStrokeType (thick + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (accent);
        g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float cr = rad - thick - 2.0f;
        juce::ColourGradient cg (Colour (0xff5b4632), c.x - cr * 0.3f, c.y - cr * 0.4f, Colour (0xff1c150f), c.x + cr, c.y + cr, true);
        cg.addColour (0.7, Colour (0xff2f241a));
        g.setGradientFill (cg);
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawEllipse (c.x - cr, c.y - cr, cr * 2.0f, cr * 2.0f, 1.0f);

        g.setColour (theme::bone);
        juce::Path ptr;
        ptr.startNewSubPath (c.getPointOnCircumference (cr * 0.25f, ang));
        ptr.lineTo (c.getPointOnCircumference (cr * 0.9f, ang));
        g.strokePath (ptr, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
};

//==============================================================================
class Knob : public juce::Component
{
public:
    Knob (APVTS& s, const juce::String& id, const juce::String& text, int size, Colour accent)
        : label (text), knobSize (size)
    {
        slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (200);
        slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
        slider.onValueChange = [this] { repaint(); };
        slider.onDragStart = [this] { active = true; repaint(); };
        slider.onDragEnd = [this] { active = false; repaint(); };
        addAndMakeVisible (slider);
        setInterceptsMouseClicks (false, true);
        attachment = std::make_unique<APVTS::SliderAttachment> (s, id, slider);
        if (auto* prm = s.getParameter (id))
            slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        setSize (juce::jmax (62, size + 14), size + 30);
    }

    void setEnabledLook (bool e)
    {
        slider.setEnabled (e);
        setAlpha (e ? 1.0f : 0.3f);
    }

    void resized() override { slider.setBounds ((getWidth() - knobSize) / 2, 0, knobSize, knobSize); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (active ? theme::bone : theme::bone.withAlpha (0.9f));
        g.setFont (theme::mono (10.5f));
        g.drawText (slider.getTextFromValue (slider.getValue()), r.withY ((float) knobSize).withHeight (14.0f), juce::Justification::centred, false);
        g.setColour (theme::boneFaint);
        g.setFont (theme::label (9.5f, 0.18f));
        g.drawText (label, r.withY ((float) knobSize + 14.0f).withHeight (13.0f), juce::Justification::centred, false);
    }

    juce::Slider slider;

private:
    juce::String label;
    int knobSize;
    bool active = false;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

//==============================================================================
// Plain clickable component painted by its owner's style.
class FlatButton : public juce::Component
{
public:
    explicit FlatButton (juce::String t = {}, bool primary = false) : text (std::move (t)), prime (primary)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setText (const juce::String& t) { text = t; repaint(); }
    void setLit (bool b) { lit = b; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (prime ? theme::moss : (hover ? theme::bark3 : theme::bark2));
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (lit ? accent : (prime ? theme::moss : (hover ? Colour (0xff4a523b) : theme::line)));
        g.drawRoundedRectangle (r, 7.0f, 1.0f);
        if (icon)
            icon (g, r, prime ? theme::ground : (lit ? accent : (hover ? theme::bone : theme::boneDim)));
        if (text.isNotEmpty())
        {
            g.setColour (prime ? theme::ground : (lit ? accent : (hover ? theme::bone : theme::boneDim)));
            g.setFont (theme::label (11.5f, 0.12f));
            g.drawText (text, r.withTrimmedLeft (icon ? 16.0f : 0.0f), juce::Justification::centred, false);
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()) && onClick)
            onClick();
    }

    std::function<void()> onClick;
    std::function<void (juce::Graphics&, juce::Rectangle<float>, Colour)> icon;
    Colour accent = theme::rowColours[3];

private:
    juce::String text;
    bool prime = false, hover = false, lit = false;
};

//==============================================================================
// Segmented choice selector bound to a choice parameter.
class Segments : public juce::Component
{
public:
    Segments (APVTS& s, const juce::String& id, juce::StringArray labels, Colour accent)
        : items (std::move (labels)), colour (accent)
    {
        attachment = makeAttachment (s, id, [this] (float v) { index = juce::roundToInt (v); repaint(); });
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (Colour (0xff0e110b));
        g.fillRoundedRectangle (r, 7.0f);
        const float w = r.getWidth() / (float) items.size();
        for (int i = 0; i < items.size(); ++i)
        {
            auto cell = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight());
            if (i == index)
            {
                g.setColour (theme::bark3);
                g.fillRoundedRectangle (cell.reduced (1.0f), 6.0f);
                g.setColour (colour);
                g.fillRect (cell.reduced (8.0f, 0.0f).removeFromBottom (2.5f).translated (0.0f, -1.0f));
            }
            if (i > 0)
            {
                g.setColour (theme::line);
                g.drawVerticalLine ((int) cell.getX(), r.getY() + 1.0f, r.getBottom() - 1.0f);
            }
            g.setColour (i == index ? theme::bone : theme::boneFaint);
            g.setFont (theme::label (11.0f, 0.12f));
            g.drawText (items[i], cell, juce::Justification::centred, false);
        }
        g.setColour (theme::line);
        g.drawRoundedRectangle (r, 7.0f, 1.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = juce::jlimit (0, items.size() - 1, (int) ((float) e.x / ((float) getWidth() / (float) items.size())));
        attachment->setValueAsCompleteGesture ((float) i);
    }

private:
    juce::StringArray items;
    Colour colour;
    int index = 0;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

//==============================================================================
// Rounded chip bound to a bool parameter.
class Chip : public juce::Component
{
public:
    Chip (APVTS& s, const juce::String& id, juce::String onText, juce::String offText, Colour accent)
        : textOn (std::move (onText)), textOff (std::move (offText)), colour (accent)
    {
        attachment = makeAttachment (s, id, [this] (float v) { on = v > 0.5f; repaint(); if (onChange) onChange (on); });
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const float rad = r.getHeight() * 0.5f;
        g.setColour (on ? colour : theme::screen);
        g.fillRoundedRectangle (r, rad);
        g.setColour (on ? colour : theme::line);
        g.drawRoundedRectangle (r, rad, 1.0f);
        g.setColour (on ? theme::ground : theme::boneFaint);
        g.setFont (theme::label (10.0f, 0.16f));
        g.drawText (on ? textOn : textOff, r, juce::Justification::centred, false);
    }

    void mouseDown (const juce::MouseEvent&) override { attachment->setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

    bool isOn() const { return on; }
    std::function<void (bool)> onChange;

private:
    juce::String textOn, textOff;
    Colour colour;
    bool on = false;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

//==============================================================================
// Glowing power dot (row on / off).
class PowerDot : public juce::Component
{
public:
    PowerDot (APVTS& s, const juce::String& id, Colour c) : colour (c)
    {
        attachment = makeAttachment (s, id, [this] (float v) { on = v > 0.5f; repaint(); if (onChange) onChange (on); });
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        const float d = 18.0f;
        if (on)
        {
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.5f), c.x, c.y, colour.withAlpha (0.0f), c.x + d, c.y, true));
            g.fillEllipse (c.x - d, c.y - d, d * 2.0f, d * 2.0f);
            g.setColour (colour);
            g.fillEllipse (c.x - d * 0.5f, c.y - d * 0.5f, d, d);
        }
        else
        {
            g.setColour (theme::boneFaint);
            g.drawEllipse (c.x - d * 0.5f + 0.75f, c.y - d * 0.5f + 0.75f, d - 1.5f, d - 1.5f, 1.5f);
        }
    }

    void mouseDown (const juce::MouseEvent&) override { attachment->setValueAsCompleteGesture (on ? 0.0f : 1.0f); }

    bool isOn() const { return on; }
    std::function<void (bool)> onChange;

private:
    Colour colour;
    bool on = true;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

//==============================================================================
// Big LONG / SHORT slide switch for the envelope.
class SlideSwitch : public juce::Component, private juce::Timer
{
public:
    SlideSwitch (APVTS& s, const juce::String& id, juce::String leftLabel, juce::String rightLabel, Colour c)
        : leftText (std::move (leftLabel)), rightText (std::move (rightLabel)), colour (c)
    {
        attachment = makeAttachment (s, id, [this] (float v) { right = v > 0.5f; startTimerHz (60); if (onChange) onChange (right); });
        knobPos = right ? 1.0f : 0.0f;
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float tw = 96.0f, th = 42.0f;
        auto track = juce::Rectangle<float> (tw, th).withCentre (r.getCentre());
        g.setFont (theme::display (19.0f, 0.14f));
        g.setColour (right ? theme::boneFaint : theme::bone);
        g.drawText (leftText, r.withRight (track.getX() - 14.0f), juce::Justification::centredRight, false);
        g.setColour (right ? theme::bone : theme::boneFaint);
        g.drawText (rightText, r.withLeft (track.getRight() + 14.0f), juce::Justification::centredLeft, false);

        g.setGradientFill (juce::ColourGradient (theme::screen, 0.0f, track.getY(), Colour (0xff151a10), 0.0f, track.getBottom(), false));
        g.fillRoundedRectangle (track, th * 0.5f);
        g.setColour (theme::line);
        g.drawRoundedRectangle (track.reduced (0.5f), th * 0.5f, 1.0f);

        auto nub = juce::Rectangle<float> (44.0f, 32.0f).withPosition (track.getX() + 5.0f + knobPos * (tw - 54.0f), track.getY() + 5.0f);
        const auto top = right ? colour : Colour (0xff8a6a47), bot = right ? colour.darker (0.6f) : Colour (0xff5a442e);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (nub.translated (0.0f, 2.0f), 15.0f);
        g.setGradientFill (juce::ColourGradient (top, 0.0f, nub.getY(), bot, 0.0f, nub.getBottom(), false));
        g.fillRoundedRectangle (nub, 15.0f);
    }

    void mouseDown (const juce::MouseEvent&) override { attachment->setValueAsCompleteGesture (right ? 0.0f : 1.0f); }

    std::function<void (bool)> onChange;

private:
    void timerCallback() override
    {
        const float t = right ? 1.0f : 0.0f;
        knobPos += (t - knobPos) * 0.3f;
        if (std::abs (t - knobPos) < 0.002f)
        {
            knobPos = t;
            stopTimer();
        }
        repaint();
    }

    juce::String leftText, rightText;
    Colour colour;
    bool right = false;
    float knobPos = 0.0f;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

//==============================================================================
// Step geometry shared by the grid and the ruler: bars > beats > sixteenths.
inline std::vector<juce::Rectangle<float>> stepRects (juce::Rectangle<float> r, int numSteps, bool compact)
{
    const int bars = juce::jmax (1, numSteps / 16);
    const float barGap = compact ? 10.0f : 14.0f, beatGap = compact ? 5.0f : 7.0f, stepGap = compact ? 2.0f : 3.0f;
    const float barW = (r.getWidth() - barGap * (float) (bars - 1)) / (float) bars;
    const float beatW = (barW - beatGap * 3.0f) / 4.0f;
    const float stepW = (beatW - stepGap * 3.0f) / 4.0f;
    std::vector<juce::Rectangle<float>> out;
    out.reserve ((size_t) numSteps);
    for (int i = 0; i < numSteps; ++i)
    {
        const int b = i / 16, q = (i / 4) % 4, s = i % 4;
        const float x = r.getX() + (float) b * (barW + barGap) + (float) q * (beatW + beatGap) + (float) s * (stepW + stepGap);
        out.emplace_back (x, r.getY(), stepW, r.getHeight());
    }
    return out;
}

inline int numStepsOf (APVTS& s)
{
    return org::kBarCounts[juce::jlimit (0, 2, (int) std::lround (s.getRawParameterValue ("bars")->load()))] * 16;
}

//==============================================================================
// One sequencer lane. Click toggles a sixteenth; hover raises a vertical mix line;
// drag up/down (or scroll) sets that sixteenth's mix.
class StepGrid : public juce::Component, private juce::Timer
{
public:
    StepGrid (OrganikaAudioProcessor& p, int rowIndex, bool compactLook)
        : proc (p), row (rowIndex), compact (compactLook), colour (theme::rowColours[(size_t) rowIndex])
    {
        hoverAmt.fill (0.0f);
        startTimerHz (40);
    }

    void paint (juce::Graphics& g) override
    {
        const int n = numStepsOf (proc.apvts);
        const auto rects = stepRects (getLocalBounds().toFloat().reduced (0.0f, 2.0f), n, compact);
        const int now = proc.currentStep.load();
        const bool rowOn = proc.apvts.getRawParameterValue (org::rowOnId (row))->load() > 0.5f;
        const float rr = compact ? 3.0f : 5.0f;

        for (int i = 0; i < n; ++i)
        {
            const auto r = rects[(size_t) i];
            const bool first = i % 4 == 0;
            const bool on = proc.sequence.isOn (row, i);
            const float m = proc.sequence.getMix (row, i);

            g.setColour (first ? theme::stepBeat : theme::stepOff);
            g.fillRoundedRectangle (r, rr);

            if (on)
            {
                const auto c = rowOn ? colour : colour.withSaturation (0.15f).withMultipliedBrightness (0.6f);
                auto fill = r.reduced (2.0f);
                fill = fill.removeFromBottom (fill.getHeight() * m);
                g.setGradientFill (juce::ColourGradient (c.darker (0.15f), 0.0f, fill.getBottom(), c.withAlpha (0.5f), 0.0f, fill.getY(), false));
                g.fillRoundedRectangle (fill, rr - 1.5f);
            }

            g.setColour (on ? colour.darker (0.35f) : (first ? theme::stepBeatEdge : theme::stepOffEdge));
            g.drawRoundedRectangle (r.reduced (0.5f), rr, first ? 1.2f : 1.0f);
            if (first)
            {
                g.setColour (Colour (0x1affd9a0));
                g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + 3.0f, r.getRight() - 3.0f);
            }

            if (i == now)
            {
                g.setColour (theme::bone);
                g.drawRoundedRectangle (r.reduced (0.5f), rr, 1.4f);
                g.setColour (theme::bone.withAlpha (0.25f));
                g.drawRoundedRectangle (r.expanded (2.5f), rr + 2.0f, 1.0f);
            }

            // hover / drag: the vertical mix line grows from the bottom
            const float h = hoverAmt[(size_t) i];
            if (h > 0.01f)
            {
                const auto lane = r.reduced (0.0f, 5.0f);
                const float cx = lane.getCentreX(), top = lane.getBottom() - lane.getHeight() * h;
                g.setGradientFill (juce::ColourGradient (theme::bone, cx, lane.getBottom(), theme::bone.withAlpha (0.15f), cx, lane.getY(), false));
                g.fillRect (cx - 1.0f, top, 2.0f, lane.getBottom() - top);
                const float hy = lane.getBottom() - lane.getHeight() * m;
                if (hy >= top - 1.0f)
                {
                    const float hd = compact ? 8.0f : 10.0f;
                    g.setColour (colour.withAlpha (0.5f * h));
                    g.fillEllipse (cx - hd, hy - hd, hd * 2.0f, hd * 2.0f);
                    g.setColour (theme::ground);
                    g.fillEllipse (cx - hd * 0.5f, hy - hd * 0.5f, hd, hd);
                    g.setColour (theme::bone);
                    g.drawEllipse (cx - hd * 0.5f, hy - hd * 0.5f, hd, hd, 2.0f);
                }
            }
        }

        // value tag above the hovered step
        if (hovered >= 0 && hovered < n && hoverAmt[(size_t) hovered] > 0.5f)
        {
            const auto r = rects[(size_t) hovered];
            const auto txt = juce::String (proc.sequence.isOn (row, hovered) ? "" : "off  ") + juce::String (juce::roundToInt (proc.sequence.getMix (row, hovered) * 100.0f)) + "%";
            const auto f = theme::mono (10.0f);
            const float tw = theme::textWidth (f, txt) + 10.0f;
            auto tag = juce::Rectangle<float> (tw, 15.0f).withCentre ({ r.getCentreX(), r.getY() + 9.0f });
            tag.setX (juce::jlimit (0.0f, (float) getWidth() - tw, tag.getX()));
            g.setColour (theme::bone);
            g.fillRoundedRectangle (tag, 4.0f);
            g.setColour (theme::ground);
            g.setFont (f);
            g.drawText (txt, tag, juce::Justification::centred, false);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { hovered = hitTest (e.position); }
    void mouseExit (const juce::MouseEvent&) override { if (! dragging) hovered = -1; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragIndex = hitTest (e.position);
        if (dragIndex < 0)
            return;
        hovered = dragIndex;
        dragging = true;
        moved = false;
        startY = e.position.y;
        startMix = proc.sequence.getMix (row, dragIndex);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragIndex < 0)
            return;
        const float dy = startY - e.position.y;
        if (! moved && std::abs (dy) > 3.0f)
            moved = true;
        if (moved)
        {
            const float h = (float) getHeight() * 1.6f;
            proc.sequence.set (row, dragIndex, true, std::round (juce::jlimit (0.0f, 1.0f, startMix + dy / h) * 100.0f) / 100.0f);
            repaint();
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragIndex >= 0 && ! moved)
            proc.sequence.setOn (row, dragIndex, ! proc.sequence.isOn (row, dragIndex));
        dragging = false;
        dragIndex = -1;
        hovered = hitTest (e.position);
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int i = hitTest (e.position);
        if (i < 0)
            return;
        proc.sequence.set (row, i, true, juce::jlimit (0.0f, 1.0f, proc.sequence.getMix (row, i) + w.deltaY * 0.25f));
        repaint();
    }

private:
    int hitTest (juce::Point<float> p) const
    {
        const auto rects = stepRects (getLocalBounds().toFloat().reduced (0.0f, 2.0f), numStepsOf (proc.apvts), compact);
        for (size_t i = 0; i < rects.size(); ++i)
            if (rects[i].expanded (1.5f, 0.0f).contains (p))
                return (int) i;
        return -1;
    }

    void timerCallback() override
    {
        bool dirty = false;
        for (int i = 0; i < org::kMaxSteps; ++i)
        {
            const float t = (i == hovered) ? 1.0f : 0.0f;
            auto& h = hoverAmt[(size_t) i];
            if (std::abs (h - t) > 0.004f)
            {
                h += (t - h) * (t > h ? 0.28f : 0.22f);
                dirty = true;
            }
            else if (h != t)
            {
                h = t;
                dirty = true;
            }
        }
        const int now = proc.currentStep.load(), ver = proc.sequence.getVersion();
        const int steps = numStepsOf (proc.apvts);
        const bool rowOn = proc.apvts.getRawParameterValue (org::rowOnId (row))->load() > 0.5f;
        if (now != lastStep || ver != lastVersion || steps != lastSteps || rowOn != lastRowOn)
        {
            lastStep = now;
            lastVersion = ver;
            lastSteps = steps;
            lastRowOn = rowOn;
            dirty = true;
        }
        if (dirty)
            repaint();
    }

    OrganikaAudioProcessor& proc;
    int row;
    bool compact;
    Colour colour;
    std::array<float, org::kMaxSteps> hoverAmt {};
    int hovered = -1, dragIndex = -1, lastStep = -2, lastVersion = -1, lastSteps = 0;
    bool dragging = false, moved = false, lastRowOn = true;
    float startY = 0.0f, startMix = 0.0f;
};

//==============================================================================
class PresetBar : public juce::Component
{
public:
    PresetBar()
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setText (const juce::String& category, const juce::String& name)
    {
        cat = category;
        title = name;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (Colour (0xff0e110b));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (theme::line);
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
        g.drawVerticalLine (kArrow, r.getY(), r.getBottom());
        g.drawVerticalLine (getWidth() - kArrow, r.getY(), r.getBottom());

        auto arrow = [&g] (juce::Rectangle<float> a, bool left)
        {
            const auto c = a.getCentre();
            juce::Path p;
            p.startNewSubPath (c.x + (left ? 3.0f : -3.0f), c.y - 6.0f);
            p.lineTo (c.x + (left ? -3.0f : 3.0f), c.y);
            p.lineTo (c.x + (left ? 3.0f : -3.0f), c.y + 6.0f);
            g.setColour (theme::boneDim);
            g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        arrow (r.withWidth ((float) kArrow), true);
        arrow (r.withLeft ((float) (getWidth() - kArrow)), false);

        auto mid = r.reduced ((float) kArrow + 12.0f, 6.0f);
        g.setColour (theme::boneFaint);
        g.setFont (theme::label (9.5f, 0.2f));
        g.drawText (cat, mid.removeFromTop (14.0f), juce::Justification::centredLeft, true);
        g.setColour (theme::bone);
        g.setFont (theme::display (17.0f, 0.04f));
        g.drawText (title, mid, juce::Justification::centredLeft, true);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasClicked())
            return;
        if (e.x < kArrow) { if (onPrev) onPrev(); }
        else if (e.x > getWidth() - kArrow) { if (onNext) onNext(); }
        else if (onOpen) onOpen();
    }

    std::function<void()> onPrev, onNext, onOpen;

private:
    static constexpr int kArrow = 30;
    juce::String cat, title;
};

//==============================================================================
// Preset browser covering the sequencer area: five groups of ten, plus your own.
class PresetLibrary : public juce::Component
{
public:
    explicit PresetLibrary (org::PresetManager& pm) : presets (pm)
    {
        close.onClick = [this] { if (onClose) onClose(); };
        addAndMakeVisible (close);
    }

    void refresh()
    {
        userNames = presets.userPresetNames();
        repaint();
    }

    void resized() override { close.setBounds (getWidth() - 96, 12, 80, 28); }

    void paint (juce::Graphics& g) override
    {
        g.setColour (Colour (0xf20c0f09));
        g.fillRect (getLocalBounds());
        g.setColour (theme::bone);
        g.setFont (theme::display (19.0f, 0.16f));
        g.drawText ("PRESETS", 18, 12, 300, 28, juce::Justification::centredLeft);

        const auto& list = org::factoryPresets();
        const int cur = presets.getFactoryIndex();
        for (int gi = 0; gi < 6; ++gi)
        {
            const auto col = columnRect (gi);
            const auto gc = gi < 5 ? theme::groupColours[(size_t) gi] : theme::bone;
            g.setColour (gc);
            g.setFont (theme::label (10.5f, 0.26f));
            g.drawText (gi < 5 ? org::kPresetGroups[gi] : juce::String ("YOUR PRESETS"), col.withHeight (20.0f), juce::Justification::centredLeft);

            const int count = gi < 5 ? 10 : userNames.size();
            if (gi == 5 && count == 0)
            {
                g.setColour (theme::boneFaint);
                g.setFont (theme::body (12.5f));
                g.drawFittedText ("Nothing saved yet. Press SAVE in the header to keep the current pattern and every module setting.",
                                  col.withTrimmedTop (26).withHeight (80).toNearestInt(), juce::Justification::topLeft, 5);
            }
            for (int i = 0; i < count; ++i)
            {
                const auto item = itemRect (gi, i);
                const bool current = gi < 5 && cur == gi * 10 + i;
                const bool hover = hoverGroup == gi && hoverItem == i;
                if (current || hover)
                {
                    g.setColour (current ? theme::bark3 : theme::bark2);
                    g.fillRoundedRectangle (item, 6.0f);
                    if (current)
                    {
                        g.setColour (gc);
                        g.fillRect (item.withWidth (2.0f));
                    }
                }
                g.setColour (theme::boneFaint);
                g.setFont (theme::mono (10.0f));
                g.drawText (gi < 5 ? juce::String (gi * 10 + i + 1).paddedLeft ('0', 2) : "U" + juce::String (i + 1), item.withWidth (30.0f).translated (8.0f, 0.0f), juce::Justification::centredLeft);
                g.setColour (current || hover ? theme::bone : theme::boneDim);
                g.setFont (theme::body (14.0f));
                g.drawText (gi < 5 ? juce::String (list[(size_t) (gi * 10 + i)].name) : userNames[i], item.withTrimmedLeft (40.0f), juce::Justification::centredLeft, true);
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        auto [gIdx, iIdx] = hit (e.position);
        if (gIdx != hoverGroup || iIdx != hoverItem)
        {
            hoverGroup = gIdx;
            hoverItem = iIdx;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override { hoverGroup = hoverItem = -1; repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        auto [gIdx, iIdx] = hit (e.position);
        if (gIdx < 0)
            return;
        if (gIdx < 5 && onFactory)
            onFactory (gIdx * 10 + iIdx);
        else if (gIdx == 5 && onUser)
            onUser (userNames[iIdx]);
    }

    std::function<void (int)> onFactory;
    std::function<void (const juce::String&)> onUser;
    std::function<void()> onClose;

private:
    juce::Rectangle<float> columnRect (int gi) const
    {
        const float w = ((float) getWidth() - 36.0f - 5.0f * 14.0f) / 6.0f;
        return { 18.0f + (float) gi * (w + 14.0f), 56.0f, w, (float) getHeight() - 70.0f };
    }

    juce::Rectangle<float> itemRect (int gi, int i) const
    {
        const auto col = columnRect (gi);
        return { col.getX(), col.getY() + 26.0f + (float) i * 29.0f, col.getWidth(), 26.0f };
    }

    std::pair<int, int> hit (juce::Point<float> p) const
    {
        for (int gi = 0; gi < 6; ++gi)
        {
            const int count = gi < 5 ? 10 : userNames.size();
            for (int i = 0; i < count; ++i)
                if (itemRect (gi, i).contains (p))
                    return { gi, i };
        }
        return { -1, -1 };
    }

    org::PresetManager& presets;
    juce::StringArray userNames;
    FlatButton close { "CLOSE" };
    int hoverGroup = -1, hoverItem = -1;
};

//==============================================================================
class SaveBox : public juce::Component
{
public:
    SaveBox()
    {
        editor.setFont (theme::body (15.0f));
        editor.setColour (juce::TextEditor::backgroundColourId, theme::screen);
        editor.setColour (juce::TextEditor::outlineColourId, theme::line);
        editor.setColour (juce::TextEditor::focusedOutlineColourId, theme::moss);
        editor.setColour (juce::TextEditor::textColourId, theme::bone);
        editor.setColour (juce::TextEditor::highlightColourId, theme::moss.withAlpha (0.4f));
        editor.setColour (juce::CaretComponent::caretColourId, theme::bone);
        editor.setInputRestrictions (32);
        editor.onReturnKey = [this] { commit(); };
        editor.onEscapeKey = [this] { if (onCancel) onCancel(); };
        save.onClick = [this] { commit(); };
        cancel.onClick = [this] { if (onCancel) onCancel(); };
        for (auto* c : std::initializer_list<juce::Component*> { &editor, &save, &cancel })
            addAndMakeVisible (c);
    }

    void begin (const juce::String& suggestion)
    {
        editor.setText (suggestion, false);
        editor.selectAll();
        editor.grabKeyboardFocus();
    }

    void resized() override
    {
        editor.setBounds (12, 30, getWidth() - 24, 32);
        save.setBounds (12, 72, 90, 28);
        cancel.setBounds (110, 72, 90, 28);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (r.translated (0.0f, 6.0f), 10.0f);
        g.setColour (theme::bark2);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (Colour (0xff4a523b));
        g.drawRoundedRectangle (r, 10.0f, 1.0f);
        g.setColour (theme::boneFaint);
        g.setFont (theme::label (10.0f, 0.24f));
        g.drawText ("SAVE PRESET AS", 12, 8, getWidth() - 24, 18, juce::Justification::centredLeft);
    }

    std::function<void (const juce::String&)> onSave;
    std::function<void()> onCancel;

private:
    void commit()
    {
        const auto t = editor.getText().trim();
        if (t.isNotEmpty() && onSave)
            onSave (t);
    }

    juce::TextEditor editor;
    FlatButton save { "SAVE", true }, cancel { "CANCEL" };
};

//==============================================================================
class Toast : public juce::Component, private juce::Timer
{
public:
    Toast() { setInterceptsMouseClicks (false, false); }

    void show (const juce::String& t)
    {
        text = t;
        alpha = 1.0f;
        hold = 45;
        setVisible (true);
        startTimerHz (30);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto f = theme::label (12.0f, 0.08f);
        const float w = theme::textWidth (f, text) + 32.0f;
        auto r = juce::Rectangle<float> (w, 28.0f).withCentre (getLocalBounds().toFloat().getCentre());
        g.setColour (theme::moss.withAlpha (alpha));
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (theme::ground.withAlpha (alpha));
        g.setFont (f);
        g.drawText (text, r, juce::Justification::centred, false);
    }

private:
    void timerCallback() override
    {
        if (hold > 0) { --hold; return; }
        alpha -= 0.08f;
        if (alpha <= 0.0f)
        {
            stopTimer();
            setVisible (false);
        }
        repaint();
    }

    juce::String text;
    float alpha = 0.0f;
    int hold = 0;
};

//==============================================================================
// Last hits as bars: height = level, brightness = tone, tick = place. Proof that no two hits match.
class Fingerprint : public juce::Component, private juce::Timer
{
public:
    explicit Fingerprint (OrganikaAudioProcessor& p) : proc (p) { startTimerHz (20); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        theme::screenBack (g, r);
        const int total = proc.hitCount.load (std::memory_order_acquire);
        const int n = juce::jmin (total, OrganikaAudioProcessor::kHitHistory);
        const float bw = (r.getWidth() - 8.0f) / (float) OrganikaAudioProcessor::kHitHistory;
        for (int k = 0; k < OrganikaAudioProcessor::kHitHistory; ++k)
        {
            const float x = r.getX() + 4.0f + (float) k * bw;
            g.setColour (Colour (0xff14180f));
            g.fillRect (x, r.getBottom() - 6.0f, bw - 3.0f, 2.0f);
        }
        for (int k = 0; k < n; ++k)
        {
            const int idx = (total - n + k) % OrganikaAudioProcessor::kHitHistory;
            const auto& h = proc.hits[(size_t) idx];
            const float lvl = juce::Decibels::decibelsToGain (h.gain.load (std::memory_order_relaxed));
            const float tilt = h.tilt.load (std::memory_order_relaxed), pan = h.pan.load (std::memory_order_relaxed);
            const int row = h.row.load (std::memory_order_relaxed);
            const float x = r.getX() + 4.0f + (float) k * bw;
            const float hh = (r.getHeight() - 14.0f) * lvl;
            const auto c = row >= 0 ? theme::rowColours[(size_t) row] : theme::boneDim;
            g.setColour (c.withAlpha (juce::jlimit (0.3f, 1.0f, 0.65f + tilt / 12.0f)));
            g.fillRect (x, r.getBottom() - 6.0f - hh, bw - 3.0f, hh);
            g.setColour (theme::bone);
            g.fillRect (x + (bw - 3.0f) * 0.5f + pan * 8.0f - 0.75f, r.getBottom() - 8.0f - hh, 1.5f, 2.0f);
        }
    }

private:
    void timerCallback() override
    {
        const int c = proc.hitCount.load();
        if (c != last) { last = c; repaint(); }
    }

    OrganikaAudioProcessor& proc;
    int last = -1;
};

class OutMeter : public juce::Component, private juce::Timer
{
public:
    explicit OutMeter (OrganikaAudioProcessor& p) : proc (p) { startTimerHz (30); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (theme::screen);
        g.fillRoundedRectangle (r, 4.0f);
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        auto bar = r.reduced (1.5f);
        bar = bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f));
        g.setGradientFill (juce::ColourGradient (theme::moss, r.getX(), 0.0f, Colour (0xffc9924f), r.getRight(), 0.0f, false));
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (theme::line);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

private:
    void timerCallback() override
    {
        const float v = proc.outLevel.load();
        level = juce::jmax (v, level * 0.88f);
        repaint();
    }

    OrganikaAudioProcessor& proc;
    float level = 0.0f;
};
} // namespace ui

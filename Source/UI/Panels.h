#pragma once

#include "Widgets.h"
#include "../DSP/Biquad.h"

namespace ui
{
//==============================================================================
// Base for the five module screens: owns its knobs and lays them out in rows.
class ModulePanel : public juce::Component
{
public:
    ModulePanel (OrganikaAudioProcessor& p, int rowIndex) : proc (p), row (rowIndex), colour (theme::rowColours[(size_t) rowIndex]) {}

protected:
    Knob& knob (const juce::String& id, const juce::String& text, int size = 44)
    {
        knobs.push_back (std::make_unique<Knob> (proc.apvts, id, text, size, colour));
        addAndMakeVisible (*knobs.back());
        return *knobs.back();
    }

    static void placeRow (std::initializer_list<Knob*> ks, int x, int y, int step)
    {
        for (auto* k : ks)
        {
            k->setTopLeftPosition (x + (step - k->getWidth()) / 2, y);
            x += step;
        }
    }

    void note (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r) const
    {
        g.setColour (theme::boneDim);
        g.setFont (theme::body (13.0f));
        g.drawFittedText (text, r, juce::Justification::topLeft, 6, 1.0f);
    }

    OrganikaAudioProcessor& proc;
    int row;
    Colour colour;
    std::vector<std::unique_ptr<Knob>> knobs;
};

// Repaints itself when any of the watched parameters move.
class ParamWatcher : public juce::Component, private juce::Timer
{
public:
    ParamWatcher (APVTS& s, juce::StringArray ids, int hz = 20) : apvts (s), watched (std::move (ids)) { startTimerHz (hz); }

protected:
    float get (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    virtual bool animating() const { return false; }

    APVTS& apvts;

private:
    void timerCallback() override
    {
        double h = 0.0;
        for (int i = 0; i < watched.size(); ++i)
            h += (double) apvts.getRawParameterValue (watched[i])->load() * (double) (i + 1) * 1.37;
        if (h != lastHash || animating())
        {
            lastHash = h;
            repaint();
        }
    }

    juce::StringArray watched;
    double lastHash = -1.0;
};

//==============================================================================
// 1 ENVELOPE
class EnvDisplay : public ParamWatcher
{
public:
    explicit EnvDisplay (APVTS& s, Colour c)
        : ParamWatcher (s, { "envMode", "envA", "envD", "envS", "envR", "envCurve", "envSnap" }), colour (c) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        theme::screenBack (g, r);
        theme::faintGrid (g, r, 10);
        auto a = r.reduced (16.0f, 18.0f);

        const bool shortMode = get ("envMode") > 0.5f;
        const float k = 1.5f * std::pow (4.0f, get ("envCurve") * 0.01f);
        std::vector<juce::Point<float>> pts;
        if (shortMode)
        {
            const float len = 0.08f + get ("envSnap") / 400.0f * 0.9f;
            pts.push_back ({ 0.0f, 1.0f });
            for (int i = 0; i <= 80; ++i)
            {
                const float t = (float) i / 80.0f;
                pts.push_back ({ t * len, std::pow (1.0f - t, k) });
            }
            pts.push_back ({ 1.0f, 0.0f });
        }
        else
        {
            const float A = get ("envA") / 500.0f * 0.25f, D = get ("envD") / 1500.0f * 0.3f, H = 0.2f, R = get ("envR") / 2000.0f * 0.3f;
            const float S = get ("envS") * 0.01f, tot = juce::jmax (1.0f, A + D + H + R);
            auto sc = [tot] (float v) { return v / tot * 0.98f; };
            pts.push_back ({ 0.0f, 0.0f });
            pts.push_back ({ sc (A), 1.0f });
            for (int i = 1; i <= 40; ++i)
            {
                const float t = (float) i / 40.0f;
                pts.push_back ({ sc (A + D * t), S + (1.0f - S) * std::pow (1.0f - t, k) });
            }
            pts.push_back ({ sc (A + D + H), S });
            for (int i = 1; i <= 40; ++i)
            {
                const float t = (float) i / 40.0f;
                pts.push_back ({ sc (A + D + H + R * t), S * std::pow (1.0f - t, k) });
            }
        }

        auto map = [&a] (juce::Point<float> p) { return juce::Point<float> (a.getX() + p.x * a.getWidth(), a.getBottom() - p.y * a.getHeight()); };
        juce::Path stroke, fill;
        stroke.startNewSubPath (map (pts.front()));
        fill.startNewSubPath (map ({ pts.front().x, 0.0f }));
        for (auto& p : pts)
        {
            stroke.lineTo (map (p));
            fill.lineTo (map (p));
        }
        fill.lineTo (map ({ pts.back().x, 0.0f }));
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.45f), 0.0f, a.getY(), colour.withAlpha (0.0f), 0.0f, a.getBottom(), false));
        g.fillPath (fill);
        g.setColour (colour);
        g.strokePath (stroke, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (theme::bone);
        g.setFont (theme::mono (10.5f));
        g.drawText (shortMode ? juce::String ("SHORT  A 0  S 0") : juce::String ("LONG  ADSR"), r.reduced (14.0f, 10.0f), juce::Justification::topLeft, false);
    }

private:
    Colour colour;
};

class EnvPanel : public ModulePanel
{
public:
    EnvPanel (OrganikaAudioProcessor& p) : ModulePanel (p, 0), display (p.apvts, colour), sw (p.apvts, "envMode", "LONG", "SHORT", colour)
    {
        addAndMakeVisible (display);
        addAndMakeVisible (sw);
        kA = &knob ("envA", "ATTACK");
        kD = &knob ("envD", "DECAY");
        kS = &knob ("envS", "SUSTAIN");
        kR = &knob ("envR", "RELEASE");
        kC = &knob ("envCurve", "CURVE");
        kSn = &knob ("envSnap", "SNAP");
        kDr = &knob ("envDrift", "DRIFT");
        sw.onChange = [this] (bool s) { shortMode = s; updateLook(); repaint(); };
        shortMode = p.apvts.getRawParameterValue ("envMode")->load() > 0.5f;
        updateLook();
    }

    void resized() override
    {
        display.setBounds (14, 40, 722, 262);
        placeRow ({ kA, kD, kS, kR, kC, kSn, kDr }, 24, 322, 96);
        sw.setBounds (776, 46, 354, 60);
    }

    void paint (juce::Graphics& g) override
    {
        theme::card (g, { 0.0f, 0.0f, 750.0f, 450.0f }, "SHAPE", "the curve each active step applies to the hit");
        theme::card (g, { 762.0f, 0.0f, 382.0f, 450.0f }, "CHARACTER");
        note (g, shortMode ? "SHORT: attack and sustain are locked at zero. Only an exponential decay is left, set by Snap and Curve. Made for tight, choked cymbals and ticks."
                           : "LONG: a full ADSR. A slow attack softens the stick, sustain holds for one sixteenth, and a long release lets a ride bloom between hits.",
              { 778, 128, 350, 120 });
        note (g, "Drift changes the decay length a little on every hit, so no two tails are the same.", { 778, 262, 350, 60 });
    }

private:
    void updateLook()
    {
        for (auto* k : { kA, kS, kR, kD })
            k->setEnabledLook (! shortMode);
        kSn->setEnabledLook (shortMode);
    }

    EnvDisplay display;
    SlideSwitch sw;
    Knob *kA, *kD, *kS, *kR, *kC, *kSn, *kDr;
    bool shortMode = true;
};

//==============================================================================
// 2 STEREO
class EqDisplay : public ParamWatcher
{
public:
    EqDisplay (APVTS& s, Colour c) : ParamWatcher (s, { "stLow", "stMid", "stHigh", "stMonoHz" }), colour (c) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        theme::screenBack (g, r);
        theme::faintGrid (g, r, 10);
        constexpr double sr = 48000.0;
        org::Biquad ls, pk, hs;
        ls.lowShelf (sr, 220.0, get ("stLow"));
        pk.peak (sr, 2500.0, 0.8, get ("stMid"));
        hs.highShelf (sr, 8000.0, get ("stHigh"));

        auto fx = [&r] (double f) { return r.getX() + (float) (std::log (f / 20.0) / std::log (1000.0)) * r.getWidth(); };
        auto fy = [&r] (double db) { return r.getCentreY() - (float) db / 15.0f * (r.getHeight() * 0.5f - 10.0f); };

        g.setColour (colour.withAlpha (0.08f));
        g.fillRect (r.withRight (fx (get ("stMonoHz"))));
        g.setColour (Colour (0xff2a3322));
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());

        g.setColour (theme::boneFaint);
        g.setFont (theme::mono (9.5f));
        g.drawText ("mono", r.reduced (6.0f, 4.0f), juce::Justification::topLeft, false);
        for (auto f : { 100.0, 1000.0, 10000.0 })
            g.drawText (f >= 1000.0 ? juce::String ((int) (f / 1000.0)) + "k" : juce::String ((int) f), (int) fx (f) + 3, (int) r.getBottom() - 16, 40, 12, juce::Justification::left);

        juce::Path curve, fill;
        for (int i = 0; i <= (int) r.getWidth(); i += 2)
        {
            const double f = 20.0 * std::pow (1000.0, (double) i / r.getWidth());
            const double db = ls.magnitudeDb (f, sr) + pk.magnitudeDb (f, sr) + hs.magnitudeDb (f, sr);
            const juce::Point<float> pt (r.getX() + (float) i, fy (db));
            if (i == 0) { curve.startNewSubPath (pt); fill.startNewSubPath (r.getX(), r.getBottom()); }
            curve.lineTo (pt);
            fill.lineTo (pt);
        }
        fill.lineTo (r.getRight(), r.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.35f), 0.0f, r.getY(), colour.withAlpha (0.0f), 0.0f, r.getBottom(), false));
        g.fillPath (fill);
        g.setColour (colour);
        g.strokePath (curve, juce::PathStrokeType (2.0f));

        const std::pair<double, const char*> nodes[] { { 220.0, "stLow" }, { 2500.0, "stMid" }, { 8000.0, "stHigh" } };
        for (auto& [f, id] : nodes)
        {
            const float cx = fx (f), cy = fy (get (id));
            g.setColour (theme::screen);
            g.fillEllipse (cx - 5.0f, cy - 5.0f, 10.0f, 10.0f);
            g.setColour (theme::bone);
            g.drawEllipse (cx - 5.0f, cy - 5.0f, 10.0f, 10.0f, 1.5f);
        }
    }

private:
    Colour colour;
};

class Goniometer : public juce::Component, private juce::Timer
{
public:
    Goniometer (OrganikaAudioProcessor& p, Colour c) : proc (p), colour (c) { startTimerHz (30); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        theme::screenBack (g, r);
        const auto c = r.getCentre();
        const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f - 12.0f;
        g.setColour (Colour (0xff232a1c));
        g.drawLine (c.x, c.y - rad, c.x, c.y + rad);
        g.drawLine (c.x - rad, c.y, c.x + rad, c.y);
        g.setColour (theme::boneFaint);
        g.setFont (theme::mono (9.0f));
        g.drawText ("L", (int) (c.x - rad * 0.75f) - 10, (int) (c.y - rad * 0.75f) - 6, 12, 12, juce::Justification::centred);
        g.drawText ("R", (int) (c.x + rad * 0.75f), (int) (c.y - rad * 0.75f) - 6, 12, 12, juce::Justification::centred);

        const int pos = proc.scopePos.load (std::memory_order_relaxed);
        for (int i = 0; i < OrganikaAudioProcessor::kScope; ++i)
        {
            const int k = (pos + i) % OrganikaAudioProcessor::kScope;
            const float l = proc.scopeL[(size_t) k].load (std::memory_order_relaxed);
            const float rr = proc.scopeR[(size_t) k].load (std::memory_order_relaxed);
            const float x = juce::jlimit (-1.0f, 1.0f, (l - rr) * 0.9f), y = juce::jlimit (-1.0f, 1.0f, (l + rr) * 0.9f);
            g.setColour (colour.withAlpha (0.15f + 0.5f * (float) i / (float) OrganikaAudioProcessor::kScope));
            g.fillRect (c.x + x * rad, c.y - y * rad, 1.6f, 1.6f);
        }
    }

private:
    void timerCallback() override { repaint(); }

    OrganikaAudioProcessor& proc;
    Colour colour;
};

class StereoPanel : public ModulePanel
{
public:
    StereoPanel (OrganikaAudioProcessor& p)
        : ModulePanel (p, 1), eq (p.apvts, colour), gonio (p, colour), mode (p.apvts, "stMode", { "SIDES", "PAN", "MONO" }, colour)
    {
        addAndMakeVisible (eq);
        addAndMakeVisible (gonio);
        addAndMakeVisible (mode);
        kLow = &knob ("stLow", "LOW");
        kMid = &knob ("stMid", "MID");
        kHigh = &knob ("stHigh", "HIGH");
        kMono = &knob ("stMonoHz", "MONO BELOW");
        kWidth = &knob ("stWidth", "WIDTH");
        kPan = &knob ("stPan", "PAN");
        kHaas = &knob ("stHaas", "HAAS");
        kWander = &knob ("stWander", "WANDER");
    }

    void resized() override
    {
        eq.setBounds (14, 40, 722, 262);
        placeRow ({ kLow, kMid, kHigh, kMono }, 24, 322, 110);
        gonio.setBounds (776, 40, 354, 196);
        mode.setBounds (776, 248, 354, 30);
        placeRow ({ kWidth, kPan, kHaas, kWander }, 776, 300, 88);
    }

    void paint (juce::Graphics& g) override
    {
        theme::card (g, { 0.0f, 0.0f, 750.0f, 450.0f }, "EQ", "three bands before the stereo stage");
        theme::card (g, { 762.0f, 0.0f, 382.0f, 450.0f }, "FIELD");
        note (g, "Sides flips the hit to the other side every time. Pan places it, and Wander moves that place on every hit.", { 778, 384, 350, 56 });
    }

private:
    EqDisplay eq;
    Goniometer gonio;
    Segments mode;
    Knob *kLow, *kMid, *kHigh, *kMono, *kWidth, *kPan, *kHaas, *kWander;
};

//==============================================================================
// 3 PITCH
class PitchRibbon : public ParamWatcher
{
public:
    PitchRibbon (APVTS& s, Colour c)
        : ParamWatcher (s, { "p1On", "p1Semi", "p1Fine", "p1Lvl", "p2On", "p2Semi", "p2Fine", "p2Lvl", "p3On", "p3Semi", "p3Fine", "p3Lvl", "piScatter" }),
          colour (c) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        theme::screenBack (g, r);
        auto a = r.reduced (18.0f, 10.0f);
        const float base = a.getBottom() - 14.0f, top = a.getY() + 6.0f;
        auto X = [&a] (float st) { return a.getX() + (st + 24.0f) / 48.0f * a.getWidth(); };

        for (int s = -24; s <= 24; ++s)
        {
            const bool big = s % 12 == 0;
            g.setColour (big ? Colour (0xff3a4330) : Colour (0xff1f251a));
            g.drawVerticalLine ((int) X ((float) s), big ? top : (top + base) * 0.5f, base);
            if (big || s == 7 || s == -7 || s == 5 || s == 19 || s == -19)
            {
                g.setColour (big ? theme::boneDim : theme::boneFaint);
                g.setFont (theme::mono (9.5f));
                g.drawText (org::fmtSigned ((float) s, 0), (int) X ((float) s) - 15, (int) base + 1, 30, 12, juce::Justification::centred);
            }
        }
        g.setColour (theme::bone);
        g.fillRect (X (0.0f) - 1.5f, top, 3.0f, base - top);
        g.setFont (theme::label (10.0f, 0.16f));
        g.drawText ("DRY", (int) X (0.0f) + 6, (int) top, 40, 12, juce::Justification::left);

        const float scatter = get ("piScatter") / 100.0f;
        const char* names[] { "I", "II", "III" };
        for (int v = 0; v < 3; ++v)
        {
            const juce::String p ("p" + juce::String (v + 1));
            if (apvts.getRawParameterValue (p + "On")->load() < 0.5f)
                continue;
            const float st = apvts.getRawParameterValue (p + "Semi")->load() + apvts.getRawParameterValue (p + "Fine")->load() * 0.01f;
            const float lvl = apvts.getRawParameterValue (p + "Lvl")->load() * 0.01f;
            const float x = X (st), w = juce::jmax (4.0f, scatter / 48.0f * a.getWidth() * 2.0f + 4.0f);
            const float y = base - (base - top) * lvl;
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.85f), 0.0f, y, colour.withAlpha (0.12f), 0.0f, base, false));
            g.fillRect (x - w * 0.5f, y, w, base - y);
            g.setColour (colour);
            g.fillEllipse (x - 4.0f, y - 4.0f, 8.0f, 8.0f);
            g.setColour (theme::bone);
            g.setFont (theme::label (11.0f, 0.1f));
            g.drawText (names[v], (int) x + 7, (int) y - 14, 30, 12, juce::Justification::left);
        }
    }

private:
    Colour colour;
};

class PitchPanel : public ModulePanel
{
public:
    PitchPanel (OrganikaAudioProcessor& p) : ModulePanel (p, 2), ribbon (p.apvts, colour)
    {
        addAndMakeVisible (ribbon);
        for (int v = 0; v < 3; ++v)
        {
            const juce::String id ("p" + juce::String (v + 1));
            auto& vc = voices[(size_t) v];
            vc.chip = std::make_unique<Chip> (p.apvts, id + "On", "ON", "OFF", colour);
            addAndMakeVisible (*vc.chip);
            vc.semi = &knob (id + "Semi", "SEMI", 40);
            vc.fine = &knob (id + "Fine", "FINE", 40);
            vc.lvl = &knob (id + "Lvl", "LEVEL", 40);
            vc.chip->onChange = [&vc] (bool on)
            {
                for (auto* k : { vc.semi, vc.fine, vc.lvl })
                    if (k != nullptr)
                        k->setEnabledLook (on);
            };
            vc.chip->onChange (vc.chip->isOn());
        }
        kScatter = &knob ("piScatter", "SCATTER");
        kSpread = &knob ("piSpread", "SPREAD");
        kGlide = &knob ("piGlide", "GLIDE");
        kWet = &knob ("piWet", "DRY / WET");
    }

    void resized() override
    {
        ribbon.setBounds (14, 36, 1116, 102);
        for (int v = 0; v < 3; ++v)
        {
            auto& vc = voices[(size_t) v];
            const int x = 14 + v * 244;
            vc.chip->setBounds (x + 160, 212, 62, 22);
            placeRow ({ vc.semi, vc.fine, vc.lvl }, x + 4, 254, 74);
        }
        placeRow ({ kScatter, kSpread, kGlide, kWet }, 776, 206, 88);
    }

    void paint (juce::Graphics& g) override
    {
        theme::card (g, { 0.0f, 0.0f, 1144.0f, 150.0f }, "INTERVALS", "original at 0, each voice is layered with the dry hit");
        theme::card (g, { 0.0f, 162.0f, 750.0f, 288.0f }, "VOICES");
        theme::card (g, { 762.0f, 162.0f, 382.0f, 288.0f }, "BLEND");
        const char* names[] { "VOICE I", "VOICE II", "VOICE III" };
        for (int v = 0; v < 3; ++v)
        {
            auto box = juce::Rectangle<float> (14.0f + (float) v * 244.0f, 200.0f, 232.0f, 236.0f);
            g.setColour (Colour (0xff0f130c));
            g.fillRoundedRectangle (box, 9.0f);
            g.setColour (theme::line);
            g.drawRoundedRectangle (box.reduced (0.5f), 9.0f, 1.0f);
            g.setColour (theme::bone);
            g.setFont (theme::display (13.0f, 0.14f));
            g.drawText (names[v], box.reduced (12.0f, 0.0f).withHeight (46.0f), juce::Justification::centredLeft);
        }
        note (g, "Scatter nudges every voice by a few cents on each hit, so the stack never freezes into one static chord.", { 778, 310, 350, 80 });
    }

private:
    struct VoiceControls
    {
        std::unique_ptr<Chip> chip;
        Knob *semi = nullptr, *fine = nullptr, *lvl = nullptr;
    };

    PitchRibbon ribbon;
    std::array<VoiceControls, 3> voices;
    Knob *kScatter, *kSpread, *kGlide, *kWet;
};

//==============================================================================
// 4 VOCODER
class BandDisplay : public juce::Component, private juce::Timer
{
public:
    BandDisplay (OrganikaAudioProcessor& p, Colour c) : proc (p), colour (c)
    {
        levels.fill (0.0f);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        theme::screenBack (g, r);
        const int n = juce::roundToInt (proc.apvts.getRawParameterValue ("voBands")->load());
        const bool retro = proc.apvts.getRawParameterValue ("voMode")->load() > 0.5f;
        auto a = r.reduced (12.0f, 10.0f).withTrimmedBottom (14.0f);
        const float bw = a.getWidth() / (float) n;
        for (int b = 0; b < n; ++b)
        {
            const float x = a.getX() + (float) b * bw;
            g.setColour (Colour (0xff1b2016));
            g.fillRect (x + 1.0f, a.getY(), bw - 2.0f, a.getHeight());
            const float h = a.getHeight() * juce::jlimit (0.0f, 1.0f, levels[(size_t) b]);
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.5f), 0.0f, a.getBottom(), retro ? Colour (0xffe8b066) : colour, 0.0f, a.getBottom() - h, false));
            g.fillRect (x + 1.0f, a.getBottom() - h, bw - 2.0f, h);
        }
        g.setColour (theme::boneFaint);
        g.setFont (theme::mono (9.5f));
        g.drawText (org::fmtHz (proc.apvts.getRawParameterValue ("voLow")->load()), r.reduced (12.0f, 4.0f), juce::Justification::bottomLeft, false);
        g.drawText (org::fmtHz (proc.apvts.getRawParameterValue ("voHigh")->load()), r.reduced (12.0f, 4.0f), juce::Justification::bottomRight, false);
        g.drawText (juce::String (n) + " BANDS  " + (retro ? "RETRO" : "PRECISE"), r.reduced (12.0f, 4.0f), juce::Justification::centredBottom, false);
    }

private:
    void timerCallback() override
    {
        for (int b = 0; b < org::kMaxBands; ++b)
        {
            // band envelopes are tiny; show them on a log scale
            const float v = proc.vocoder.bandLevels[(size_t) b].load (std::memory_order_relaxed);
            const float t = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (v, -72.0f) + 72.0f) / 60.0f);
            auto& l = levels[(size_t) b];
            l += (t - l) * (t > l ? 0.6f : 0.12f);
        }
        repaint();
    }

    OrganikaAudioProcessor& proc;
    Colour colour;
    std::array<float, org::kMaxBands> levels {};
};

class VocoderPanel : public ModulePanel
{
public:
    VocoderPanel (OrganikaAudioProcessor& p)
        : ModulePanel (p, 3),
          carrier (p.apvts, "voCarrier", { "NOISE", "TONE", "MODULATOR", "PITCH TRACK" }, colour),
          mode (p.apvts, "voMode", { "PRECISE", "RETRO" }, colour),
          chan (p.apvts, "voChan", { "MONO", "STEREO", "L/R" }, colour),
          enhance (p.apvts, "voEnhance", "ENHANCE", "ENHANCE", colour),
          bands (p, colour)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &carrier, &mode, &chan, &enhance, &bands })
            addAndMakeVisible (c);
        kBands = &knob ("voBands", "BANDS");
        kLow = &knob ("voLow", "LOW");
        kHigh = &knob ("voHigh", "HIGH");
        kBw = &knob ("voBw", "BW");
        kFormant = &knob ("voFormant", "FORMANT");
        kTone = &knob ("voTone", "TONE");
        kAtk = &knob ("voAttack", "ATTACK", 40);
        kRel = &knob ("voRelease", "RELEASE", 40);
        kGate = &knob ("voGate", "GATE", 40);
        kUnv = &knob ("voUnvoiced", "UNVOICED", 40);
        kSens = &knob ("voSens", "SENS.", 40);
        kDepth = &knob ("voDepth", "DEPTH", 40);
        kWet = &knob ("voWet", "DRY / WET", 40);
    }

    void resized() override
    {
        carrier.setBounds (14, 40, 440, 30);
        mode.setBounds (466, 40, 190, 30);
        chan.setBounds (668, 40, 220, 30);
        enhance.setBounds (900, 43, 110, 24);
        bands.setBounds (14, 82, 1116, 114);
        placeRow ({ kBands, kLow, kHigh, kBw, kFormant, kTone }, 16, 262, 88);
        placeRow ({ kAtk, kRel, kGate, kUnv, kSens, kDepth, kWet }, 586, 266, 78);
    }

    void paint (juce::Graphics& g) override
    {
        theme::card (g, { 0.0f, 0.0f, 1144.0f, 210.0f }, "CARRIER");
        theme::card (g, { 0.0f, 222.0f, 560.0f, 228.0f }, "FILTER BANK", "the hit is the modulator");
        theme::card (g, { 572.0f, 222.0f, 572.0f, 228.0f }, "ENVELOPE & MIX");
        note (g, "Tone and Pitch Track give the cymbal a throat. L/R uses the right input as the carrier.", { 16, 366, 530, 60 });
    }

private:
    Segments carrier, mode, chan;
    Chip enhance;
    BandDisplay bands;
    Knob *kBands, *kLow, *kHigh, *kBw, *kFormant, *kTone, *kAtk, *kRel, *kGate, *kUnv, *kSens, *kDepth, *kWet;
};

//==============================================================================
// 5 BLOOM
class XYPad : public juce::Component, private juce::Timer
{
public:
    XYPad (APVTS& s, Colour c) : apvts (s), colour (c)
    {
        ax = makeAttachment (s, "blX", [this] (float v) { x = v * 0.01f; });
        ay = makeAttachment (s, "blY", [this] (float v) { y = v * 0.01f; });
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        startTimerHz (40);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        theme::screenBack (g, r);
        g.setColour (Colour (0xff1f2619));
        for (int i = 1; i < 3; ++i)
            g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / 3.0f), r.getY() + 2.0f, r.getBottom() - 2.0f);
        g.setColour (theme::boneFaint);
        g.setFont (theme::label (10.5f, 0.2f));
        const char* zones[] { "CHORUS", "FLANGER", "PHASER" };
        for (int i = 0; i < 3; ++i)
            g.drawText (zones[i], (int) (r.getX() + r.getWidth() * (float) i / 3.0f) + 10, (int) r.getBottom() - 22, 120, 14, juce::Justification::left);
        g.drawText ("DEPTH", r.reduced (10.0f, 8.0f), juce::Justification::topLeft, false);

        const float wind = apvts.getRawParameterValue ("blWind")->load() * 0.01f;
        const float rate = apvts.getRawParameterValue ("blRate")->load();
        const float fb = apvts.getRawParameterValue ("blFb")->load() * 0.01f;
        const float wx = x + (std::sin (t * 1.3f) * 0.05f + std::sin (t * 3.1f) * 0.02f) * wind;
        const float wy = y + std::cos (t * 1.1f) * 0.05f * wind;

        for (int k = 0; k < 5; ++k)
        {
            juce::Path p;
            for (int i = 0; i <= (int) r.getWidth(); i += 4)
            {
                const float yy = r.getY() + r.getHeight() * (1.0f - wy)
                               + std::sin ((float) i / r.getWidth() * juce::MathConstants<float>::pi * (2.0f + (float) k) + t * rate * 6.0f + (float) k)
                                     * y * 30.0f * (1.0f - (float) k * 0.15f) * (1.0f + fb);
                if (i == 0) p.startNewSubPath (r.getX(), yy); else p.lineTo (r.getX() + (float) i, yy);
            }
            g.setColour (colour.withAlpha (0.08f + (float) k * 0.03f));
            g.strokePath (p, juce::PathStrokeType (1.0f));
        }

        trail.push_back ({ wx, wy });
        if (trail.size() > 40)
            trail.erase (trail.begin());
        for (size_t i = 0; i < trail.size(); ++i)
        {
            const float f = (float) i / (float) trail.size();
            const float d = 4.0f + f * 6.0f;
            g.setColour (colour.withAlpha (f * 0.5f));
            g.fillEllipse (r.getX() + trail[i].x * r.getWidth() - d * 0.5f, r.getY() + (1.0f - trail[i].y) * r.getHeight() - d * 0.5f, d, d);
        }
        const float cx = r.getX() + x * r.getWidth(), cy = r.getY() + (1.0f - y) * r.getHeight();
        g.setColour (theme::bone);
        g.drawEllipse (cx - 9.0f, cy - 9.0f, 18.0f, 18.0f, 2.0f);
        g.setColour (colour);
        g.fillEllipse (r.getX() + wx * r.getWidth() - 5.0f, r.getY() + (1.0f - wy) * r.getHeight() - 5.0f, 10.0f, 10.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        ax->beginGesture();
        ay->beginGesture();
        mouseDrag (e);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto r = getLocalBounds().toFloat();
        ax->setValueAsPartOfGesture (juce::jlimit (0.0f, 100.0f, (e.position.x - r.getX()) / r.getWidth() * 100.0f));
        ay->setValueAsPartOfGesture (juce::jlimit (0.0f, 100.0f, (1.0f - (e.position.y - r.getY()) / r.getHeight()) * 100.0f));
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        ax->endGesture();
        ay->endGesture();
    }

private:
    void timerCallback() override
    {
        t += 0.025f;
        repaint();
    }

    APVTS& apvts;
    Colour colour;
    std::unique_ptr<juce::ParameterAttachment> ax, ay;
    float x = 0.3f, y = 0.5f, t = 0.0f;
    std::vector<juce::Point<float>> trail;
};

class BloomPanel : public ModulePanel
{
public:
    BloomPanel (OrganikaAudioProcessor& p) : ModulePanel (p, 4), pad (p.apvts, colour)
    {
        addAndMakeVisible (pad);
        kRate = &knob ("blRate", "RATE");
        kFb = &knob ("blFb", "FEEDBACK");
        kWind = &knob ("blWind", "WIND");
        kMoss = &knob ("blMoss", "MOSS");
        kGrain = &knob ("blGrain", "GRAIN");
        kWet = &knob ("blWet", "DRY / WET");
    }

    void resized() override
    {
        pad.setBounds (14, 40, 722, 396);
        placeRow ({ kRate, kFb, kWind }, 776, 48, 118);
        placeRow ({ kMoss, kGrain, kWet }, 776, 140, 118);
    }

    void paint (juce::Graphics& g) override
    {
        theme::card (g, { 0.0f, 0.0f, 750.0f, 450.0f }, "MORPH", "drag the seed: chorus, flanger, phaser; up = deeper");
        theme::card (g, { 762.0f, 0.0f, 382.0f, 450.0f }, "GROWTH");
        note (g, "Wind pushes the seed somewhere new on every hit. Moss is a dark filter that wanders slowly. Grain adds soft, tape-like saturation to the wet signal.",
              { 778, 246, 350, 120 });
    }

private:
    XYPad pad;
    Knob *kRate, *kFb, *kWind, *kMoss, *kGrain, *kWet;
};

//==============================================================================
inline void drawEditIcon (juce::Graphics& g, juce::Rectangle<float> r, Colour c)
{
    const auto m = r.getCentre();
    g.setColour (c);
    g.drawLine (m.x - 7.0f, m.y - 4.0f, m.x + 7.0f, m.y - 4.0f, 1.5f);
    g.drawLine (m.x - 7.0f, m.y + 4.0f, m.x + 7.0f, m.y + 4.0f, 1.5f);
    g.setColour (theme::bark2);
    g.fillEllipse (m.x + 1.0f, m.y - 6.5f, 5.0f, 5.0f);
    g.fillEllipse (m.x - 6.0f, m.y + 1.5f, 5.0f, 5.0f);
    g.setColour (c);
    g.drawEllipse (m.x + 1.0f, m.y - 6.5f, 5.0f, 5.0f, 1.4f);
    g.drawEllipse (m.x - 6.0f, m.y + 1.5f, 5.0f, 5.0f, 1.4f);
}

// One lane on the main screen: power, name, settings button, steps, amount.
class RowStrip : public juce::Component
{
public:
    RowStrip (OrganikaAudioProcessor& p, int r)
        : row (r), colour (theme::rowColours[(size_t) r]),
          power (p.apvts, org::rowOnId (r), colour),
          grid (p, r, false),
          amount (p.apvts, org::rowAmtId (r), "AMOUNT", 38, colour)
    {
        edit.icon = drawEditIcon;
        edit.accent = colour;
        edit.onClick = [this] { if (onEdit) onEdit (row); };
        power.onChange = [this] (bool) { repaint(); };
        for (auto* c : std::initializer_list<juce::Component*> { &power, &edit, &grid, &amount })
            addAndMakeVisible (c);
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void resized() override
    {
        const int h = getHeight();
        power.setBounds (10, h / 2 - 16, 32, 32);
        edit.setBounds (150, h / 2 - 15, 30, 30);
        grid.setBounds (192, 4, getWidth() - 192 - 84, h - 8);
        amount.setTopLeftPosition (getWidth() - 76, h / 2 - amount.getHeight() / 2);
    }

    void paint (juce::Graphics& g) override
    {
        auto name = nameRect().toFloat();
        g.setColour (power.isOn() ? theme::bone : theme::boneFaint);
        g.setFont (theme::display (16.0f, 0.1f));
        g.drawText (org::kRowNames[row], name.withTrimmedBottom (name.getHeight() * 0.45f), juce::Justification::bottomLeft, false);
        g.setColour (power.isOn() ? colour : colour.withAlpha (0.4f));
        g.setFont (theme::label (9.5f, 0.16f));
        g.drawText (org::kRowSubs[row], name.withTrimmedTop (name.getHeight() * 0.58f), juce::Justification::topLeft, false);
        if (row > 0)
        {
            g.setColour (Colour (0xff23281c));
            g.drawHorizontalLine (0, 10.0f, (float) getWidth() - 10.0f);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && nameRect().contains (e.getPosition()) && onEdit)
            onEdit (row);
    }

    std::function<void (int)> onEdit;

private:
    juce::Rectangle<int> nameRect() const { return { 48, 0, 100, getHeight() }; }

    int row;
    Colour colour;
    PowerDot power;
    FlatButton edit;
    StepGrid grid;
    Knob amount;
};

class SequencerView : public juce::Component
{
public:
    explicit SequencerView (OrganikaAudioProcessor& p) : proc (p)
    {
        for (int r = 0; r < org::kRows; ++r)
        {
            rows[(size_t) r] = std::make_unique<RowStrip> (p, r);
            addAndMakeVisible (*rows[(size_t) r]);
        }
        barsAttachment = makeAttachment (p.apvts, "bars", [this] (float) { repaint(); });
    }

    void setOnEdit (std::function<void (int)> f)
    {
        for (auto& r : rows)
            r->onEdit = f;
    }

    void resized() override
    {
        const int top = 26, h = (getHeight() - top) / org::kRows;
        for (int r = 0; r < org::kRows; ++r)
            rows[(size_t) r]->setBounds (0, top + r * h, getWidth(), h);
    }

    void paint (juce::Graphics& g) override
    {
        const int n = numStepsOf (proc.apvts);
        const auto rects = stepRects ({ 192.0f, 6.0f, (float) getWidth() - 192.0f - 84.0f, 16.0f }, n, false);
        g.setFont (theme::mono (9.5f));
        for (int i = 0; i < n; i += 4)
        {
            g.setColour (i % 16 == 0 ? theme::boneDim : theme::boneFaint);
            g.drawText (juce::String (i / 16 + 1) + "." + juce::String ((i / 4) % 4 + 1), rects[(size_t) i].withWidth (40.0f).translated (2.0f, 0.0f),
                        juce::Justification::bottomLeft, false);
        }
        g.setColour (theme::boneFaint);
        g.setFont (theme::label (9.5f, 0.2f));
        g.drawText ("AMOUNT", getWidth() - 84, 6, 84, 16, juce::Justification::centred);
    }

private:
    OrganikaAudioProcessor& proc;
    std::array<std::unique_ptr<RowStrip>, org::kRows> rows;
    std::unique_ptr<juce::ParameterAttachment> barsAttachment;
};

//==============================================================================
// Round numbered tabs for jumping between modules.
class ModuleTabs : public juce::Component
{
public:
    void setCurrent (int r) { current = r; repaint(); }

    void paint (juce::Graphics& g) override
    {
        for (int i = 0; i < org::kRows; ++i)
        {
            auto r = cell (i).reduced (1.0f);
            const auto c = theme::rowColours[(size_t) i];
            g.setColour (i == current ? c : juce::Colours::transparentBlack);
            g.fillEllipse (r);
            g.setColour (c);
            g.drawEllipse (r, 1.5f);
            g.setColour (i == current ? theme::ground : theme::boneDim);
            g.setFont (theme::mono (11.0f));
            g.drawText (juce::String (i + 1), r, juce::Justification::centred, false);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < org::kRows; ++i)
            if (cell (i).contains (e.position) && onSelect)
                onSelect (i);
    }

    std::function<void (int)> onSelect;

private:
    juce::Rectangle<float> cell (int i) const { return { (float) i * 36.0f, 0.0f, 30.0f, 30.0f }; }
    int current = 0;
};

class ModuleView : public juce::Component
{
public:
    explicit ModuleView (OrganikaAudioProcessor& p)
        : envPanel (p), stereoPanel (p), pitchPanel (p), vocoderPanel (p), bloomPanel (p)
    {
        back.icon = [] (juce::Graphics& g, juce::Rectangle<float> r, Colour c)
        {
            juce::Path a;
            const float x = r.getX() + 14.0f, y = r.getCentreY();
            a.startNewSubPath (x + 4.0f, y - 5.0f);
            a.lineTo (x - 1.0f, y);
            a.lineTo (x + 4.0f, y + 5.0f);
            g.setColour (c);
            g.strokePath (a, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        };
        back.onClick = [this] { if (onBack) onBack(); };
        tabs.onSelect = [this] (int r) { setRow (r); };
        addAndMakeVisible (back);
        addAndMakeVisible (tabs);

        panels = { &envPanel, &stereoPanel, &pitchPanel, &vocoderPanel, &bloomPanel };
        for (int r = 0; r < org::kRows; ++r)
        {
            const auto c = theme::rowColours[(size_t) r];
            powers[(size_t) r] = std::make_unique<PowerDot> (p.apvts, org::rowOnId (r), c);
            amounts[(size_t) r] = std::make_unique<Knob> (p.apvts, org::rowAmtId (r), "AMOUNT", 34, c);
            grids[(size_t) r] = std::make_unique<StepGrid> (p, r, true);
            addChildComponent (*powers[(size_t) r]);
            addChildComponent (*amounts[(size_t) r]);
            addChildComponent (*grids[(size_t) r]);
            addChildComponent (panels[(size_t) r]);
        }
        setRow (0);
    }

    void setRow (int r)
    {
        row = juce::jlimit (0, org::kRows - 1, r);
        for (int i = 0; i < org::kRows; ++i)
        {
            powers[(size_t) i]->setVisible (i == row);
            amounts[(size_t) i]->setVisible (i == row);
            grids[(size_t) i]->setVisible (i == row);
            panels[(size_t) i]->setVisible (i == row);
        }
        tabs.setCurrent (row);
        repaint();
    }

    int getRow() const { return row; }

    void resized() override
    {
        back.setBounds (18, 8, 130, 30);
        tabs.setBounds (getWidth() - 18 - 64 - 186, 8, 180, 30);
        for (int i = 0; i < org::kRows; ++i)
        {
            powers[(size_t) i]->setBounds (158, 7, 32, 32);
            amounts[(size_t) i]->setTopLeftPosition (getWidth() - 18 - 62, 0);
            grids[(size_t) i]->setBounds (28, 60, getWidth() - 56, 40);
            panels[(size_t) i]->setBounds (18, 122, 1144, 450);
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto c = theme::rowColours[(size_t) row];
        g.setColour (theme::bone);
        g.setFont (theme::display (23.0f, 0.12f));
        g.drawText (org::kRowNames[row], 198, 2, 300, 26, juce::Justification::centredLeft);
        g.setColour (c);
        g.setFont (theme::label (10.5f, 0.2f));
        g.drawText (org::kRowSubs[row], 198, 27, 300, 14, juce::Justification::centredLeft);

        auto strip = juce::Rectangle<float> (18.0f, 54.0f, (float) getWidth() - 36.0f, 52.0f);
        g.setColour (Colour (0xff0d100a));
        g.fillRoundedRectangle (strip, 9.0f);
        g.setColour (theme::line);
        g.drawRoundedRectangle (strip.reduced (0.5f), 9.0f, 1.0f);
    }

    std::function<void()> onBack;

private:
    FlatButton back { "SEQUENCER" };
    ModuleTabs tabs;
    EnvPanel envPanel;
    StereoPanel stereoPanel;
    PitchPanel pitchPanel;
    VocoderPanel vocoderPanel;
    BloomPanel bloomPanel;
    std::array<juce::Component*, org::kRows> panels {};
    std::array<std::unique_ptr<PowerDot>, org::kRows> powers;
    std::array<std::unique_ptr<Knob>, org::kRows> amounts;
    std::array<std::unique_ptr<StepGrid>, org::kRows> grids;
    int row = 0;
};
} // namespace ui

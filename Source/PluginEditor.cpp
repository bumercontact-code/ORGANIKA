#include "PluginEditor.h"

using juce::Colour;

namespace
{
constexpr int kHeaderH = 88, kFooterH = 56;
const juce::Rectangle<int> kBody { 0, kHeaderH, Canvas::kWidth, Canvas::kHeight - kHeaderH - kFooterH };
} // namespace

//==============================================================================
Canvas::Canvas (OrganikaAudioProcessor& p)
    : proc (p),
      bars (p.apvts, "bars", { "1 BAR", "2 BARS", "4 BARS" }, theme::moss),
      lifeKnob (p.apvts, "life", "LIFE", 46, theme::moss),
      mixKnob (p.apvts, "mix", "MIX", 34, theme::boneDim),
      outKnob (p.apvts, "output", "OUT", 34, theme::boneDim),
      staticChip (p.apvts, "static", "STATIC LOOP", "COMPARE: STATIC", Colour (0xffc9924f)),
      seqView (p),
      moduleView (p),
      fingerprint (p),
      meter (p),
      library (p.presets)
{
    setLookAndFeel (&lnf);
    setSize (kWidth, kHeight);

    for (auto* c : std::initializer_list<juce::Component*> { &presetBar, &saveButton, &bars, &lifeKnob, &mixKnob, &outKnob, &staticChip,
                                                             &seqView, &fingerprint, &meter })
        addAndMakeVisible (c);
    addChildComponent (moduleView);
    addChildComponent (library);
    addChildComponent (saveBox);
    addChildComponent (toast);

    // ---- navigation
    seqView.setOnEdit ([this] (int r) { showModule (r); });
    moduleView.onBack = [this] { showSequencer(); };

    // ---- presets
    presetBar.onPrev = [this]
    {
        const int i = proc.presets.getFactoryIndex();
        loadFactory (i < 0 ? (int) org::factoryPresets().size() - 1 : i - 1);
    };
    presetBar.onNext = [this] { loadFactory (proc.presets.getFactoryIndex() + 1); };
    presetBar.onOpen = [this] { showLibrary (! library.isVisible()); };
    library.onFactory = [this] (int i) { loadFactory (i); showLibrary (false); };
    library.onUser = [this] (const juce::String& name)
    {
        if (proc.presets.loadUser (name))
            toast.show ("LOADED " + name.toUpperCase());
        syncPresetText();
        showLibrary (false);
    };
    library.onClose = [this] { showLibrary (false); };

    saveButton.onClick = [this]
    {
        showLibrary (false);
        saveBox.setVisible (true);
        saveBox.toFront (true);
        saveBox.begin ((proc.presets.getName() + " Var").substring (0, 32));
    };
    saveBox.onSave = [this] (const juce::String& name)
    {
        proc.presets.saveUser (name);
        saveBox.setVisible (false);
        syncPresetText();
        toast.show ("SAVED \"" + name + "\"");
    };
    saveBox.onCancel = [this] { saveBox.setVisible (false); };

    syncPresetText();
    startTimerHz (10);
}

Canvas::~Canvas()
{
    setLookAndFeel (nullptr);
}

void Canvas::resized()
{
    presetBar.setBounds (292, 22, 300, 46);
    saveButton.setBounds (602, 30, 66, 30);
    bars.setBounds (692, 36, 216, 30);
    lifeKnob.setTopLeftPosition (912, 6);
    mixKnob.setTopLeftPosition (1044, 14);
    outKnob.setTopLeftPosition (1104, 14);

    seqView.setBounds (kBody.reduced (10, 6));
    moduleView.setBounds (kBody);
    library.setBounds (kBody);
    saveBox.setBounds (kWidth - 18 - 300, kHeaderH - 6, 300, 112);
    toast.setBounds (0, kHeight - kFooterH - 44, kWidth, 36);

    const int fy = kHeight - kFooterH;
    fingerprint.setBounds (128, fy + 10, 170, 36);
    staticChip.setBounds (316, fy + 16, 150, 24);
    meter.setBounds (kWidth - 18 - 130, fy + 24, 130, 9);
}

void Canvas::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    // wooden frame around a forest-floor panel
    theme::fillWood (g, r, 14.0f);
    auto panel = r.reduced (9.0f);
    g.setGradientFill (juce::ColourGradient (Colour (0xff1b2116), 0.0f, panel.getY(), Colour (0xff12160e), 0.0f, panel.getBottom(), false));
    g.fillRoundedRectangle (panel, 9.0f);
    g.setGradientFill (juce::ColourGradient (Colour (0x3326301d), panel.getCentreX(), panel.getY() - 60.0f, Colour (0x0026301d), panel.getCentreX(), panel.getY() + 260.0f, true));
    g.fillRoundedRectangle (panel, 9.0f);
    theme::fillGrain (g, panel, 1.0f, 9.0f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (panel, 9.0f, 1.2f);

    // ---- header
    g.setColour (theme::line);
    g.drawHorizontalLine (kHeaderH, panel.getX(), panel.getRight());
    theme::drawLogo (g, { 44.0f, 45.0f }, 46.0f);
    g.setColour (theme::bone);
    g.setFont (theme::display (27.0f, 0.14f));
    g.drawText ("ORGANIKA", 76, 16, 220, 30, juce::Justification::centredLeft);
    g.setColour (theme::boneDim);
    g.setFont (theme::label (10.0f, 0.32f));
    g.drawText ("EFFECT SEQUENCER", 77, 45, 220, 14, juce::Justification::centredLeft);
    g.setColour (theme::moss);
    g.setFont (theme::label (9.5f, 0.38f));
    g.drawText ("BY NICEGUY", 77, 60, 220, 13, juce::Justification::centredLeft);

    g.setColour (theme::boneFaint);
    g.setFont (theme::label (9.5f, 0.24f));
    g.drawText ("LENGTH", 692, 18, 120, 14, juce::Justification::centredLeft);

    // LIFE box
    auto life = juce::Rectangle<float> (906.0f, 12.0f, 130.0f, 68.0f);
    g.setGradientFill (juce::ColourGradient (Colour (0xff1e2717), 0.0f, life.getY(), Colour (0xff151b10), 0.0f, life.getBottom(), false));
    g.fillRoundedRectangle (life, 10.0f);
    g.setColour (Colour (0xff3d4a2b));
    g.drawRoundedRectangle (life.reduced (0.5f), 10.0f, 1.0f);
    g.setColour (theme::moss);
    g.setFont (theme::display (14.0f, 0.14f));
    g.drawText ("LIFE", 974, 20, 60, 18, juce::Justification::centredLeft);
    g.setColour (theme::boneDim);
    g.setFont (theme::body (11.0f));
    g.drawFittedText ("every hit a little different", 974, 38, 58, 36, juce::Justification::topLeft, 3, 1.0f);

    // ---- footer
    const float fy = (float) (kHeight - kFooterH);
    g.setColour (theme::line);
    g.drawHorizontalLine ((int) fy, panel.getX(), panel.getRight());
    g.setColour (Colour (0x660e110b));
    g.fillRect (panel.withTop (fy + 1.0f).withTrimmedBottom (1.0f).reduced (1.0f, 0.0f));
    g.setColour (theme::boneFaint);
    g.setFont (theme::label (9.5f, 0.22f));
    g.drawText ("HIT", 24, (int) fy + 12, 100, 14, juce::Justification::centredLeft);
    g.drawText ("FINGERPRINT", 24, (int) fy + 26, 100, 14, juce::Justification::centredLeft);
    g.drawText ("OUT", kWidth - 18 - 130, (int) fy + 8, 60, 14, juce::Justification::centredLeft);
    g.setFont (theme::body (12.5f));
    g.drawFittedText ("Click a step to switch it on. Hover and drag up or down, or scroll, to set its mix. Double-click a knob to reset it.",
                      490, (int) fy + 8, 520, 40, juce::Justification::centredLeft, 2, 1.0f);
}

void Canvas::timerCallback()
{
    if (proc.presets.getName() != lastPresetName)
        syncPresetText();
}

void Canvas::syncPresetText()
{
    lastPresetName = proc.presets.getName();
    presetBar.setText (proc.presets.getCategory(), lastPresetName);
}

void Canvas::loadFactory (int index)
{
    proc.presets.loadFactory (index);
    syncPresetText();
}

void Canvas::showLibrary (bool show)
{
    if (show)
    {
        saveBox.setVisible (false);
        library.refresh();
        library.toFront (false);
    }
    library.setVisible (show);
}

void Canvas::showModule (int row)
{
    moduleView.setRow (row);
    seqView.setVisible (false);
    moduleView.setVisible (true);
}

void Canvas::showSequencer()
{
    moduleView.setVisible (false);
    seqView.setVisible (true);
}

//==============================================================================
OrganikaEditor::OrganikaEditor (OrganikaAudioProcessor& p)
    : AudioProcessorEditor (&p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    setResizeLimits (Canvas::kWidth / 2, Canvas::kHeight / 2, Canvas::kWidth * 2, Canvas::kHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) Canvas::kWidth / (double) Canvas::kHeight);
    setSize (Canvas::kWidth, Canvas::kHeight);
}

void OrganikaEditor::resized()
{
    const float s = juce::jmin ((float) getWidth() / (float) Canvas::kWidth, (float) getHeight() / (float) Canvas::kHeight);
    canvas.setTransform (juce::AffineTransform::scale (s));
    canvas.setTopLeftPosition (0, 0);
}

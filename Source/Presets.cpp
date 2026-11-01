#include "Presets.h"
#include "Parameters.h"

namespace org
{
const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> list {
        { "WATER", "Morning Dew" },   { "WATER", "Creek Stones" },  { "WATER", "River Bend" },    { "WATER", "Spring Melt" },
        { "WATER", "Pebble Rush" },   { "WATER", "Waterfall Mist" },{ "WATER", "Tidepool" },      { "WATER", "Shale Creek" },
        { "WATER", "Riverbed Glint" },{ "WATER", "Marsh Fog" },

        { "SOIL", "Deep Humus" },     { "SOIL", "Clay Bank" },      { "SOIL", "Peat Bog" },       { "SOIL", "Mushroom Ring" },
        { "SOIL", "Root Network" },   { "SOIL", "Badger Den" },     { "SOIL", "Stone Circle" },   { "SOIL", "Rotten Leaf" },
        { "SOIL", "Bark Beetle" },    { "SOIL", "Acorn Drop" },

        { "WOOD", "Birch Bark" },     { "WOOD", "Cedar Smoke" },    { "WOOD", "Fallen Pine" },    { "WOOD", "Hollow Log" },
        { "WOOD", "Oak Hollow" },     { "WOOD", "Amber Resin" },    { "WOOD", "Sap Flow" },       { "WOOD", "Woodpecker" },
        { "WOOD", "Sunken Branch" },  { "WOOD", "Hazel Grove" },

        { "LEAF", "Wet Moss" },       { "LEAF", "Fern Shadow" },    { "LEAF", "Rain on Leaves" }, { "LEAF", "Lichen Drift" },
        { "LEAF", "Willow Sway" },    { "LEAF", "Ivy Crawl" },      { "LEAF", "Canopy Light" },   { "LEAF", "Wild Mint" },
        { "LEAF", "Moss Carpet" },    { "LEAF", "Pollen Storm" },

        { "NIGHT", "Firefly Dusk" },  { "NIGHT", "Owl Hour" },      { "NIGHT", "Cricket Field" }, { "NIGHT", "Dragonfly" },
        { "NIGHT", "Dry Reeds" },     { "NIGHT", "Reed Flute" },    { "NIGHT", "Thistle" },       { "NIGHT", "Bramble" },
        { "NIGHT", "Elderflower" },   { "NIGHT", "Last Light" },
    };
    return list;
}

void PresetManager::set (const juce::String& id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
    else
    {
        jassertfalse; // unknown parameter id
    }
}

void PresetManager::resetToDefaults()
{
    static const juce::StringArray keep { "output", "power", "static" };
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! keep.contains (rp->paramID))
            {
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (rp->getDefaultValue());
                rp->endChangeGesture();
            }
}

juce::String PresetManager::getName() const
{
    return apvts.state.getProperty ("presetName", "Morning Dew").toString();
}

juce::String PresetManager::getCategory() const
{
    return apvts.state.getProperty ("presetCategory", u8 ("01 \xc2\xb7 WATER")).toString();
}

void PresetManager::setName (const juce::String& category, const juce::String& name)
{
    apvts.state.setProperty ("presetName", name, nullptr);
    apvts.state.setProperty ("presetCategory", category, nullptr);
}

void PresetManager::loadFactory (int index)
{
    const auto& list = factoryPresets();
    const int num = (int) list.size();
    index = ((index % num) + num) % num;
    factoryIndex = index;
    const int group = index / 10;

    juce::Random r ((juce::int64) index * 7919 + 101);
    auto uni = [&r] (float a, float b) { return a + r.nextFloat() * (b - a); };
    auto chance = [&r] (float p) { return r.nextFloat() < p; };
    auto onOff = [&chance] (float p) { return chance (p) ? 1.0f : 0.0f; };
    auto pickOf = [&r] (std::initializer_list<float> v) { return *(v.begin() + r.nextInt ((int) v.size())); };

    resetToDefaults();

    static const int barsTable[] { 0, 0, 1, 0, 1, 2, 0, 1 };
    set ("bars", (float) barsTable[index % 8]);
    set ("life", uni (35.0f, 85.0f));
    set ("mix", 100.0f);

    // ---- steps: one bar of pattern, later bars repeat it with a few mutations
    float dens[kRows] { 0.34f, 0.30f, 0.20f, 0.16f, 0.24f };
    switch (group)
    {
        case 0: dens[1] = 0.45f; dens[4] = 0.34f; break;
        case 1: dens[0] = 0.52f; break;
        case 2: dens[2] = 0.36f; break;
        case 3: dens[4] = 0.42f; dens[1] = 0.36f; break;
        default: dens[3] = 0.36f; dens[2] = 0.26f; break;
    }

    for (int row = 0; row < kRows; ++row)
    {
        set (rowOnId (row), row < 3 || chance (0.75f) ? 1.0f : 0.0f);
        set (rowAmtId (row), uni (60.0f, 100.0f));

        const float d = dens[row] * uni (0.6f, 1.4f);
        bool baseOn[16];
        float baseMix[16];
        for (int i = 0; i < 16; ++i)
        {
            baseOn[i] = r.nextFloat() < d * (i % 4 == 0 ? 1.5f : 0.8f);
            baseMix[i] = 0.3f + r.nextFloat() * 0.7f;
        }
        for (int i = 0; i < kMaxSteps; ++i)
        {
            if (i >= 16 && chance (0.22f))
                sequence.set (row, i, r.nextFloat() < d, 0.3f + r.nextFloat() * 0.7f);
            else
                sequence.set (row, i, baseOn[i % 16], baseMix[i % 16]);
        }
    }

    // ---- 1 envelope
    set ("envMode", chance (group == 1 ? 0.85f : 0.55f) ? 1.0f : 0.0f);
    set ("envA", uni (1.0f, 40.0f));
    set ("envD", uni (80.0f, 600.0f));
    set ("envS", uni (20.0f, 70.0f));
    set ("envR", uni (80.0f, 600.0f));
    set ("envCurve", uni (-30.0f, 70.0f));
    set ("envSnap", uni (25.0f, group == 1 ? 110.0f : 180.0f));
    set ("envDrift", uni (15.0f, 60.0f));

    // ---- 2 stereo
    set ("stMode", group == 0 || group == 3 ? (chance (0.5f) ? 0.0f : 1.0f) : pickOf ({ 0.0f, 1.0f, 1.0f, 2.0f }));
    set ("stLow", uni (-6.0f, 2.0f));
    set ("stMid", uni (-4.0f, 4.0f));
    set ("stHigh", uni (-2.0f, 5.0f));
    set ("stMonoHz", uni (60.0f, 250.0f));
    set ("stWidth", uni (60.0f, 180.0f));
    set ("stPan", uni (-20.0f, 20.0f));
    set ("stHaas", uni (0.0f, 14.0f));
    set ("stWander", uni (20.0f, 90.0f));

    // ---- 3 pitch
    set ("p1Semi", pickOf ({ 12.0f, 7.0f, -12.0f, 5.0f, 19.0f, 24.0f, -5.0f }));
    set ("p2On", onOff (group == 2 ? 0.7f : 0.45f));
    set ("p2Semi", pickOf ({ -12.0f, -24.0f, 7.0f, -7.0f, 12.0f }));
    set ("p3On", onOff (group == 2 ? 0.5f : 0.25f));
    set ("p3Semi", pickOf ({ 7.0f, 3.0f, 4.0f, -5.0f, 19.0f }));
    for (auto* v : { "p1", "p2", "p3" })
    {
        const juce::String p (v);
        set (p + "Fine", uni (-15.0f, 15.0f));
        set (p + "Lvl", uni (25.0f, 70.0f));
    }
    set ("piScatter", uni (4.0f, 25.0f));
    set ("piSpread", uni (20.0f, 90.0f));
    set ("piGlide", uni (0.0f, 60.0f));
    set ("piWet", uni (30.0f, 70.0f));

    // ---- 4 vocoder
    set ("voCarrier", group == 4 ? pickOf ({ 1.0f, 3.0f, 0.0f }) : (float) r.nextInt (4));
    set ("voBands", (float) (8 + r.nextInt (25)));
    set ("voLow", uni (80.0f, 400.0f));
    set ("voHigh", uni (6000.0f, 16000.0f));
    set ("voBw", uni (50.0f, 150.0f));
    set ("voFormant", uni (-5.0f, 5.0f));
    set ("voTone", (float) r.nextInt (36));
    set ("voAttack", uni (1.0f, 10.0f));
    set ("voRelease", uni (40.0f, 400.0f));
    set ("voUnvoiced", uni (0.0f, 60.0f));
    set ("voSens", uni (30.0f, 80.0f));
    set ("voDepth", uni (60.0f, 100.0f));
    set ("voEnhance", onOff (0.4f));
    set ("voMode", onOff (0.3f));
    set ("voWet", uni (35.0f, 80.0f));

    // ---- 5 bloom
    set ("blX", group == 3 ? uni (0.0f, 45.0f) : group == 0 ? uni (30.0f, 70.0f) : uni (0.0f, 100.0f));
    set ("blY", uni (30.0f, 85.0f));
    set ("blRate", uni (0.1f, 2.5f));
    set ("blFb", uni (10.0f, 70.0f));
    set ("blWind", uni (25.0f, 85.0f));
    set ("blMoss", uni (10.0f, 60.0f));
    set ("blGrain", uni (0.0f, 45.0f));
    set ("blWet", uni (35.0f, 80.0f));

    const auto& fp = list[(size_t) index];
    setName (juce::String (index + 1).paddedLeft ('0', 2) + u8 (" \xc2\xb7 ") + fp.group, fp.name);
}

juce::File PresetManager::userDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Organika")
                   .getChildFile ("Presets");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

void PresetManager::saveUser (const juce::String& name)
{
    const auto clean = name.trim();
    if (clean.isEmpty())
        return;

    setName ("YOUR PRESET", clean);
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("SEQ"), nullptr);
    state.appendChild (sequence.toValueTree(), nullptr);
    if (auto xml = state.createXml())
    {
        xml->setAttribute ("presetDisplayName", clean);
        xml->writeTo (userDirectory().getChildFile (juce::File::createLegalFileName (clean) + ".orgpreset"));
    }
    factoryIndex = -1;
}

bool PresetManager::loadUser (const juce::String& name)
{
    const auto file = userDirectory().getChildFile (juce::File::createLegalFileName (name) + ".orgpreset");
    auto xml = juce::parseXML (file);
    if (xml == nullptr)
        return false;

    static const juce::StringArray keep { "output", "power", "static" };
    const auto tree = juce::ValueTree::fromXml (*xml);
    for (auto child : tree)
    {
        if (child.hasType ("SEQ"))
        {
            sequence.fromValueTree (child);
            continue;
        }
        if (! child.hasType ("PARAM"))
            continue;
        const auto id = child.getProperty ("id").toString();
        if (keep.contains (id))
            continue;
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) child.getProperty ("value")));
            p->endChangeGesture();
        }
    }
    setName ("YOUR PRESET", xml->getStringAttribute ("presetDisplayName", name));
    factoryIndex = -1;
    return true;
}

juce::StringArray PresetManager::userPresetNames() const
{
    juce::StringArray names;
    for (auto& f : userDirectory().findChildFiles (juce::File::findFiles, false, "*.orgpreset"))
        names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}
} // namespace org

#include "PluginProcessor.h"
#include "PluginEditor.h"

OrganikaAudioProcessor::OrganikaAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ORG_STATE", org::createLayout()),
      presets (apvts, sequence)
{
    presets.loadFactory (0);
}

void OrganikaAudioProcessor::prepareToPlay (double sampleRate, int)
{
    onset.prepare (sampleRate);
    life.prepare (sampleRate);
    envelope.prepare (sampleRate);
    stereo.prepare (sampleRate);
    pitch.prepare (sampleRate);
    vocoder.prepare (sampleRate);
    bloom.prepare (sampleRate);
    for (auto& s : wetS)
    {
        s.setTime (sampleRate, 3.0);
        s.v = 0.0f;
    }
    readModuleParams();
    lastOut = juce::Decibels::decibelsToGain (param ("output"));
    lastMix = param ("mix") * 0.01f;
}

bool OrganikaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void OrganikaAudioProcessor::readModuleParams()
{
    auto B = [this] (const char* id) { return param (id) > 0.5f; };
    auto I = [this] (const char* id) { return (int) std::lround (param (id)); };

    org::EnvParams ep;
    ep.shortMode = I ("envMode") == 1;
    ep.a = param ("envA");
    ep.d = param ("envD");
    ep.s = param ("envS") * 0.01f;
    ep.r = param ("envR");
    ep.curve = param ("envCurve") * 0.01f;
    ep.snap = param ("envSnap");
    ep.drift = param ("envDrift") * 0.01f;
    envelope.setParams (ep);

    org::StereoParams sp;
    sp.mode = I ("stMode");
    sp.low = param ("stLow");
    sp.mid = param ("stMid");
    sp.high = param ("stHigh");
    sp.monoHz = param ("stMonoHz");
    sp.width = param ("stWidth") * 0.01f;
    sp.pan = param ("stPan") * 0.01f;
    sp.haasMs = param ("stHaas");
    sp.wander = param ("stWander") * 0.01f;
    stereo.setParams (sp);

    org::PitchParams pp;
    for (int v = 0; v < 3; ++v)
    {
        const juce::String p ("p" + juce::String (v + 1));
        auto& pv = pp.voices[(size_t) v];
        pv.on = param (p + "On") > 0.5f;
        pv.semi = param (p + "Semi");
        pv.fine = param (p + "Fine");
        pv.level = param (p + "Lvl") * 0.01f;
    }
    pp.scatter = param ("piScatter");
    pp.spread = param ("piSpread") * 0.01f;
    pp.glideMs = param ("piGlide");
    pp.wet = param ("piWet") * 0.01f;
    pitch.setParams (pp);

    org::VocParams vp;
    vp.carrier = I ("voCarrier");
    vp.bands = I ("voBands");
    vp.low = param ("voLow");
    vp.high = param ("voHigh");
    vp.bw = param ("voBw") * 0.01f;
    vp.formant = param ("voFormant");
    vp.toneHz = 65.406f * std::exp2 ((float) I ("voTone") / 12.0f); // C2 upwards
    vp.attackMs = param ("voAttack");
    vp.releaseMs = param ("voRelease");
    vp.gateDb = param ("voGate");
    vp.unvoiced = param ("voUnvoiced") * 0.01f;
    vp.sens = param ("voSens") * 0.01f;
    vp.depth = param ("voDepth") * 0.01f;
    vp.enhance = B ("voEnhance");
    vp.retro = I ("voMode") == 1;
    vp.chan = I ("voChan");
    vp.wet = param ("voWet") * 0.01f;
    vocoder.setParams (vp);

    org::BloomParams bp;
    bp.x = param ("blX") * 0.01f;
    bp.y = param ("blY") * 0.01f;
    bp.rate = param ("blRate");
    bp.fb = param ("blFb") * 0.01f;
    bp.wind = param ("blWind") * 0.01f;
    bp.moss = param ("blMoss") * 0.01f;
    bp.grain = param ("blGrain") * 0.01f;
    bp.wet = param ("blWet") * 0.01f;
    bloom.setParams (bp);
}

void OrganikaAudioProcessor::registerHit (const org::HitInfo& h, int row)
{
    const int i = hitCount.load (std::memory_order_relaxed) % kHitHistory;
    hits[(size_t) i].gain.store (h.gainDb, std::memory_order_relaxed);
    hits[(size_t) i].tilt.store (h.tiltDb, std::memory_order_relaxed);
    hits[(size_t) i].pan.store (h.pan, std::memory_order_relaxed);
    hits[(size_t) i].row.store (row, std::memory_order_relaxed);
    hitCount.fetch_add (1, std::memory_order_release);
}

void OrganikaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    if (buffer.getNumChannels() < 2 || n == 0)
        return;

    if (numIn == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, n);
    else if (numIn == 0)
        buffer.clear();

    // ---- transport: follow the host, or run a free clock when it is stopped
    double bpm = 120.0, ppq = internalPpq;
    bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm())
                bpm = juce::jmax (20.0, *b);
            if (pos->getIsPlaying())
                if (auto q = pos->getPpqPosition())
                {
                    ppq = *q;
                    hostPlaying = true;
                }
        }

    const double sr = getSampleRate();
    const double ppqPerSample = bpm / (60.0 * sr);
    const double stepSamples = sr * 60.0 / bpm / 4.0;

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    if (param ("power") < 0.5f)
    {
        currentStep.store (-1);
        internalPpq = ppq + ppqPerSample * n;
        return;
    }

    readModuleParams();

    const int numSteps = org::kBarCounts[juce::jlimit (0, 2, (int) std::lround (param ("bars")))] * 16;
    const float lifeAmt = param ("static") > 0.5f ? 0.0f : param ("life") * 0.01f;
    std::array<bool, org::kRows> rowOn {};
    std::array<float, org::kRows> rowAmt {};
    for (int r = 0; r < org::kRows; ++r)
    {
        rowOn[(size_t) r] = param (org::rowOnId (r)) > 0.5f;
        rowAmt[(size_t) r] = param (org::rowAmtId (r)) * 0.01f;
    }

    const float mixT = param ("mix") * 0.01f;
    const float outT = juce::Decibels::decibelsToGain (param ("output"));
    float peak = 0.0f;
    int step = 0;

    for (int i = 0; i < n; ++i)
    {
        const double q = ppq + ppqPerSample * i;
        step = (int) std::fmod (std::floor (q * 4.0), (double) numSteps);
        if (step < 0)
            step += numSteps;

        std::array<float, org::kRows> wet {};
        for (int r = 0; r < org::kRows; ++r)
        {
            const float target = rowOn[(size_t) r] && sequence.isOn (r, step) ? sequence.getMix (r, step) * rowAmt[(size_t) r] : 0.0f;
            wet[(size_t) r] = wetS[(size_t) r].next (target);
        }

        float l = L[i], rr = R[i];
        const float dl = l, dr = rr;

        if (onset.process ((l + rr) * 0.5f))
        {
            const auto h = life.onHit (rng, lifeAmt);
            envelope.onHit (rng, stepSamples);
            stereo.onHit (rng);
            pitch.onHit (rng);
            bloom.onHit (rng);

            int dominant = -1;
            float best = 0.01f;
            for (int r = 0; r < org::kRows; ++r)
                if (wet[(size_t) r] > best) { best = wet[(size_t) r]; dominant = r; }
            registerHit (h, dominant);
        }

        // the envelope runs before LIFE so its timing stays locked to the detected hit
        envelope.process (l, rr, wet[0]);
        life.process (l, rr);
        stereo.process (l, rr, wet[1]);
        pitch.process (l, rr, wet[2]);
        vocoder.process (l, rr, wet[3]);
        bloom.process (l, rr, wet[4]);

        const float t = (float) (i + 1) / (float) n;
        const float mg = lastMix + (mixT - lastMix) * t;
        const float og = lastOut + (outT - lastOut) * t;
        l = (dl + (l - dl) * mg) * og;
        rr = (dr + (rr - dr) * mg) * og;

        L[i] = l;
        R[i] = rr;
        peak = juce::jmax (peak, std::abs (l), std::abs (rr));

        if ((++scopeDecim & 3) == 0)
        {
            const int sp = scopePos.load (std::memory_order_relaxed);
            scopeL[(size_t) sp].store (l, std::memory_order_relaxed);
            scopeR[(size_t) sp].store (rr, std::memory_order_relaxed);
            scopePos.store ((sp + 1) % kScope, std::memory_order_relaxed);
        }
    }

    lastMix = mixT;
    lastOut = outT;
    internalPpq = hostPlaying ? ppq + ppqPerSample * n : internalPpq + ppqPerSample * n;
    currentStep.store (step);
    outLevel.store (juce::jmax (peak, outLevel.load() * 0.85f));
}

void OrganikaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.removeChild (state.getChildWithName ("SEQ"), nullptr);
    state.appendChild (sequence.toValueTree(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void OrganikaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            const auto seq = tree.getChildWithName ("SEQ");
            if (seq.isValid())
            {
                sequence.fromValueTree (seq);
                tree.removeChild (seq, nullptr);
            }
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* OrganikaAudioProcessor::createEditor()
{
    return new OrganikaEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OrganikaAudioProcessor();
}

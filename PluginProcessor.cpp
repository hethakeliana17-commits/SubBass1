#include "Source/PluginProcessor.h"
#include "Source/PluginEditor.h"

namespace
{
    struct Preset
    {
        const char* name;
        int osc1, osc2, octave;
        float detune, osc2Level, sub;
        float cutoff, resonance, envAmount, keyTrack;
        float fA, fD, fS, fR;
        float aA, aD, aS, aR;
        float glide;
        bool legato;
        float velocity, drive, volume;
    };

    // clang-format off
    const Preset presets[] =
    {
        //            name               o1 o2 oct  det  o2lv sub   cut   res   env   key    fA   fD    fS    fR    aA   aD    aS    aR   glide leg   vel  drv   vol
        { "Deep Sub",                     3, 2, 0,  0.0f, 0.0f, 0.5f, 180.f, 0.05f, 0.10f, 0.0f, 1.f, 120.f, 0.50f, 80.f, 3.f, 100.f, 0.90f, 120.f,  0.f, true, 0.3f, 0.10f, -3.f },
        { "Classic Saw Bass",             0, 1, 0,  7.0f, 0.5f, 0.6f, 280.f, 0.25f, 0.45f, 0.3f, 1.f, 260.f, 0.15f, 120.f, 2.f, 150.f, 0.85f, 140.f, 0.f, true, 0.5f, 0.25f, -3.f },
        { "Acid Squelch",                 0, 0, 0,  0.0f, 0.0f, 0.3f, 220.f, 0.80f, 0.85f, 0.2f, 1.f, 180.f, 0.05f, 90.f, 1.f, 120.f, 0.80f, 80.f, 70.f, false, 0.4f, 0.45f, -4.f },
        { "Reese Growl",                  0, 0, 0, 18.0f, 0.9f, 0.5f, 420.f, 0.30f, 0.35f, 0.4f, 8.f, 500.f, 0.35f, 250.f, 5.f, 200.f, 0.90f, 220.f, 25.f, true, 0.3f, 0.40f, -4.f },
        { "Pluck Bass",                   1, 0, 0,  5.0f, 0.4f, 0.4f, 160.f, 0.35f, 0.70f, 0.3f, 1.f, 140.f, 0.00f, 60.f, 1.f, 180.f, 0.00f, 90.f,  0.f, false, 0.7f, 0.20f, -3.f },
    };
    // clang-format on
    constexpr int numPresets = (int) (sizeof (presets) / sizeof (presets[0]));

    using APF = juce::AudioParameterFloat;
    using APC = juce::AudioParameterChoice;
    using APB = juce::AudioParameterBool;

    std::unique_ptr<APF> makeFloat (const char* id, const char* name, float lo, float hi, float def,
                                    const char* unit, int decimals, float centre = 0.0f)
    {
        juce::NormalisableRange<float> range (lo, hi);
        if (centre > 0.0f) range.setSkewForCentre (centre);

        const juce::String suffix = juce::String (" ") + unit;
        auto attrs = juce::AudioParameterFloatAttributes()
                         .withStringFromValueFunction ([decimals, suffix] (float v, int)
                                                       { return juce::String (v, decimals) + suffix; });

        return std::make_unique<APF> (juce::ParameterID { id, 1 }, name, range, def, attrs);
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout BassProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const juce::StringArray shapes { "Saw", "Square", "Triangle", "Sine" };

    layout.add (std::make_unique<APC> (juce::ParameterID { "osc1Shape", 1 }, "Osc 1 Shape", shapes, 0));
    layout.add (std::make_unique<APC> (juce::ParameterID { "osc2Shape", 1 }, "Osc 2 Shape", shapes, 1));
    layout.add (std::make_unique<APC> (juce::ParameterID { "osc2Octave", 1 }, "Osc 2 Octave",
                                       juce::StringArray { "-1", "0", "+1" }, 1));
    layout.add (makeFloat ("osc2Detune", "Osc 2 Detune", -50.0f, 50.0f, 7.0f, "ct", 1));
    layout.add (makeFloat ("osc2Level",  "Osc 2 Level",  0.0f, 1.0f, 0.5f, "", 2));
    layout.add (makeFloat ("subLevel",   "Sub Level",    0.0f, 1.0f, 0.7f, "", 2));

    layout.add (makeFloat ("cutoff",    "Cutoff",     20.0f, 12000.0f, 250.0f, "Hz", 0, 400.0f));
    layout.add (makeFloat ("resonance", "Resonance",  0.0f, 1.0f, 0.25f, "", 2));
    layout.add (makeFloat ("envAmount", "Env Amount", 0.0f, 1.0f, 0.5f, "", 2));
    layout.add (makeFloat ("keyTrack",  "Key Track",  0.0f, 1.0f, 0.3f, "", 2));

    layout.add (makeFloat ("fltA", "Filter Attack",  0.5f, 2000.0f, 1.0f,   "ms", 1, 50.0f));
    layout.add (makeFloat ("fltD", "Filter Decay",   1.0f, 4000.0f, 250.0f, "ms", 0, 300.0f));
    layout.add (makeFloat ("fltS", "Filter Sustain", 0.0f, 1.0f,    0.15f,  "", 2));
    layout.add (makeFloat ("fltR", "Filter Release", 1.0f, 4000.0f, 120.0f, "ms", 0, 300.0f));

    layout.add (makeFloat ("ampA", "Amp Attack",  0.5f, 2000.0f, 2.0f,   "ms", 1, 50.0f));
    layout.add (makeFloat ("ampD", "Amp Decay",   1.0f, 4000.0f, 150.0f, "ms", 0, 300.0f));
    layout.add (makeFloat ("ampS", "Amp Sustain", 0.0f, 1.0f,    0.85f,  "", 2));
    layout.add (makeFloat ("ampR", "Amp Release", 1.0f, 4000.0f, 140.0f, "ms", 0, 300.0f));

    layout.add (makeFloat ("glide", "Glide", 0.0f, 500.0f, 0.0f, "ms", 0, 100.0f));
    layout.add (std::make_unique<APB> (juce::ParameterID { "legato", 1 }, "Legato", true));
    layout.add (makeFloat ("velocity", "Velocity", 0.0f, 1.0f, 0.5f, "", 2));
    layout.add (makeFloat ("drive",    "Drive",    0.0f, 1.0f, 0.25f, "", 2));
    layout.add (makeFloat ("volume",   "Volume",  -36.0f, 6.0f, -3.0f, "dB", 1));

    return layout;
}

//==============================================================================
BassProcessor::BassProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    pOsc1Shape = raw ("osc1Shape");   pOsc2Shape = raw ("osc2Shape");   pOsc2Octave = raw ("osc2Octave");
    pOsc2Detune = raw ("osc2Detune"); pOsc2Level = raw ("osc2Level");   pSubLevel = raw ("subLevel");
    pCutoff = raw ("cutoff");         pResonance = raw ("resonance");   pEnvAmount = raw ("envAmount");
    pKeyTrack = raw ("keyTrack");
    pFltA = raw ("fltA"); pFltD = raw ("fltD"); pFltS = raw ("fltS"); pFltR = raw ("fltR");
    pAmpA = raw ("ampA"); pAmpD = raw ("ampD"); pAmpS = raw ("ampS"); pAmpR = raw ("ampR");
    pGlide = raw ("glide");           pLegato = raw ("legato");         pVelocity = raw ("velocity");
    pDrive = raw ("drive");           pVolume = raw ("volume");
}

bool BassProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void BassProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    updateEngineParams();
}

void BassProcessor::updateEngineParams()
{
    bassengine::Params p;
    p.osc1Shape  = (int) pOsc1Shape->load();
    p.osc2Shape  = (int) pOsc2Shape->load();
    p.osc2Octave = (int) pOsc2Octave->load() - 1;      // choices are -1, 0, +1
    p.osc2Detune = pOsc2Detune->load();
    p.osc2Level  = pOsc2Level->load();
    p.subLevel   = pSubLevel->load();
    p.cutoff     = pCutoff->load();
    p.resonance  = pResonance->load();
    p.envAmount  = pEnvAmount->load();
    p.keyTrack   = pKeyTrack->load();
    p.fltA = pFltA->load(); p.fltD = pFltD->load(); p.fltS = pFltS->load(); p.fltR = pFltR->load();
    p.ampA = pAmpA->load(); p.ampD = pAmpD->load(); p.ampS = pAmpS->load(); p.ampR = pAmpR->load();
    p.glideMs  = pGlide->load();
    p.legato   = pLegato->load() > 0.5f;
    p.velocity = pVelocity->load();
    p.drive    = pDrive->load();
    p.volumeDb = pVolume->load();
    engine.setParams (p);
}

void BassProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())                engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())          engine.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff())      engine.allNotesOff();
    else if (m.isAllSoundOff())      engine.allSoundOff();
    else if (m.isPitchWheel())
        engine.setPitchBendSemitones (((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f * 2.0f);
}

void BassProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    buffer.clear();
    if (numChannels == 0) return;

    updateEngineParams();

    float* left = buffer.getWritePointer (0);
    int pos = 0;

    // Render audio up to each MIDI event so notes start sample-accurately.
    for (const auto metadata : midi)
    {
        const int eventPos = juce::jlimit (pos, numSamples, metadata.samplePosition);
        for (; pos < eventPos; ++pos) left[pos] = engine.process();
        handleMidi (metadata.getMessage());
    }
    for (; pos < numSamples; ++pos) left[pos] = engine.process();

    for (int ch = 1; ch < numChannels; ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
}

//==============================================================================
int BassProcessor::getNumPrograms() { return numPresets; }

const juce::String BassProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, numPresets) ? presets[index].name : "";
}

void BassProcessor::setParameterValue (const char* id, float plainValue)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

void BassProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, numPresets)) return;
    currentProgram = index;
    const Preset& p = presets[index];

    setParameterValue ("osc1Shape", (float) p.osc1);
    setParameterValue ("osc2Shape", (float) p.osc2);
    setParameterValue ("osc2Octave", (float) (p.octave + 1));
    setParameterValue ("osc2Detune", p.detune);
    setParameterValue ("osc2Level", p.osc2Level);
    setParameterValue ("subLevel", p.sub);
    setParameterValue ("cutoff", p.cutoff);
    setParameterValue ("resonance", p.resonance);
    setParameterValue ("envAmount", p.envAmount);
    setParameterValue ("keyTrack", p.keyTrack);
    setParameterValue ("fltA", p.fA);  setParameterValue ("fltD", p.fD);
    setParameterValue ("fltS", p.fS);  setParameterValue ("fltR", p.fR);
    setParameterValue ("ampA", p.aA);  setParameterValue ("ampD", p.aD);
    setParameterValue ("ampS", p.aS);  setParameterValue ("ampR", p.aR);
    setParameterValue ("glide", p.glide);
    setParameterValue ("legato", p.legato ? 1.0f : 0.0f);
    setParameterValue ("velocity", p.velocity);
    setParameterValue ("drive", p.drive);
    setParameterValue ("volume", p.volume);
}

//==============================================================================
void BassProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BassProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* BassProcessor::createEditor() { return new BassEditor (*this); }

// This creates the plugin instance. The host calls it.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BassProcessor(); }

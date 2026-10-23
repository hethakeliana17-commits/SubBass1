#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Source/BassEngine.h"

class BassProcessor : public juce::AudioProcessor
{
public:
    BassProcessor();
    ~BassProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Sub Bass"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateEngineParams();
    void handleMidi (const juce::MidiMessage& m);
    void setParameterValue (const char* id, float plainValue);

    bassengine::Engine engine;
    int currentProgram = 0;

    // Cached pointers to the parameter values (so the audio thread never looks up strings).
    std::atomic<float> *pOsc1Shape, *pOsc2Shape, *pOsc2Octave, *pOsc2Detune, *pOsc2Level, *pSubLevel,
                       *pCutoff, *pResonance, *pEnvAmount, *pKeyTrack,
                       *pFltA, *pFltD, *pFltS, *pFltR,
                       *pAmpA, *pAmpD, *pAmpS, *pAmpR,
                       *pGlide, *pLegato, *pVelocity, *pDrive, *pVolume;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BassProcessor)
};

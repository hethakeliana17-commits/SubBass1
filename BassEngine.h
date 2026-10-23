// BassEngine.h
// A small monophonic bass synthesizer engine with NO dependency on JUCE.
// Signal path:  Osc1 + Osc2 + Sub  ->  4-pole ladder low-pass  ->  drive  ->  amp envelope  ->  volume
// Keeping the DSP separate from the plugin wrapper makes it easy to test and to reuse.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace bassengine
{
constexpr double kPi = 3.14159265358979323846;

enum Shape { Saw = 0, Square = 1, Triangle = 2, Sine = 3 };

// All values the plugin can control. The plugin wrapper fills this in once per audio block.
struct Params
{
    int   osc1Shape  = Saw;
    int   osc2Shape  = Square;
    int   osc2Octave = 0;        // -1, 0, +1
    float osc2Detune = 7.0f;     // cents
    float osc2Level  = 0.5f;     // 0..1
    float subLevel   = 0.7f;     // 0..1  (sine one octave below osc 1)

    float cutoff     = 250.0f;   // Hz
    float resonance  = 0.25f;    // 0..1
    float envAmount  = 0.5f;     // 0..1  (1.0 = filter envelope opens the filter by 5 octaves)
    float keyTrack   = 0.3f;     // 0..1

    float fltA = 1.0f,  fltD = 250.0f, fltS = 0.15f, fltR = 120.0f;   // ms, ms, level, ms
    float ampA = 2.0f,  ampD = 150.0f, ampS = 0.85f, ampR = 140.0f;

    float glideMs   = 0.0f;
    bool  legato    = true;
    float velocity  = 0.5f;      // velocity sensitivity 0..1
    float drive     = 0.25f;     // 0..1
    float volumeDb  = -3.0f;
};

// ---------------------------------------------------------------------------------------------
// PolyBLEP removes most of the aliasing from saw and square waves.
inline float polyBlep (float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

class Oscillator
{
public:
    void reset() { phase = 0.0; }

    float tick (float freq, float sampleRate, int shape)
    {
        const float dt = std::min (freq / sampleRate, 0.45f);
        const float t  = (float) phase;
        float out = 0.0f;

        switch (shape)
        {
            case Saw:
                out = 2.0f * t - 1.0f - polyBlep (t, dt);
                break;
            case Square:
            {
                out = t < 0.5f ? 1.0f : -1.0f;
                out += polyBlep (t, dt);
                float t2 = t + 0.5f;
                if (t2 >= 1.0f) t2 -= 1.0f;
                out -= polyBlep (t2, dt);
                break;
            }
            case Triangle:
                out = 4.0f * std::fabs (t - 0.5f) - 1.0f;
                break;
            default:
                out = std::sin ((float) (2.0 * kPi) * t);
                break;
        }

        phase += (double) dt;
        if (phase >= 1.0) phase -= 1.0;
        return out;
    }

private:
    double phase = 0.0;
};

// ---------------------------------------------------------------------------------------------
// 24 dB/oct low-pass "ladder" filter (zero-delay-feedback style) with a soft-clipped input.
class LadderFilter
{
public:
    void reset() { z1 = z2 = z3 = z4 = 0.0f; }

    float process (float x, float cutoffHz, float resonance, float sampleRate)
    {
        cutoffHz = std::min (std::max (cutoffHz, 20.0f), 0.45f * sampleRate);
        const float g  = std::tan ((float) kPi * cutoffHz / sampleRate);
        const float G  = g / (1.0f + g);
        const float k  = resonance * 3.95f;
        const float G2 = G * G, G3 = G2 * G, G4 = G3 * G;

        // Predict the filter output so the feedback loop can be solved without a delay.
        const float S  = (1.0f - G) * (G3 * z1 + G2 * z2 + G * z3 + z4);
        const float y4Pred = (G4 * x + S) / (1.0f + k * G4);
        const float u  = std::tanh (x - k * y4Pred);

        const float v1 = (u  - z1) * G;  const float y1 = v1 + z1;  z1 = y1 + v1;
        const float v2 = (y1 - z2) * G;  const float y2 = v2 + z2;  z2 = y2 + v2;
        const float v3 = (y2 - z3) * G;  const float y3 = v3 + z3;  z3 = y3 + v3;
        const float v4 = (y3 - z4) * G;  const float y4 = v4 + z4;  z4 = y4 + v4;

        // Partly make up for the bass lost when resonance is turned up.
        return y4 * (1.0f + 0.35f * k);
    }

private:
    float z1 = 0.0f, z2 = 0.0f, z3 = 0.0f, z4 = 0.0f;
};

// ---------------------------------------------------------------------------------------------
class Adsr
{
public:
    void setSampleRate (double sr) { sampleRate = sr; }

    void set (float attackMs, float decayMs, float sustain, float releaseMs)
    {
        attackInc = (float) (1000.0 / (std::max (attackMs, 0.5f) * sampleRate));
        decayCoef = coefFor (decayMs);
        relCoef   = coefFor (releaseMs);
        sus       = std::min (std::max (sustain, 0.0f), 1.0f);
    }

    void gate (bool on)
    {
        if (on)                      stage = Stage::Attack;
        else if (stage != Stage::Idle) stage = Stage::Release;
    }

    void kill() { stage = Stage::Idle; level = 0.0f; }
    bool isIdle() const { return stage == Stage::Idle; }
    float getLevel() const { return level; }

    float next()
    {
        switch (stage)
        {
            case Stage::Idle:
                level = 0.0f;
                break;
            case Stage::Attack:
                level += attackInc;
                if (level >= 1.0f) { level = 1.0f; stage = Stage::Decay; }
                break;
            case Stage::Decay:
                level += (sus - level) * decayCoef;
                if (std::fabs (level - sus) < 1.0e-4f) { level = sus; stage = Stage::Sustain; }
                break;
            case Stage::Sustain:
                level = sus;
                break;
            case Stage::Release:
                level -= level * relCoef;
                if (level < 1.0e-4f) { level = 0.0f; stage = Stage::Idle; }
                break;
        }
        return level;
    }

private:
    enum class Stage { Idle, Attack, Decay, Sustain, Release };

    float coefFor (float ms) const
    {
        const double seconds = std::max (ms, 1.0f) * 0.001;
        return (float) (1.0 - std::exp (-4.6 / (seconds * sampleRate)));   // reaches ~99% in 'ms'
    }

    double sampleRate = 44100.0;
    Stage  stage = Stage::Idle;
    float  level = 0.0f, attackInc = 0.01f, decayCoef = 0.001f, relCoef = 0.001f, sus = 0.8f;
};

// ---------------------------------------------------------------------------------------------
// The monophonic voice: last-note priority, optional legato, glide, pitch bend.
class Engine
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        ampEnv.setSampleRate (sr);
        fltEnv.setSampleRate (sr);
        smoothCoef = (float) (1.0 - std::exp (-1.0 / (0.005 * sr)));   // 5 ms parameter smoothing
        allSoundOff();
        setParams (params);
    }

    void setParams (const Params& p)
    {
        params = p;
        ampEnv.set (p.ampA, p.ampD, p.ampS, p.ampR);
        fltEnv.set (p.fltA, p.fltD, p.fltS, p.fltR);
        osc2Ratio = std::exp2 ((float) p.osc2Octave + p.osc2Detune / 1200.0f);
        const float seconds = p.glideMs * 0.001f;
        glideCoef = seconds <= 0.0005f ? 1.0f : (float) (1.0 - std::exp (-4.6 / (seconds * sampleRate)));
    }

    void setPitchBendSemitones (float semis) { bendSemis = semis; }

    void noteOn (int note, float vel)
    {
        const bool wasEmpty = (numHeld == 0);
        removeHeld (note);
        if (numHeld == (int) held.size())
        {
            for (int i = 1; i < numHeld; ++i) held[(size_t) i - 1] = held[(size_t) i];
            --numHeld;
        }
        held[(size_t) numHeld++] = note;

        targetSemis = (float) note;
        velocity = vel;
        if (! haveNote) { currentSemis = targetSemis; haveNote = true; }

        if (wasEmpty || ! params.legato)
        {
            if (ampEnv.isIdle())
            {
                osc1.reset(); osc2.reset(); sub.reset();
                filter.reset();
            }
            ampEnv.gate (true);
            fltEnv.gate (true);
        }
    }

    void noteOff (int note)
    {
        if (numHeld == 0) return;
        removeHeld (note);
        if (numHeld == 0)
        {
            ampEnv.gate (false);
            fltEnv.gate (false);
        }
        else
        {
            targetSemis = (float) held[(size_t) numHeld - 1];   // fall back to the previous held key
        }
    }

    void allNotesOff()
    {
        numHeld = 0;
        ampEnv.gate (false);
        fltEnv.gate (false);
    }

    void allSoundOff()
    {
        numHeld = 0;
        ampEnv.kill();
        fltEnv.kill();
        filter.reset();
    }

    // One mono sample.
    float process()
    {
        smoothedCutoff += (params.cutoff - smoothedCutoff) * smoothCoef;
        const float targetGain = std::pow (10.0f, params.volumeDb / 20.0f);
        smoothedGain += (targetGain - smoothedGain) * smoothCoef;

        const float amp = ampEnv.next();
        const float fe  = fltEnv.next();
        if (ampEnv.isIdle())
        {
            filter.reset();
            return 0.0f;
        }

        currentSemis += (targetSemis - currentSemis) * glideCoef;
        const float f1 = 440.0f * std::exp2 ((currentSemis + bendSemis - 69.0f) / 12.0f);

        const float o1 = osc1.tick (f1, (float) sampleRate, params.osc1Shape);
        const float o2 = osc2.tick (f1 * osc2Ratio, (float) sampleRate, params.osc2Shape);
        const float s  = sub.tick (f1 * 0.5f, (float) sampleRate, Sine);
        const float mix = (o1 + o2 * params.osc2Level + s * params.subLevel) * 0.5f;

        float cutoff = smoothedCutoff * std::exp2 (params.envAmount * 5.0f * fe);
        if (params.keyTrack > 0.0f)
            cutoff *= std::exp2 (params.keyTrack * std::log2 (std::max (f1, 1.0f) / 55.0f));

        float y = filter.process (mix, cutoff, params.resonance, (float) sampleRate);

        const float gain = 1.0f + params.drive * 8.0f;
        y = std::tanh (y * gain) / std::tanh (gain);

        const float velGain = (1.0f - params.velocity) + params.velocity * velocity;
        return y * amp * velGain * smoothedGain;
    }

private:
    void removeHeld (int note)
    {
        int w = 0;
        for (int i = 0; i < numHeld; ++i)
            if (held[(size_t) i] != note) held[(size_t) w++] = held[(size_t) i];
        numHeld = w;
    }

    Params params;
    double sampleRate = 44100.0;

    Oscillator osc1, osc2, sub;
    LadderFilter filter;
    Adsr ampEnv, fltEnv;

    std::array<int, 32> held {};
    int   numHeld = 0;
    bool  haveNote = false;
    float targetSemis = 33.0f, currentSemis = 33.0f, bendSemis = 0.0f;
    float velocity = 1.0f, osc2Ratio = 1.0f, glideCoef = 1.0f;
    float smoothCoef = 0.01f, smoothedCutoff = 250.0f, smoothedGain = 0.5f;
};

} // namespace bassengine

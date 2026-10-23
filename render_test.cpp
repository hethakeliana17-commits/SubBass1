// Standalone test for the bass engine (no JUCE needed).
//   g++ -O2 -std=c++17 -I Source tests/render_test.cpp -o render_test && ./render_test
// Renders a short bass line to test_bassline.wav and checks pitch, level and stability.
#include "BassEngine.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace bassengine;

static void writeWav (const char* path, const std::vector<float>& x, int sr)
{
    FILE* f = std::fopen (path, "wb");
    if (! f) return;
    const uint32_t dataBytes = (uint32_t) (x.size() * 2);
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (1); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 2); w16 (2); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (float s : x)
    {
        const float c = std::fmax (-1.0f, std::fmin (1.0f, s));
        const int16_t v = (int16_t) std::lrintf (c * 32767.0f);
        std::fwrite (&v, 2, 1, f);
    }
    std::fclose (f);
}

// Estimate the fundamental by counting positive-going zero crossings of the low-passed signal.
static float estimateFreq (const std::vector<float>& x, size_t a, size_t b, int sr)
{
    int crossings = 0;
    for (size_t i = a + 1; i < b; ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f) ++crossings;
    return (float) crossings * (float) sr / (float) (b - a);
}

int main()
{
    const int sr = 44100;
    int failures = 0;
    auto check = [&] (bool ok, const char* msg) { std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", msg); if (! ok) ++failures; };

    // 1) Pitch accuracy for each oscillator shape: play A1 (55 Hz), filter wide open.
    for (int shape = 0; shape < 4; ++shape)
    {
        Engine e; Params p;
        p.osc1Shape = shape; p.osc2Level = 0.0f; p.subLevel = 0.0f; p.cutoff = 12000.0f;
        p.envAmount = 0.0f; p.keyTrack = 0.0f; p.resonance = 0.0f; p.drive = 0.0f;
        p.ampA = 1.0f; p.ampS = 1.0f; p.volumeDb = 0.0f;
        e.prepare (sr); e.setParams (p);
        e.noteOn (33, 1.0f);                       // MIDI 33 = A1 = 55 Hz
        std::vector<float> out ((size_t) sr * 2);
        for (auto& s : out) s = e.process();
        const float f = estimateFreq (out, (size_t) sr / 2, out.size(), sr);
        char msg[96]; std::snprintf (msg, sizeof msg, "shape %d plays 55 Hz (measured %.2f Hz)", shape, f);
        check (std::fabs (f - 55.0f) < 1.0f, msg);
    }

    // 2) A bass line with the default settings: no NaN/inf, no clipping, silence after release.
    {
        Engine e; Params p; e.prepare (sr); e.setParams (p);
        std::vector<float> out;
        const int notes[] = { 33, 33, 36, 38, 40, 38, 36, 31 };   // A1 A1 C2 D2 E2 D2 C2 G1
        for (int n : notes)
        {
            e.noteOn (n, 0.9f);
            for (int i = 0; i < sr / 4; ++i) out.push_back (e.process());
            e.noteOff (n);
            for (int i = 0; i < sr / 8; ++i) out.push_back (e.process());
        }
        for (int i = 0; i < sr; ++i) out.push_back (e.process());

        bool finite = true; float peak = 0.0f, tailPeak = 0.0f;
        for (size_t i = 0; i < out.size(); ++i)
        {
            if (! std::isfinite (out[i])) finite = false;
            peak = std::fmax (peak, std::fabs (out[i]));
        }
        for (size_t i = out.size() - sr / 2; i < out.size(); ++i) tailPeak = std::fmax (tailPeak, std::fabs (out[i]));
        char msg[96];
        std::snprintf (msg, sizeof msg, "bass line peak %.3f (must be < 1.0)", peak);
        check (finite, "output is finite (no NaN/inf)");
        check (peak < 1.0f && peak > 0.05f, msg);
        check (tailPeak < 1.0e-3f, "silent after the last note releases");
        writeWav ("test_bassline.wav", out, sr);
    }

    // 3) Extreme settings must stay stable: max resonance, max drive, max env amount.
    {
        Engine e; Params p;
        p.resonance = 1.0f; p.drive = 1.0f; p.envAmount = 1.0f; p.cutoff = 12000.0f; p.volumeDb = 6.0f;
        e.prepare (sr); e.setParams (p);
        bool finite = true; float peak = 0.0f;
        for (int n = 24; n < 72; n += 5)
        {
            e.noteOn (n, 1.0f);
            for (int i = 0; i < sr / 5; ++i) { const float s = e.process(); finite = finite && std::isfinite (s); peak = std::fmax (peak, std::fabs (s)); }
            e.noteOff (n);
        }
        char msg[96]; std::snprintf (msg, sizeof msg, "extreme settings stay finite and bounded (peak %.2f)", peak);
        check (finite && peak < 4.0f, msg);
    }

    // 4) Glide: with 200 ms glide the pitch should be between the two notes shortly after the jump.
    {
        Engine e; Params p;
        p.osc1Shape = Sine; p.osc2Level = 0.0f; p.subLevel = 0.0f; p.cutoff = 12000.0f; p.envAmount = 0.0f;
        p.keyTrack = 0.0f; p.resonance = 0.0f; p.drive = 0.0f; p.glideMs = 200.0f; p.ampS = 1.0f; p.volumeDb = 0.0f;
        e.prepare (sr); e.setParams (p);
        e.noteOn (33, 1.0f);
        for (int i = 0; i < sr / 2; ++i) e.process();
        e.noteOn (45, 1.0f);                      // legato jump up one octave
        std::vector<float> out ((size_t) sr / 10);
        for (auto& s : out) s = e.process();
        const float f = estimateFreq (out, 0, out.size(), sr);
        char msg[96]; std::snprintf (msg, sizeof msg, "glide: 100 ms after jump pitch is %.0f Hz (between 55 and 110)", f);
        check (f > 58.0f && f < 108.0f, msg);
    }

    std::printf (failures == 0 ? "\nAll tests passed.\n" : "\n%d test(s) FAILED.\n", failures);
    return failures == 0 ? 0 : 1;
}

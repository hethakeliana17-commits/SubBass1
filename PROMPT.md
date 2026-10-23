# Prompts for building and extending this plugin with an AI assistant

Use these with Claude (or any capable coding assistant). Paste the relevant files along with the prompt so the
assistant sees your real code.

---

## 1. Prompt to create this plugin from scratch

> You are an expert audio plugin developer. Create a complete, ready-to-build **JUCE 8 + CMake** project for a
> **monophonic synth bass VST3 plugin** called "Sub Bass", and a **GitHub Actions workflow** that builds it for
> Windows, macOS and Linux so I never have to install a compiler. I am not a programmer, so every file must be
> complete (no "..." placeholders) and compile on the first try.
>
> **Sound design**
> - Two oscillators (saw, square, triangle, sine) with PolyBLEP anti-aliasing; Osc 2 has octave (-1/0/+1) and
>   detune in cents and a level knob.
> - A sine sub-oscillator one octave below Osc 1, with a level knob.
> - 24 dB/oct resonant ladder low-pass filter with cutoff, resonance, envelope amount (up to 5 octaves) and key tracking.
> - Two ADSR envelopes: filter and amplitude.
> - Soft-saturation drive after the filter, and a volume control in dB.
> - Monophonic, last-note priority, legato option, glide (portamento) in milliseconds, velocity sensitivity,
>   pitch bend of ±2 semitones, all-notes-off handling.
> - Five factory presets (Deep Sub, Classic Saw Bass, Acid Squelch, Reese Growl, Pluck Bass), exposed as host programs
>   and in a drop-down in the plugin window.
>
> **Code requirements**
> - Put the DSP in a header (`BassEngine.h`) with **no JUCE dependency**, so it can be unit-tested.
> - Use `AudioProcessorValueTreeState` for all parameters so the host can automate and save them.
> - Audio thread rules: no memory allocation, no locks, no string lookups inside `processBlock`.
> - Handle MIDI sample-accurately by rendering audio up to each MIDI event.
> - A custom editor (dark theme, orange accent) with rotary knobs, grouped in labelled sections, with readable value text
>   (e.g. "250 Hz", "12 ms").
> - Parameters must have sensible ranges and skew (cutoff and times are logarithmic-feeling).
>
> **Project files to output** (each in its own code block, with its path as a heading)
> `CMakeLists.txt` (fetch JUCE 8.0.12 with FetchContent; formats VST3 + Standalone, plus AU on macOS; universal
> binaries on macOS), `Source/BassEngine.h`, `Source/PluginProcessor.h/.cpp`, `Source/PluginEditor.h/.cpp`,
> `tests/render_test.cpp` (JUCE-free test of pitch accuracy, no NaN, level, glide), `.github/workflows/build.yml`
> (run the engine test first, then build all three platforms, zip the `build/SubBass_artefacts/Release` folder,
> upload as artifacts, and attach to a GitHub Release when a `v*` tag is pushed), `.gitignore`, and `README.md`
> with click-by-click instructions for a beginner to upload to GitHub, download the plugin, and install it in a DAW.
>
> Finish by listing anything you could not verify, and what error output I should paste back to you if the build fails.

---

## 2. Prompt to fix a failed build

> My GitHub Actions build of the attached JUCE plugin failed. Here is the complete error log from the red step:
>
> ```
> (paste the log here)
> ```
>
> Explain the cause in one or two sentences, then give me the **complete corrected file(s)**, not just snippets, and
> tell me exactly which files in my repository to replace.

## 3. Prompt to change the sound or add features

Start with this, then add one request from the list below:

> Here is my working JUCE synth plugin (files attached). Make the change below **without breaking anything else**.
> Keep the DSP in `BassEngine.h` free of JUCE, keep the audio thread free of allocations and locks, add any new
> parameters with unique IDs in `createLayout()` and in the editor, and give me complete updated files plus an
> updated `render_test.cpp` test for the new feature.

Ideas to ask for:

- "Add a second filter mode switch: low-pass, band-pass and high-pass."
- "Add an LFO (sine/triangle/square, rate 0.1 to 20 Hz, tempo-sync option) that can modulate cutoff, pitch or amplitude."
- "Add a wavetable oscillator with 8 built-in bass wavetables and a position knob."
- "Add a unison/Reese mode with 3 to 7 detuned saws and a stereo spread control."
- "Make it 8-voice polyphonic with a mono/poly switch."
- "Add a built-in tempo-synced chorus and a compressor on the output."
- "Add an oscilloscope and a filter-response display to the editor."
- "Add preset saving and loading to files, and a preset name display."
- "Add MIDI CC mapping so the mod wheel controls cutoff, and MIDI learn for any knob."
- "Add oversampling (2x or 4x) around the drive stage to cut aliasing."

## 4. Prompt to use this project as a template for a different instrument

> Using this project as a template (attached), turn it into a **[describe: lead synth / pad / drum machine / FM synth /
> sampler]** with these features: **[list them]**. Keep the same folder layout, CMake setup and GitHub workflow, change
> the plugin name and codes in `CMakeLists.txt` (give it a new `PLUGIN_CODE`), and give me every changed file in full.

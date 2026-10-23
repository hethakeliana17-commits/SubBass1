# Sub Bass: a synth bass VST3 plugin

A monophonic bass synthesizer plugin (VST3, plus Audio Unit on Mac, plus a standalone app) built with
[JUCE](https://juce.com). GitHub builds the plugin for you, so you do not need to install a compiler.

## What it does

| Section | Controls |
|---|---|
| Oscillators | Osc 1 and Osc 2 shape (saw, square, triangle, sine), Osc 2 octave and detune, Osc 2 level, sub oscillator level |
| Filter | 24 dB/oct ladder low-pass: cutoff, resonance, envelope amount, key tracking |
| Envelopes | Separate ADSR for the filter and the amplitude |
| Playing | Glide (portamento), legato on/off, velocity sensitivity, pitch bend (±2 semitones) |
| Output | Drive (soft saturation), volume |
| Presets | Deep Sub, Classic Saw Bass, Acid Squelch, Reese Growl, Pluck Bass |

It is monophonic: one note at a time, last note wins, which is what you want for bass lines.

## Get the plugin (no coding needed)

1. **Make a free account** at <https://github.com>.
2. **Create a repository:** click **+** (top right) then **New repository**. Name it `SubBass`. Choose **Public**
   (public repositories get free build time; private ones use a monthly allowance, and Mac builds use it up faster).
3. **Upload the files:** unzip `SubBass.zip`. On your new repository page click **uploading an existing file**,
   open the unzipped `SubBass` folder and drag **everything inside it** onto the page, then click **Commit changes**.
   - The folder `.github` is hidden on some computers (Mac: press `Cmd+Shift+.` in Finder to show it). If it didn't
     upload, click **Add file → Create new file**, type `.github/workflows/build.yml` as the name (typing the slashes
     creates the folders), paste in the contents of that file, and commit.
4. **Wait for the build:** click the **Actions** tab. A run called *Build Sub Bass* starts by itself. It takes about
   10 to 20 minutes. A green tick means success.
5. **Download:** click the finished run, scroll to **Artifacts** at the bottom, and download `SubBass-Windows`
   (or `SubBass-macOS` / `SubBass-Linux`).
6. **Unzip twice:** the download is a zip that contains another zip. Inside you will find `VST3/Sub Bass.vst3`
   and a `Standalone` folder.

### Install it

**Windows:** copy the `Sub Bass.vst3` folder into `C:\Program Files\Common Files\VST3\`, then rescan plugins in your DAW.
In FL Studio: *Options → Manage plugins → Find installed plugins*. If Windows SmartScreen warns about the standalone
app, choose *More info → Run anyway* (the files are not code-signed).

**Mac:** copy `Sub Bass.vst3` to `~/Library/Audio/Plug-Ins/VST3/` (and `Sub Bass.component` to
`~/Library/Audio/Plug-Ins/Components/` for Logic and GarageBand). Because the files are not signed or notarized, macOS
may block them. Run this once in Terminal, then rescan in your DAW:

```
xattr -cr "$HOME/Library/Audio/Plug-Ins/VST3/Sub Bass.vst3"
codesign --force --deep -s - "$HOME/Library/Audio/Plug-Ins/VST3/Sub Bass.vst3"
```

**Linux:** copy `Sub Bass.vst3` to `~/.vst3/`.

The **Standalone** app lets you try the sound without a DAW (it can play from your computer keyboard or a MIDI keyboard).

## If the build fails

Click the failed job on the Actions page, open the red step, and copy the error text. Paste it into Claude together
with the prompt in `PROMPT.md` and ask for a fix. Replace the changed files in your repository and the build reruns.

## Make a downloadable release

On your computer (with git installed): `git tag v1.0.0 && git push origin v1.0.0`. GitHub builds the plugin and attaches
the zip files to the **Releases** page, where anyone can download them.

## Make it your own

- **Names:** in `CMakeLists.txt` change `COMPANY_NAME`, `PRODUCT_NAME`, and `PLUGIN_MANUFACTURER_CODE` (4 characters,
  at least one uppercase letter). Give each new plugin you make its own `PLUGIN_CODE`.
- **Sound design:** the sound engine is in `Source/BassEngine.h`; presets are the table at the top of
  `Source/PluginProcessor.cpp`; the look is in `Source/PluginEditor.cpp`.
- **Test the engine without a DAW:** `g++ -O2 -std=c++17 -I Source tests/render_test.cpp -o render_test && ./render_test`
  checks pitch accuracy, level, stability and glide, and writes `test_bassline.wav` so you can listen.

## Project layout

```
CMakeLists.txt                 build recipe (downloads JUCE automatically)
Source/BassEngine.h            the synth sound engine (no JUCE needed)
Source/PluginProcessor.*       parameters, MIDI handling, presets, host connection
Source/PluginEditor.*          the on-screen controls
tests/render_test.cpp          automatic checks for the sound engine
.github/workflows/build.yml    GitHub builds Windows, macOS and Linux for you
PROMPT.md                      prompts to rebuild or extend this plugin with an AI assistant
```

## License note

JUCE is dual-licensed: free under AGPLv3 (you must share your plugin's source if you distribute it) or under a paid
commercial license if you want to sell it without sharing the source. Check <https://juce.com/juce-8-licence/> before
selling a plugin built with it. VST is a trademark of Steinberg; the VST3 SDK is bundled with JUCE under its own license.

## Known limits

- The code compiles cleanly against JUCE 8 and the sound engine passes its tests, but the finished plugin binaries are
  produced by GitHub's build servers, so the first load in your DAW is the real test. If something misbehaves,
  describe what you see and use the prompts in `PROMPT.md`.
- Not code-signed or notarized, so Windows and macOS show security warnings.

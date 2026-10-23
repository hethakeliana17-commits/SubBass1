#include "Source/PluginEditor.h"

namespace
{
    const juce::Colour kBackground (0xff14161b);
    const juce::Colour kPanel      (0xff1e2128);
    const juce::Colour kAccent     (0xffff8a1f);
    const juce::Colour kText       (0xffe6e8ee);
    const juce::Colour kDimText    (0xff9aa0ad);
}

//==============================================================================
ParamControl::ParamControl (juce::AudioProcessorValueTreeState& state, const juce::String& paramId,
                            const juce::String& text, Kind controlKind)
    : kind (controlKind)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (12.0f)));
    label.setColour (juce::Label::textColourId, kDimText);
    addAndMakeVisible (label);

    switch (kind)
    {
        case Kind::Knob:
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 66, 16);
            slider.setColour (juce::Slider::rotarySliderFillColourId, kAccent);
            slider.setColour (juce::Slider::thumbColourId, kText);
            slider.setColour (juce::Slider::textBoxTextColourId, kText);
            slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            addAndMakeVisible (slider);
            sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
            break;

        case Kind::Choice:
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramId)))
                combo.addItemList (p->choices, 1);
            addAndMakeVisible (combo);
            comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, paramId, combo);
            break;

        case Kind::Toggle:
            toggle.setButtonText ("On");
            toggle.setColour (juce::ToggleButton::textColourId, kText);
            toggle.setColour (juce::ToggleButton::tickColourId, kAccent);
            addAndMakeVisible (toggle);
            buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramId, toggle);
            break;
    }
}

void ParamControl::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));

    switch (kind)
    {
        case Kind::Knob:   slider.setBounds (r); break;
        case Kind::Choice: combo.setBounds (r.withSizeKeepingCentre (r.getWidth() - 6, 24)); break;
        case Kind::Toggle: toggle.setBounds (r.withSizeKeepingCentre (r.getWidth() - 6, 24)); break;
    }
}

//==============================================================================
BassEditor::BassEditor (BassProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p)
{
    using K = ParamControl::Kind;

    auto& osc = addSection ("OSCILLATORS");
    addControl (osc, "osc1Shape",  "Osc 1",     K::Choice);
    addControl (osc, "osc2Shape",  "Osc 2",     K::Choice);
    addControl (osc, "osc2Octave", "Osc 2 Oct", K::Choice);
    addControl (osc, "osc2Detune", "Detune",    K::Knob);
    addControl (osc, "osc2Level",  "Osc 2 Lvl", K::Knob);
    addControl (osc, "subLevel",   "Sub",       K::Knob);

    auto& flt = addSection ("FILTER");
    addControl (flt, "cutoff",    "Cutoff",    K::Knob);
    addControl (flt, "resonance", "Resonance", K::Knob);
    addControl (flt, "envAmount", "Env Amt",   K::Knob);
    addControl (flt, "keyTrack",  "Key Track", K::Knob);

    auto& fenv = addSection ("FILTER ENVELOPE");
    addControl (fenv, "fltA", "Attack",  K::Knob);
    addControl (fenv, "fltD", "Decay",   K::Knob);
    addControl (fenv, "fltS", "Sustain", K::Knob);
    addControl (fenv, "fltR", "Release", K::Knob);

    auto& aenv = addSection ("AMP ENVELOPE");
    addControl (aenv, "ampA", "Attack",  K::Knob);
    addControl (aenv, "ampD", "Decay",   K::Knob);
    addControl (aenv, "ampS", "Sustain", K::Knob);
    addControl (aenv, "ampR", "Release", K::Knob);

    auto& perf = addSection ("PLAYING");
    addControl (perf, "glide",    "Glide",    K::Knob);
    addControl (perf, "legato",   "Legato",   K::Toggle);
    addControl (perf, "velocity", "Velocity", K::Knob);

    auto& out = addSection ("OUTPUT");
    addControl (out, "drive",  "Drive",  K::Knob);
    addControl (out, "volume", "Volume", K::Knob);

    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem (processor.getProgramName (i), i + 1);
    presetBox.setSelectedItemIndex (juce::jmax (0, processor.getCurrentProgram()), juce::dontSendNotification);
    presetBox.onChange = [this] { processor.setCurrentProgram (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);

    setSize (800, 480);
}

BassEditor::Section& BassEditor::addSection (const juce::String& title)
{
    sections.push_back (std::make_unique<Section>());
    sections.back()->title = title;
    return *sections.back();
}

void BassEditor::addControl (Section& s, const char* id, const char* text, ParamControl::Kind kind)
{
    auto c = std::make_unique<ParamControl> (processor.apvts, id, text, kind);
    addAndMakeVisible (*c);
    s.controls.push_back (std::move (c));
}

void BassEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBackground);

    g.setColour (kAccent);
    g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold)));
    g.drawText ("SUB BASS", 20, 12, 250, 34, juce::Justification::centredLeft);

    for (auto& s : sections)
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (s->area.toFloat(), 8.0f);
        g.setColour (kDimText);
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText (s->title, s->area.getX() + 12, s->area.getY() + 6, s->area.getWidth() - 24, 16,
                    juce::Justification::centredLeft);
    }
}

void BassEditor::resized()
{
    constexpr int colW = 70, pad = 10, gap = 8, rowH = 130, titleH = 24;

    auto bounds = getLocalBounds().reduced (10);
    auto header = bounds.removeFromTop (44);
    presetBox.setBounds (header.removeFromRight (200).reduced (0, 8));

    int x = bounds.getX();
    int y = bounds.getY();

    for (auto& s : sections)
    {
        const int w = (int) s->controls.size() * colW + pad * 2;
        if (x + w > bounds.getRight() + 1)
        {
            x = bounds.getX();
            y += rowH + gap;
        }

        s->area = { x, y, w, rowH };

        auto inner = s->area.reduced (pad, 0);
        inner.removeFromTop (titleH);
        inner.removeFromBottom (6);
        for (auto& c : s->controls)
            c->setBounds (inner.removeFromLeft (colW));

        x += w + gap;
    }
}

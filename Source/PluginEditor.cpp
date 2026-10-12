#include "PluginEditor.h"

NAMAudioProcessorEditor::NAMAudioProcessorEditor(NAMAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(720, 440);
    auto setupLabel = [this](juce::Label& l, const juce::String& text, float size, bool bold = false)
    {
        l.setText(text, juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, juce::Colour(0xffe8edf0));
        l.setFont(juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain)));
        addAndMakeVisible(l);
    };
    setupLabel(title, "NAM AUDIO", 26.0f, true);
    setupLabel(subtitle, "NEURAL AMP MODELER  /  VST3", 11.0f);
    setupLabel(modelStatus, "No model loaded", 12.0f);
    setupLabel(inputReadout, "INPUT  -inf dBFS", 12.0f, true);
    setupLabel(outputReadout, "OUTPUT  -inf dBFS", 12.0f, true);
    setupLabel(footer, "READY  •  PEAK METERING", 11.0f);

    loadButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xffb6f36b));
    loadButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff10140c));
    loadButton.onClick = [this] { chooseModel(); };
    addAndMakeVisible(loadButton);

    configureSlider(inputGain, inputGainLabel, "INPUT");
    configureSlider(outputGain, outputGainLabel, "OUTPUT");
    configureSlider(mix, mixLabel, "MIX");
    inputAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "inputGain", inputGain);
    outputAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "outputGain", outputGain);
    mixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.parameters, "mix", mix);
    bypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.parameters, "bypass", bypass);
    addAndMakeVisible(bypass);
    addAndMakeVisible(resetClipButton);
    resetClipButton.onClick = [this] { processor.clipping.store(false); };
    startTimerHz(30);
}

NAMAudioProcessorEditor::~NAMAudioProcessorEditor() { stopTimer(); }

void NAMAudioProcessorEditor::configureSlider(juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 82, 22);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffb6f36b));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffb6f36b));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe8edf0));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff20272b));
    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour(0xffa8b3ba));
    addAndMakeVisible(label);
    addAndMakeVisible(slider);
}

void NAMAudioProcessorEditor::chooseModel()
{
    auto chooser = std::make_shared<juce::FileChooser>("Select a NAM model", juce::File{}, "*.nam");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, chooser](const juce::FileChooser& fc)
        {
            const auto file = fc.getResult();
            if (file.existsAsFile())
                processor.loadModelAsync(file);
        });
}

void NAMAudioProcessorEditor::timerCallback()
{
    inputLevel = processor.inputPeak.load();
    outputLevel = processor.outputPeak.load();
    auto db = [](float x) { return x > 0.000001f ? juce::Decibels::gainToDecibels(x) : -100.0f; };
    inputReadout.setText("INPUT   " + juce::String(db(inputLevel), 1) + " dBFS", juce::dontSendNotification);
    outputReadout.setText("OUTPUT   " + juce::String(db(outputLevel), 1) + " dBFS", juce::dontSendNotification);
    modelStatus.setText(processor.getModelStatus(), juce::dontSendNotification);
    footer.setText(processor.clipping.load() ? "CLIP DETECTED  •  RESET CLIP" :
                   (processor.modelReady.load() ? "MODEL ACTIVE  •  PEAK METERING" : "READY  •  PEAK METERING"),
                   juce::dontSendNotification);
    repaint();
}

void NAMAudioProcessorEditor::paint(juce::Graphics& g)
{
    const juce::Colour bg(0xff111518), panel(0xff1b2226), line(0xff303a40), green(0xffb6f36b);
    g.fillAll(bg);
    g.setColour(panel);
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(14.0f), 18.0f);
    g.setColour(line);
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(14.0f), 18.0f, 1.0f);
    g.setColour(green);
    g.fillRoundedRectangle(30.0f, 83.0f, 660.0f, 2.0f, 1.0f);

    auto meter = [&](float x, float y, float w, float level)
    {
        g.setColour(juce::Colour(0xff0b0e10));
        g.fillRoundedRectangle(x, y, w, 12.0f, 6.0f);
        const float norm = juce::jlimit(0.0f, 1.0f, level);
        g.setColour(level >= 0.999f ? juce::Colour(0xffff5d62) :
                    level >= 0.707f ? juce::Colour(0xffffc857) : green);
        g.fillRoundedRectangle(x, y, w * norm, 12.0f, 6.0f);
    };
    meter(38.0f, 288.0f, 300.0f, inputLevel);
    meter(382.0f, 288.0f, 300.0f, outputLevel);
    g.setColour(juce::Colour(0xff7e8a91));
    g.setFont(10.0f);
    g.drawText("-inf", 38, 302, 30, 14, juce::Justification::left);
    g.drawText("0 dBFS", 300, 302, 38, 14, juce::Justification::right);
    g.drawText("-inf", 382, 302, 30, 14, juce::Justification::left);
    g.drawText("0 dBFS", 644, 302, 38, 14, juce::Justification::right);
}

void NAMAudioProcessorEditor::resized()
{
    title.setBounds(32, 25, 360, 36);
    subtitle.setBounds(34, 59, 360, 18);
    loadButton.setBounds(32, 99, 125, 34);
    modelStatus.setBounds(171, 99, 510, 34);
    inputGainLabel.setBounds(46, 145, 150, 20);
    inputGain.setBounds(38, 163, 165, 110);
    outputGainLabel.setBounds(209, 145, 150, 20);
    outputGain.setBounds(201, 163, 165, 110);
    mixLabel.setBounds(372, 145, 150, 20);
    mix.setBounds(364, 163, 165, 110);
    bypass.setBounds(552, 174, 100, 28);
    resetClipButton.setBounds(552, 210, 115, 28);
    inputReadout.setBounds(38, 258, 300, 22);
    outputReadout.setBounds(382, 258, 300, 22);
    footer.setBounds(36, 350, 640, 30);
}

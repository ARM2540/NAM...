#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class NAMAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit NAMAudioProcessorEditor(NAMAudioProcessor&);
    ~NAMAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void chooseModel();
    void configureSlider(juce::Slider&, juce::Label&, const juce::String&);
    NAMAudioProcessor& processor;
    juce::Label title, subtitle, modelStatus, inputReadout, outputReadout, footer;
    juce::TextButton loadButton { "Choose .nam" }, resetClipButton { "Reset clip" };
    juce::Slider inputGain, outputGain, mix;
    juce::Label inputGainLabel, outputGainLabel, mixLabel;
    juce::ToggleButton bypass { "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttach, outputAttach, mixAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach;
    float inputLevel = 0.0f, outputLevel = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NAMAudioProcessorEditor)
};

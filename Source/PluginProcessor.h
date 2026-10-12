#pragma once
#include <JuceHeader.h>
#include <dsp.h>
#include <get_dsp.h>
#include <atomic>
#include <memory>
#include <mutex>

class NAMAudioProcessor final : public juce::AudioProcessor
{
public:
    NAMAudioProcessor();
    ~NAMAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "NAM Audio Processor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };
    std::atomic<bool> clipping { false };
    std::atomic<bool> modelReady { false };
    std::atomic<bool> modelLoadFailed { false };

    juce::String getModelPath() const;
    void loadModelAsync(const juce::File& file);
    juce::String getModelStatus() const;

private:
    using ModelPtr = std::shared_ptr<nam::DSP>;
    juce::String modelPath;
    juce::String modelStatus { "No model loaded" };
    mutable juce::CriticalSection stateLock;
    juce::ThreadPool loaderPool { 1 };

    // Model pointer handoff is protected. The lock is only taken briefly to copy
    // a shared_ptr; NAM inference itself runs without holding this lock.
    ModelPtr activeModel;
    std::mutex modelMutex;
    double currentSampleRate = 48000.0;
    int currentBlockSize = 512;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    void setStatus(const juce::String& status, bool failed = false);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NAMAudioProcessor)
};

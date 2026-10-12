#include "PluginProcessor.h"
#include "PluginEditor.h"

NAMAudioProcessor::NAMAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::mono(), true)
        .withOutput("Output", juce::AudioChannelSet::mono(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameters())
{
}

NAMAudioProcessor::~NAMAudioProcessor()
{
    loaderPool.removeAllJobs(true, 5000);
}

juce::AudioProcessorValueTreeState::ParameterLayout NAMAudioProcessor::createParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "inputGain", 1 }, "Input Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "outputGain", 1 }, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "mix", 1 }, "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 100.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "bypass", 1 }, "Bypass", false));
    return layout;
}

void NAMAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize = juce::jmax(1, samplesPerBlock);

    ModelPtr model;
    {
        std::lock_guard<std::mutex> lock(modelMutex);
        model = activeModel;
    }
    if (model)
    {
        try
        {
            model->Reset(sampleRate, currentBlockSize);
            modelReady.store(true);
        }
        catch (const std::exception& e)
        {
            setStatus("Reset failed: " + juce::String(e.what()), true);
        }
    }
}

void NAMAudioProcessor::releaseResources() {}

bool NAMAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Current implementation deliberately supports mono input/output only.
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void NAMAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() == 0 || n == 0)
        return;

    const float inputPeakNow = buffer.getMagnitude(0, 0, n);
    inputPeak.store(inputPeakNow);

    const bool bypassed = parameters.getRawParameterValue("bypass")->load() > 0.5f;
    const float inGain = juce::Decibels::decibelsToGain(
        parameters.getRawParameterValue("inputGain")->load());
    const float outGain = juce::Decibels::decibelsToGain(
        parameters.getRawParameterValue("outputGain")->load());
    const float wet = parameters.getRawParameterValue("mix")->load() * 0.01f;

    if (!bypassed)
    {
        juce::AudioBuffer<float> dry;
        dry.makeCopyOf(buffer, true);
        buffer.applyGain(inGain);

        ModelPtr model;
        {
            std::lock_guard<std::mutex> lock(modelMutex);
            model = activeModel;
        }

        if (model && modelReady.load())
        {
            // NAM Core's DSP API accepts channel-pointer arrays.
            // This build currently supports mono models only; mismatched models
            // fall back to dry audio and display a status in the UI.
            if (model->NumInputChannels() == 1 && model->NumOutputChannels() == 1)
            {
                float* inChannels[1] = { buffer.getWritePointer(0) };
                float* outChannels[1] = { buffer.getWritePointer(0) };
                model->process(inChannels, outChannels, n);
                buffer.applyGain(outGain);

                const float dryGain = 1.0f - wet;
                const float wetGain = wet;
                for (int i = 0; i < n; ++i)
                    buffer.setSample(0, i, buffer.getSample(0, i) * wetGain
                        + dry.getSample(0, i) * dryGain);
            }
            else
            {
                buffer.makeCopyOf(dry, true);
                setStatus("This prototype currently requires a mono-in / mono-out NAM model");
            }
        }
        else
        {
            // No active model: transparent gain stage so host routing remains useful.
            buffer.makeCopyOf(dry, true);
            buffer.applyGain(inGain * outGain);
        }
    }

    float outPeakNow = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        outPeakNow = juce::jmax(outPeakNow, buffer.getMagnitude(ch, 0, n));
    outputPeak.store(outPeakNow);
    if (outPeakNow >= 0.999f)
        clipping.store(true);
}

void NAMAudioProcessor::setStatus(const juce::String& status, bool failed)
{
    const juce::ScopedLock lock(stateLock);
    modelStatus = status;
    modelLoadFailed.store(failed);
}

juce::String NAMAudioProcessor::getModelStatus() const
{
    const juce::ScopedLock lock(stateLock);
    return modelStatus;
}

juce::String NAMAudioProcessor::getModelPath() const
{
    const juce::ScopedLock lock(stateLock);
    return modelPath;
}

void NAMAudioProcessor::loadModelAsync(const juce::File& file)
{
    if (!file.existsAsFile() || !file.getFileExtension().equalsIgnoreCase(".nam"))
    {
        setStatus("Select a valid .nam file", true);
        return;
    }

    setStatus("Loading " + file.getFileName() + " ...");
    modelLoadFailed.store(false);

    loaderPool.addJob([this, file]
    {
        try
        {
            auto loadedUnique = nam::get_dsp(std::filesystem::path(file.getFullPathName().toStdString()));
            if (!loadedUnique)
                throw std::runtime_error("NAM Core returned an empty model");

            // NAM Reset may allocate and prewarm; it must not run on the audio thread.
            loadedUnique->Reset(currentSampleRate, currentBlockSize);
            if (loadedUnique->NumInputChannels() != 1 || loadedUnique->NumOutputChannels() != 1)
                throw std::runtime_error("Only mono-in / mono-out NAM models are supported by this build");

            ModelPtr loaded(std::move(loadedUnique));
            {
                std::lock_guard<std::mutex> lock(modelMutex);
                activeModel = std::move(loaded);
            }
            {
                const juce::ScopedLock lock(stateLock);
                modelPath = file.getFullPathName();
                modelStatus = "Loaded: " + file.getFileName();
            }
            modelReady.store(true);
            modelLoadFailed.store(false);
        }
        catch (const std::exception& e)
        {
            setStatus("Load failed: " + juce::String(e.what()), true);
        }
    });
}

void NAMAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty("modelPath", getModelPath(), nullptr);
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void NAMAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
    {
        auto state = juce::ValueTree::fromXml(*xml);
        if (state.isValid())
        {
            const auto path = state.getProperty("modelPath").toString();
            parameters.replaceState(state);
            if (path.isNotEmpty())
                loadModelAsync(juce::File(path));
        }
    }
}

juce::AudioProcessorEditor* NAMAudioProcessor::createEditor()
{
    return new NAMAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NAMAudioProcessor();
}

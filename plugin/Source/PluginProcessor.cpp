#include "PluginProcessor.h"
#include "PluginEditor.h"

// --- SearchThread ------------------------------------------------------

class ClauducerAudioProcessor::SearchThread : public juce::Thread
{
public:
    SearchThread(ClauducerAudioProcessor& ownerIn, BackendClient& backendIn,
                 juce::String audioPathIn, juce::String promptIn)
        : juce::Thread("clauducer-search"), owner(ownerIn), backend(backendIn),
          audioPath(std::move(audioPathIn)), prompt(std::move(promptIn))
    {
    }

    void run() override
    {
        BackendClient::SearchResponse response;
        auto result = backend.search(audioPath, prompt, response);

        if (result.failed())
        {
            auto message = result.getErrorMessage();
            juce::MessageManager::callAsync([&ownerRef = owner, message]
            {
                ownerRef.listeners.call([&](ClauducerAudioProcessor::SearchListener& l) { l.searchFailed(message); });
            });
        }
        else
        {
            juce::MessageManager::callAsync([&ownerRef = owner, response]
            {
                ownerRef.listeners.call([&](ClauducerAudioProcessor::SearchListener& l) { l.searchCompleted(response); });
            });
        }
    }

private:
    ClauducerAudioProcessor& owner;
    BackendClient& backend;
    juce::String audioPath;
    juce::String prompt;
};

// --- ClauducerAudioProcessor --------------------------------------------

ClauducerAudioProcessor::ClauducerAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

ClauducerAudioProcessor::~ClauducerAudioProcessor() = default;

bool ClauducerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void ClauducerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    {
        const juce::ScopedLock sl(ringBufferLock);
        ringBuffer.setSize(2, static_cast<int>(kCaptureBufferSeconds * sampleRate));
        ringBuffer.clear();
        ringWritePos = 0;
    }

    juce::ignoreUnused(samplesPerBlock);
}

void ClauducerAudioProcessor::releaseResources()
{
}

void ClauducerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = juce::jmin(buffer.getNumChannels(), 2);

    // Mirror input into the ring buffer for later capture. Never blocks the
    // audio thread: if a capture-in-progress holds the lock, this block is
    // simply not mirrored (rare -- capture is a brief, user-initiated action).
    {
        const juce::GenericScopedTryLock<juce::CriticalSection> tryLock(ringBufferLock);
        if (tryLock.isLocked() && ringBuffer.getNumSamples() > 0)
        {
            const int ringLength = ringBuffer.getNumSamples();
            for (int i = 0; i < numSamples; ++i)
            {
                const int writeIndex = (ringWritePos + i) % ringLength;
                for (int ch = 0; ch < numChannels; ++ch)
                    ringBuffer.setSample(ch, writeIndex, buffer.getSample(ch, i));
            }
            ringWritePos = (ringWritePos + numSamples) % ringLength;
        }
    }
    // Passthrough -- buffer already holds the input audio, leave it untouched.
}

juce::AudioProcessorEditor* ClauducerAudioProcessor::createEditor()
{
    return new ClauducerAudioProcessorEditor(*this);
}

juce::String ClauducerAudioProcessor::captureReferenceToTempFile(double seconds)
{
    juce::AudioBuffer<float> snapshot;
    int channels = 0;

    {
        const juce::GenericScopedTryLock<juce::CriticalSection> tryLock(ringBufferLock);
        if (!tryLock.isLocked() || ringBuffer.getNumSamples() == 0)
            return {};

        channels = ringBuffer.getNumChannels();
        const int ringLength = ringBuffer.getNumSamples();
        const int numToCopy = juce::jmin(ringLength, static_cast<int>(seconds * currentSampleRate));
        snapshot.setSize(channels, numToCopy);

        // Ring buffer's oldest sample we want is (writePos - numToCopy), read forward from there.
        const int startIndex = ((ringWritePos - numToCopy) % ringLength + ringLength) % ringLength;
        for (int i = 0; i < numToCopy; ++i)
        {
            const int readIndex = (startIndex + i) % ringLength;
            for (int ch = 0; ch < channels; ++ch)
                snapshot.setSample(ch, i, ringBuffer.getSample(ch, readIndex));
        }
    }

    auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile("clauducer_capture_" + juce::String(juce::Time::currentTimeMillis()) + ".wav");

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> outputStream(tempFile.createOutputStream());
    if (outputStream == nullptr)
        return {};

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(outputStream.get(), currentSampleRate, static_cast<unsigned>(channels), 16, {}, 0));
    if (writer == nullptr)
        return {};

    outputStream.release(); // writer now owns it
    writer->writeFromAudioSampleBuffer(snapshot, 0, snapshot.getNumSamples());
    writer.reset(); // flush

    return tempFile.getFullPathName();
}

void ClauducerAudioProcessor::runSearch(const juce::String& capturedAudioPath, const juce::String& prompt)
{
    listeners.call([](SearchListener& l) { l.searchStarted(); });
    searchThread = std::make_unique<SearchThread>(*this, backend, capturedAudioPath, prompt);
    searchThread->startThread();
}

juce::Result ClauducerAudioProcessor::downloadForDrag(const juce::String& assetUuid, const juce::String& name, juce::String& outLocalPath)
{
    return backend.download(assetUuid, name, outLocalPath);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ClauducerAudioProcessor();
}

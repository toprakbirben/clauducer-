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
        log("Loaded " + juce::File(audioPath).getFileName());

        BackendClient::AnalyzeResponse analysis;
        auto result = backend.analyze(audioPath, analysis);
        if (result.failed())
        {
            fail(result.getErrorMessage());
            return;
        }
        log(describeFeatures(analysis.features));
        if (analysis.feeling.isNotEmpty())
            log("Feeling: " + analysis.feeling);

        if (threadShouldExit())
            return;

        log("Searching Splice...");
        BackendClient::SearchResponse response;
        result = backend.search(audioPath, prompt, response, analysis.features);

        if (result.failed())
        {
            fail(result.getErrorMessage());
            return;
        }

        log("Query: \"" + response.query + "\"");
        log(juce::String(response.results.size()) + " results");
        juce::MessageManager::callAsync([&ownerRef = owner, response]
        {
            ownerRef.listeners.call([&](ClauducerAudioProcessor::SearchListener& l) { l.searchCompleted(response); });
        });
    }

private:
    // e.g. "12.3s · Tempo 86 BPM · C# minor · dark, punchy, thin"
    static juce::String describeFeatures(const juce::var& f)
    {
        juce::StringArray parts;
        parts.add(juce::String(static_cast<double>(f.getProperty("duration_sec", 0.0)), 1) + "s");
        const auto bpm = static_cast<double>(f.getProperty("tempo_bpm", 0.0));
        parts.add(bpm >= 20.0 ? "Tempo " + juce::String(juce::roundToInt(bpm)) + " BPM" : juce::String("No steady tempo"));
        parts.add(f.getProperty("key", "").toString() + " " + f.getProperty("mode", "").toString());
        juce::StringArray timbre;
        if (auto* descriptors = f.getProperty("timbre_descriptors", juce::var()).getArray())
            for (auto& d : *descriptors)
                timbre.add(d.toString());
        if (!timbre.isEmpty())
            parts.add(timbre.joinIntoString(", "));
        return parts.joinIntoString(juce::CharPointer_UTF8(" \xc2\xb7 "));
    }

    void log(const juce::String& line)
    {
        juce::MessageManager::callAsync([&ownerRef = owner, line]
        {
            ownerRef.listeners.call([&](ClauducerAudioProcessor::SearchListener& l) { l.searchLog(line); });
        });
    }

    void fail(const juce::String& message)
    {
        log("Error: " + message);
        juce::MessageManager::callAsync([&ownerRef = owner, message]
        {
            ownerRef.listeners.call([&](ClauducerAudioProcessor::SearchListener& l) { l.searchFailed(message); });
        });
    }

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

    // Audio isn't running during prepareToPlay, so reallocating is safe --
    // but an in-progress capture can't survive it.
    auto state = captureState.load();
    if (state == CaptureState::armed || state == CaptureState::recording)
        captureState = CaptureState::stopped;
    captureBuffer.setSize(2, static_cast<int>(kMaxCaptureSeconds * sampleRate));

    juce::ignoreUnused(samplesPerBlock);
}

void ClauducerAudioProcessor::releaseResources()
{
}

void ClauducerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Passthrough -- buffer already holds the input audio and is left
    // untouched; capture below only reads from it.
    auto state = captureState.load();
    if (state != CaptureState::armed && state != CaptureState::recording)
        return;

    const int numSamples = buffer.getNumSamples();
    auto* playHead = getPlayHead();
    const auto position = playHead != nullptr ? playHead->getPosition() : juce::Optional<juce::AudioPlayHead::PositionInfo>();
    const bool playing = position.hasValue() && position->getIsPlaying();
    hostPlaying = playing;

    if (!playing)
    {
        if (state == CaptureState::recording)
            captureState.compare_exchange_strong(state, CaptureState::stopped);
        return;
    }

    int startOffset = 0;
    if (state == CaptureState::armed)
    {
        // Find where the next bar line falls in this block. Hosts without
        // tempo/position info fall back to 120 BPM, recording from now.
        const double bpm = position->getBpm().orFallback(120.0);
        const auto sig = position->getTimeSignature().orFallback(juce::AudioPlayHead::TimeSignature {});
        const double quartersPerBar = sig.numerator * 4.0 / sig.denominator;
        const double samplesPerQuarter = 60.0 / bpm * currentSampleRate;

        double quartersToNextBar = 0.0;
        if (auto ppq = position->getPpqPosition())
        {
            const double lastBar = position->getPpqPositionOfLastBarStart().orFallback(std::floor(*ppq / quartersPerBar) * quartersPerBar);
            const double intoBar = std::fmod(*ppq - lastBar + quartersPerBar, quartersPerBar);
            if (intoBar > 1.0e-6 && quartersPerBar - intoBar > 1.0e-6)
                quartersToNextBar = quartersPerBar - intoBar;
        }

        startOffset = juce::roundToInt(quartersToNextBar * samplesPerQuarter);
        if (startOffset >= numSamples)
            return; // bar line is in a later block

        const int samplesPerBar = juce::roundToInt(quartersPerBar * samplesPerQuarter);
        captureLength = juce::jmin(captureBars.load() * samplesPerBar, captureBuffer.getNumSamples());
        captureSamplesPerBar = samplesPerBar;
        captureWritten = 0;
        if (!captureState.compare_exchange_strong(state, CaptureState::recording))
            return; // cancelled meanwhile
    }

    const int written = captureWritten.load();
    const int toCopy = juce::jmin(numSamples - startOffset, captureLength - written);
    for (int ch = 0; ch < captureBuffer.getNumChannels(); ++ch)
        captureBuffer.copyFrom(ch, written, buffer, juce::jmin(ch, buffer.getNumChannels() - 1), startOffset, toCopy);
    captureWritten = written + toCopy;

    if (written + toCopy >= captureLength)
    {
        state = CaptureState::recording;
        captureState.compare_exchange_strong(state, CaptureState::done);
    }
}

juce::AudioProcessorEditor* ClauducerAudioProcessor::createEditor()
{
    return new ClauducerAudioProcessorEditor(*this);
}

void ClauducerAudioProcessor::startBarCapture(int bars, const juce::String& prompt)
{
    if (captureState.load() != CaptureState::idle)
        return;
    captureBars = bars;
    capturePrompt = prompt;
    lastCaptureStatus = {};
    captureState = CaptureState::armed;
    startTimerHz(20);
    timerCallback(); // report the initial status right away
}

void ClauducerAudioProcessor::cancelBarCapture()
{
    stopTimer();
    captureState = CaptureState::idle;
}

void ClauducerAudioProcessor::timerCallback()
{
    const auto report = [this](const juce::String& status)
    {
        if (status == lastCaptureStatus)
            return;
        lastCaptureStatus = status;
        listeners.call([&](SearchListener& l) { l.captureStatus(status); });
    };

    switch (captureState.load())
    {
        case CaptureState::idle:
            stopTimer();
            break;

        case CaptureState::armed:
            report(hostPlaying.load() ? "Starting on the next bar..." : "Waiting for playback...");
            break;

        case CaptureState::recording:
        {
            const int samplesPerBar = juce::jmax(1, captureSamplesPerBar.load());
            const int bar = juce::jmin(captureWritten.load() / samplesPerBar + 1, captureBars.load());
            report("Listening... bar " + juce::String(bar) + " of " + juce::String(captureBars.load()));
            break;
        }

        case CaptureState::done:
        {
            stopTimer();
            auto path = writeCaptureToTempFile(captureWritten.load());
            captureState = CaptureState::idle;
            if (path.isEmpty())
                listeners.call([](SearchListener& l) { l.searchFailed("Couldn't write the captured audio to a temp file."); });
            else
                runSearch(path, capturePrompt);
            break;
        }

        case CaptureState::stopped:
        {
            stopTimer();
            captureState = CaptureState::idle;
            const auto message = "Playback stopped before " + juce::String(captureBars.load()) + " bars were recorded.";
            listeners.call([&](SearchListener& l) { l.searchFailed(message); });
            break;
        }
    }
}

juce::String ClauducerAudioProcessor::writeCaptureToTempFile(int numSamples)
{
    auto tempFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getChildFile("clauducer_capture_" + juce::String(juce::Time::currentTimeMillis()) + ".wav");

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::FileOutputStream> outputStream(tempFile.createOutputStream());
    if (outputStream == nullptr)
        return {};

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(outputStream.get(), currentSampleRate, static_cast<unsigned>(captureBuffer.getNumChannels()), 16, {}, 0));
    if (writer == nullptr)
        return {};

    outputStream.release(); // writer now owns it
    writer->writeFromAudioSampleBuffer(captureBuffer, 0, numSamples);
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

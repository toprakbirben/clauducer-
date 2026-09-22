#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "BackendClient.h"

/** Audio-effect plugin: passes track audio through untouched and, on request,
    records the next few bars of it (bar-aligned via the host playhead). See the plan file for the
    full design -- this class owns capture and the background search call;
    ResultsListComponent (in the editor) owns the per-row drag-and-drop
    gesture and calls downloadForDrag() directly.

    No in-plugin preview playback: confirmed live against Splice's real MCP
    tools that describe_a_sound returns no preview-audio URL, only a link to
    the sound's Splice webpage, so there is no audio source to play. Dropped
    for v1 rather than built against data that doesn't exist.
*/
class ClauducerAudioProcessor : public juce::AudioProcessor,
                                private juce::Timer
{
public:
    ClauducerAudioProcessor();
    ~ClauducerAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Clauducer"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

    // --- Bar capture ---
    /** Records the next `bars` bars of track audio -- starting on the next
        bar line once the host transport is playing -- then runs a search
        with it and `prompt`. Progress arrives via SearchListener::captureStatus;
        a transport stop mid-recording arrives as searchFailed.
        Message thread only; ignored if a capture is already in progress.
    */
    void startBarCapture(int bars, const juce::String& prompt);
    void cancelBarCapture();
    bool isCapturing() const { return captureState.load() != CaptureState::idle; }

    // --- Search (backend /search, run on a background thread) ---
    // Named SearchListener (not Listener) to avoid colliding with
    // juce::AudioProcessor's own addListener/removeListener(AudioProcessorListener*).
    struct SearchListener
    {
        virtual ~SearchListener() = default;
        virtual void searchStarted() {}
        virtual void searchCompleted(const BackendClient::SearchResponse&) {}
        virtual void searchFailed(const juce::String&) {}
        // One human-readable progress line per step (analysis, feeling, query, results).
        virtual void searchLog(const juce::String&) {}
        // Bar-capture progress, e.g. "Waiting for playback..." / "Listening... bar 2 of 4".
        virtual void captureStatus(const juce::String&) {}
    };
    void addSearchListener(SearchListener* l) { listeners.add(l); }
    void removeSearchListener(SearchListener* l) { listeners.remove(l); }
    /** Kicks off POST /analyze then POST /search on a background thread; results (or failure)
        arrive later via Listener callbacks on the message thread.
    */
    void runSearch(const juce::String& capturedAudioPath, const juce::String& prompt);

    // --- Drag-out download ---
    /** Blocking call to POST /download -- spends a Splice credit as a side
        effect of succeeding. Only call this right before starting the OS
        drag (see ResultsListComponent::mouseDrag), never speculatively.
    */
    juce::Result downloadForDrag(const juce::String& assetUuid, const juce::String& name, juce::String& outLocalPath);

private:
    // 8 bars of 4/4 at 32 BPM -- longer captures are truncated.
    static constexpr double kMaxCaptureSeconds = 60.0;

    // Polls captureState while a capture is in progress (message thread).
    void timerCallback() override;
    juce::String writeCaptureToTempFile(int numSamples);

    BackendClient backend;
    juce::ListenerList<SearchListener> listeners;

    // Capture hand-off between threads: the message thread arms it (idle ->
    // armed), the audio thread advances it (armed -> recording -> done, or
    // -> stopped if the transport stops), and the message thread collects
    // the result (done/stopped -> idle). captureBuffer is only touched by
    // the audio thread while armed/recording, and by the message thread
    // otherwise, so it needs no lock.
    enum class CaptureState { idle, armed, recording, done, stopped };
    std::atomic<CaptureState> captureState { CaptureState::idle };
    juce::AudioBuffer<float> captureBuffer; // preallocated in prepareToPlay
    std::atomic<int> captureBars { 4 };
    std::atomic<int> captureSamplesPerBar { 0 };
    std::atomic<int> captureWritten { 0 };
    std::atomic<bool> hostPlaying { false };
    int captureLength = 0; // audio thread only
    juce::String capturePrompt; // message thread only
    juce::String lastCaptureStatus; // message thread only
    double currentSampleRate = 44100.0;

    class SearchThread;
    std::unique_ptr<SearchThread> searchThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClauducerAudioProcessor)
};

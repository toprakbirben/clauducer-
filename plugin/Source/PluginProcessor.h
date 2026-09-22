#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "BackendClient.h"

/** Audio-effect plugin: passes track audio through untouched while mirroring
    it into a ring buffer for on-demand capture. See the plan file for the
    full design -- this class owns capture and the background search call;
    ResultsListComponent (in the editor) owns the per-row drag-and-drop
    gesture and calls downloadForDrag() directly.

    No in-plugin preview playback: confirmed live against Splice's real MCP
    tools that describe_a_sound returns no preview-audio URL, only a link to
    the sound's Splice webpage, so there is no audio source to play. Dropped
    for v1 rather than built against data that doesn't exist.
*/
class ClauducerAudioProcessor : public juce::AudioProcessor
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

    // --- Capture ---
    /** Writes the last `seconds` of mirrored track audio to a temp WAV file
        and returns its path, or an empty string if nothing has been
        captured yet (e.g. called before any audio has played through).
        Call from the message thread (UI button handler).
    */
    juce::String captureReferenceToTempFile(double seconds);

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
    static constexpr double kCaptureBufferSeconds = 10.0;

    BackendClient backend;
    juce::ListenerList<SearchListener> listeners;

    // Ring buffer mirroring recent input audio. The audio thread writes
    // without blocking; captureReferenceToTempFile() (message thread) uses a
    // try-lock so it never stalls the audio thread -- if the lock is briefly
    // held, that capture attempt should be retried by the caller.
    juce::CriticalSection ringBufferLock;
    juce::AudioBuffer<float> ringBuffer;
    int ringWritePos = 0;
    double currentSampleRate = 44100.0;

    class SearchThread;
    std::unique_ptr<SearchThread> searchThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClauducerAudioProcessor)
};

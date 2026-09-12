#pragma once

#include <juce_core/juce_core.h>
#include <vector>

/** Blocking HTTP client for the local Python backend (backend/service.py).
    Every method does a synchronous network call -- callers must invoke these
    off the message thread (see PluginProcessor's background search thread)
    to avoid freezing the plugin UI or the host.
*/
class BackendClient
{
public:
    struct SearchResult
    {
        juce::String name;
        double bpm = 0.0;
        juce::String key;
        juce::String link;
        juce::String assetUuid;
        // Always empty for now: confirmed live against Splice's real
        // describe_a_sound MCP tool that it returns no preview-audio URL,
        // only a link to the sound's Splice webpage. Kept as a field so a
        // future preview feature has somewhere to plug in if a real source
        // for preview audio is found.
        juce::String previewUrl;
    };

    struct SearchResponse
    {
        std::vector<SearchResult> results;
    };

    explicit BackendClient(juce::String baseUrl = "http://127.0.0.1:8787");

    /** POST /search. On failure, returns a failed juce::Result with a message
        suitable for direct display in the UI (connection error, backend error, etc).
    */
    juce::Result search(const juce::String& audioPath, const juce::String& prompt, SearchResponse& out);

    /** POST /download. Spends a Splice credit as a side effect of this call
        succeeding -- callers must only invoke this once a real drag has
        started (see ResultsListComponent), never speculatively.
    */
    juce::Result download(const juce::String& assetUuid, const juce::String& name, juce::String& outLocalPath);

private:
    juce::String baseUrl;

    juce::Result postJson(const juce::String& path, const juce::var& body, juce::var& outResponse);
};

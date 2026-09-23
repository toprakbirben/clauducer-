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
        double durationSec = 0.0; // 0 = unknown
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
        juce::String query; // the natural-language query sent to Splice
    };

    struct AnalyzeResponse
    {
        juce::var features; // passed back to search() to skip re-analysis
        juce::String feeling;
    };

    explicit BackendClient(juce::String baseUrl = "http://127.0.0.1:8787");

    /** POST /analyze: audio features plus a short "feeling" description. */
    juce::Result analyze(const juce::String& audioPath, AnalyzeResponse& out);

    /** POST /search. On failure, returns a failed juce::Result with a message
        suitable for direct display in the UI (connection error, backend error, etc).
        Pass the features from a prior analyze() to skip re-analyzing the audio.
    */
    juce::Result search(const juce::String& audioPath, const juce::String& prompt, SearchResponse& out,
                        const juce::var& features = {});

    /** POST /download. Spends a Splice credit as a side effect of this call
        succeeding -- callers must only invoke this once a real drag has
        started (see ResultsListComponent), never speculatively.
    */
    juce::Result download(const juce::String& assetUuid, const juce::String& name, juce::String& outLocalPath);

    struct AuthStatus
    {
        bool authorized = false;
        bool loginInProgress = false;
        juce::String loginError; // last failed login attempt, if any
    };

    /** GET /auth/status. Fails only if the backend is unreachable or errors. */
    juce::Result authStatus(AuthStatus& out);

    /** POST /auth/login: the backend opens the browser OAuth flow and returns
        immediately -- poll authStatus() until authorized. */
    juce::Result startLogin();

private:
    juce::String baseUrl;

    juce::Result postJson(const juce::String& path, const juce::var& body, juce::var& outResponse);
    juce::Result getJson(const juce::String& path, juce::var& outResponse);
    juce::Result sendRequest(const juce::URL& url, const juce::String& path, bool isPost, int timeoutMs,
                             juce::var& outResponse);
};

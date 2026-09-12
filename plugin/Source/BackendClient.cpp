#include "BackendClient.h"

BackendClient::BackendClient(juce::String baseUrlIn) : baseUrl(std::move(baseUrlIn)) {}

juce::Result BackendClient::postJson(const juce::String& path, const juce::var& body, juce::var& outResponse)
{
    juce::URL url(baseUrl + path);
    url = url.withPOSTData(juce::JSON::toString(body));

    // On macOS this backs NSMutableURLRequest.timeoutInterval, which is the
    // total request timeout, not just the TCP handshake -- confirmed live
    // that a 15s value here was aborting /search before the backend's own
    // claude-CLI-backed search (which can legitimately take up to 90s, see
    // backend/service.py) finished, surfacing as a false "backend
    // unreachable" error even though the backend was running the whole
    // time. 100s gives margin over that 90s ceiling.
    static constexpr int kTimeoutMs = 100000;

    int statusCode = 0;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                       .withExtraHeaders("Content-Type: application/json")
                       .withConnectionTimeoutMs(kTimeoutMs)
                       .withStatusCode(&statusCode);

    auto stream = url.createInputStream(options);
    if (stream == nullptr)
        return juce::Result::fail("Could not reach backend at " + baseUrl + path
                                   + " within " + juce::String(kTimeoutMs / 1000)
                                   + "s -- either it's not running (uvicorn service:app --host 127.0.0.1 --port 8787), "
                                     "or this specific call is taking unusually long.");

    auto responseText = stream->readEntireStreamAsString();
    auto parsed = juce::JSON::parse(responseText);

    if (statusCode < 200 || statusCode >= 300)
    {
        juce::String detail = responseText;
        if (auto* obj = parsed.getDynamicObject())
            if (obj->hasProperty("detail"))
                detail = obj->getProperty("detail").toString();
        return juce::Result::fail("Backend error (" + juce::String(statusCode) + "): " + detail);
    }

    outResponse = parsed;
    return juce::Result::ok();
}

juce::Result BackendClient::search(const juce::String& audioPath, const juce::String& prompt, SearchResponse& out)
{
    auto* body = new juce::DynamicObject();
    body->setProperty("audio_path", audioPath);
    body->setProperty("prompt", prompt);

    juce::var response;
    auto result = postJson("/search", juce::var(body), response);
    if (result.failed())
        return result;

    out.results.clear();
    if (auto* resultsArray = response.getProperty("results", juce::var()).getArray())
    {
        for (auto& item : *resultsArray)
        {
            SearchResult r;
            r.name = item.getProperty("name", "").toString();
            r.bpm = static_cast<double>(item.getProperty("bpm", 0.0));
            r.key = item.getProperty("key", "").toString();
            r.link = item.getProperty("link", "").toString();
            r.assetUuid = item.getProperty("asset_uuid", "").toString();
            r.previewUrl = item.getProperty("preview_url", "").toString();
            out.results.push_back(std::move(r));
        }
    }
    return juce::Result::ok();
}

juce::Result BackendClient::download(const juce::String& assetUuid, const juce::String& name, juce::String& outLocalPath)
{
    auto* body = new juce::DynamicObject();
    body->setProperty("asset_uuid", assetUuid);
    body->setProperty("name", name);

    juce::var response;
    auto result = postJson("/download", juce::var(body), response);
    if (result.failed())
        return result;

    outLocalPath = response.getProperty("local_path", "").toString();
    if (outLocalPath.isEmpty())
        return juce::Result::fail("Backend returned no local_path for downloaded asset " + assetUuid);

    return juce::Result::ok();
}

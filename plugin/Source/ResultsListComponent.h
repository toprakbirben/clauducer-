#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BackendClient.h"
#include "PluginProcessor.h"

/** One row per Splice search result: name/metadata, and the row itself is
    the drag source for dragging the sound out to an Ableton track. A plain
    click (not a drag) fires onPreviewRequested, which the editor uses to
    show the sound's Splice webpage in an embedded PreviewPanel (falls back
    to the default browser if unset).

    No direct audio preview: Splice's MCP tools don't return a preview-audio
    URL, the page's preview audio is scrambled, and scraping it was rejected
    -- Splice's Terms of Use (splice.com/terms, Section II(8)(j)) prohibit
    automated scraping/data extraction from their web pages.

    JUCE's ListBoxModel::getDragSourceDescription is for internal (JUCE
    component to JUCE component) drags, not OS-level external drag-out, so
    it isn't used here. Instead each row is a real Component (supplied via
    refreshComponentForRow) that gets its own raw mouse events and calls
    DragAndDropContainer::performExternalDragDropOfFiles() directly once a
    drag gesture is detected. The containing editor must derive from
    juce::DragAndDropContainer for that call to have somewhere to originate
    from -- see PluginEditor.
*/
class ResultsListComponent : public juce::Component,
                              private juce::ListBoxModel
{
public:
    explicit ResultsListComponent(ClauducerAudioProcessor& processorIn);

    void setResults(std::vector<BackendClient::SearchResult> newResults);
    void resized() override;

    /** Fired on the message thread when a drag-out attempt fails (backend
        unreachable, download error, etc) so the editor can show a toast.
        Not fired for a successful drag, nor for the user simply cancelling
        the OS drag gesture (that's not an error).
    */
    std::function<void(const juce::String& errorMessage)> onDragError;

    /** Fired on a plain click (not a drag) on a result row. */
    std::function<void(const BackendClient::SearchResult&)> onPreviewRequested;

private:
    class ResultRow;
    friend class ResultRow;

    // --- juce::ListBoxModel ---
    int getNumRows() override;
    void paintListBoxItem(int, juce::Graphics&, int width, int height, bool) override;
    juce::Component* refreshComponentForRow(int rowNumber, bool isRowSelected, juce::Component* existingComponentToUpdate) override;

    // Starts the OS drag for `row`. This is where downloadForDrag() gets
    // called -- i.e. where the Splice credit actually gets spent -- right
    // before performExternalDragDropOfFiles(), never earlier (e.g. not on
    // hover or on preview).
    void startDragForRow(int row, juce::Component* dragSourceComponent);

    // A plain click (no drag) on row -- previews its Splice webpage.
    void openWebpageForRow(int row);

    ClauducerAudioProcessor& processor;
    juce::ListBox listBox { "results", this };
    std::vector<BackendClient::SearchResult> results;

    // Local cache of already-downloaded files this session, keyed by
    // asset_uuid, so dragging the same result twice in one session doesn't
    // repeat the network round trip. Splice itself only charges a credit on
    // the first download of a given asset (repeats are free on their side
    // too), but there's no reason to re-hit the backend either way.
    std::map<juce::String, juce::String> downloadedPathsByAssetUuid;
};

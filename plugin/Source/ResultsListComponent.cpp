#include "ResultsListComponent.h"
#include "AppFont.h"

// --- ResultRow -----------------------------------------------------------

class ResultsListComponent::ResultRow : public juce::Component
{
public:
    ResultRow(ResultsListComponent& ownerIn) : owner(ownerIn)
    {
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }

    void setRowIndex(int newIndex, const BackendClient::SearchResult* result)
    {
        rowIndex = newIndex;
        if (result != nullptr)
        {
            nameText = result->name;
            // One-shots have no bpm (0) -- leave it out rather than show "0 BPM".
            juce::StringArray meta;
            if (result->bpm > 0.0)
                meta.add(juce::String(result->bpm, 0) + " BPM");
            if (result->key.isNotEmpty())
                meta.add(result->key);
            if (result->durationSec > 0.0)
                meta.add(juce::String(result->durationSec, 1) + "s");
            metaText = meta.joinIntoString("   ");
        }
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::transparentBlack);

        auto area = getLocalBounds().reduced(8, 6);

        g.setColour(juce::Colour(0xff1c2030));
        g.setFont(appFont(15.0f));
        const bool isPlaying = rowIndex >= 0 && rowIndex == owner.playingRow;
        g.drawFittedText(isPlaying ? juce::String(juce::CharPointer_UTF8("\xe2\x96\xb6 ")) + nameText : nameText, area.removeFromTop(area.getHeight() * 2 / 3),
                          juce::Justification::centredLeft, 1);

        // Split before drawing either, so the hint can never overlap the
        // meta text even on a narrow row.
        auto hintArea = area.removeFromRight(110);

        g.setColour(juce::Colour(0xff5a6072));
        g.setFont(juce::Font(12.0f));
        g.drawFittedText(metaText, area, juce::Justification::centredLeft, 1);

        // Discoverability hint: dragging the row onto an Ableton track
        // downloads it (spending a Splice credit) -- not obvious just from
        // the cursor alone, so spell it out.
        g.setColour(juce::Colour(0xff8a90a0));
        g.setFont(juce::Font(11.0f, juce::Font::italic));
        g.drawFittedText(juce::CharPointer_UTF8("drag to add \xe2\x86\x92"), hintArea,
                          juce::Justification::centredRight, 1);

        g.setColour(juce::Colour(0xffe2e5ec));
        g.drawLine(0.0f, static_cast<float>(getHeight() - 1), static_cast<float>(getWidth()), static_cast<float>(getHeight() - 1));
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        owner.rowHovered(rowIndex);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragStartPos = e.getPosition();
        dragStarted = false;
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragStarted)
            return;

        // A small threshold so a plain click doesn't also register as a drag.
        if (e.getPosition().getDistanceFrom(dragStartPos) > 8)
        {
            dragStarted = true;
            owner.startDragForRow(rowIndex, this);
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        // A plain click (mouse went up without ever crossing the drag
        // threshold above) plays/stops the sound's preview instead.
        if (!dragStarted)
            owner.togglePreviewForRow(rowIndex);
    }

private:
    ResultsListComponent& owner;
    juce::String nameText, metaText;
    int rowIndex = -1;
    juce::Point<int> dragStartPos;
    bool dragStarted = false;
};

// --- ResultsListComponent -------------------------------------------------

ResultsListComponent::ResultsListComponent(ClauducerAudioProcessor& processorIn) : processor(processorIn)
{
    addAndMakeVisible(listBox);
    listBox.setRowHeight(48);
    // Transparent so the editor's own background shows through when there
    // are no results yet, instead of a flat rectangle sitting over it.
    listBox.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
}

void ResultsListComponent::setResults(std::vector<BackendClient::SearchResult> newResults)
{
    stopPreview();
    results = std::move(newResults);
    listBox.updateContent();
    listBox.repaint();
}

void ResultsListComponent::resized()
{
    listBox.setBounds(getLocalBounds());
}

int ResultsListComponent::getNumRows()
{
    return static_cast<int>(results.size());
}

void ResultsListComponent::paintListBoxItem(int, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    g.fillAll(rowIsSelected ? juce::Colour(0xffe6e9f0) : juce::Colour(0xfff6f7fb));
    juce::ignoreUnused(width, height);
}

juce::Component* ResultsListComponent::refreshComponentForRow(int rowNumber, bool, juce::Component* existingComponentToUpdate)
{
    auto* row = dynamic_cast<ResultRow*>(existingComponentToUpdate);
    if (row == nullptr)
    {
        delete existingComponentToUpdate;
        row = new ResultRow(*this);
    }

    const auto* result = (rowNumber >= 0 && rowNumber < static_cast<int>(results.size())) ? &results[static_cast<size_t>(rowNumber)] : nullptr;
    row->setRowIndex(rowNumber, result);
    return row;
}

void ResultsListComponent::startDragForRow(int row, juce::Component* dragSourceComponent)
{
    if (row < 0 || row >= static_cast<int>(results.size()))
        return;

    const auto& result = results[static_cast<size_t>(row)];

    auto* container = juce::DragAndDropContainer::findParentDragContainerFor(dragSourceComponent);
    if (container == nullptr)
        return; // editor isn't a DragAndDropContainer -- see PluginEditor

    juce::String localPath;
    auto cached = downloadedPathsByAssetUuid.find(result.assetUuid);
    if (cached != downloadedPathsByAssetUuid.end())
    {
        localPath = cached->second;
    }
    else
    {
        // Blocking on the message thread: deliberate -- the credit spend
        // and the drag start need to happen as one atomic user gesture, and
        // this call is expected to complete in well under a second against
        // a local backend. A future version could show a spinner if this
        // turns out to be too slow in practice.
        auto result_ = processor.downloadForDrag(result.assetUuid, result.name, localPath);
        if (result_.failed())
        {
            if (onDragError)
                onDragError(result_.getErrorMessage());
            return;
        }
        downloadedPathsByAssetUuid[result.assetUuid] = localPath;
    }

    container->performExternalDragDropOfFiles({ localPath }, false, dragSourceComponent);
}

void ResultsListComponent::togglePreviewForRow(int row)
{
    if (row < 0 || row >= static_cast<int>(results.size()))
        return;

    const auto& result = results[static_cast<size_t>(row)];
    if (row == playingRow || result.link.isEmpty())
    {
        stopPreview();
        return;
    }

    setPlayingRow(row);
    if (onPreviewChanged)
        onPreviewChanged(&result);
}

void ResultsListComponent::rowHovered(int row)
{
    if (onRowHovered && row >= 0 && row < static_cast<int>(results.size()))
        onRowHovered(results[static_cast<size_t>(row)]);
}

void ResultsListComponent::stopPreview()
{
    if (playingRow < 0)
        return;

    setPlayingRow(-1);
    if (onPreviewChanged)
        onPreviewChanged(nullptr);
}

void ResultsListComponent::setPlayingRow(int row)
{
    playingRow = row;
    listBox.updateContent();
    listBox.repaint();
}

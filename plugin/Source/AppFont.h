#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"

/** The plugin's display typeface -- Google's "Unica One" (static, regular
    weight only), embedded via juce_add_binary_data (see CMakeLists.txt) so
    it doesn't depend on the font being installed on the host machine. */
inline juce::Font appFont(float height)
{
    static const juce::Typeface::Ptr typeface =
        juce::Typeface::createSystemTypefaceFor(BinaryData::UnicaOneRegular_ttf,
                                                  (size_t) BinaryData::UnicaOneRegular_ttfSize);
    return juce::Font(juce::FontOptions(typeface)).withHeight(height);
}

#pragma once

#include <Geode/Geode.hpp>
#include <Geode/ui/Label.hpp>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

// Emojis travel over the network as ASCII shortcodes (":fire:") and are only
// turned into sprites when displayed, via geode::Label's emoji registry.

namespace gc::emoji {

namespace detail {
    const std::vector<std::pair<std::u32string_view, const char*>>& registryEntries();
    const std::vector<std::pair<std::string_view, std::u8string_view>>& shortcodeEntries();
}

struct PickerEntry {
    std::string_view shortcode;
    const char* frameName;
};

geode::EmojiRegistry const& registry();

/// Replaces every known ":shortcode:" in `text` with its UTF-8 emoji.
std::string translate(std::string_view text);

/// Sprite frame for a shortcode (without colons), or nullptr if unknown.
const char* frameFor(std::string_view shortcode);

/// One entry per distinct emoji, for the picker.
std::vector<PickerEntry> const& pickerEntries();

/// A label with emoji support. `maxWidth` > 0 enables line wrapping.
geode::Label* createLabel(std::string_view text, geode::ZStringView font, float maxWidth = 0.f);

}

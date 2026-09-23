#include "Emojis.hpp"

#include <unordered_map>

using namespace geode::prelude;

namespace gc::emoji {

EmojiRegistry const& registry() {
    static EmojiRegistry reg = [] {
        EmojiRegistry r;
        for (auto& [seq, frame] : detail::registryEntries()) {
            r.insert(seq, frame);
        }
        return r;
    }();
    return reg;
}

static std::unordered_map<std::string_view, std::u8string_view> const& shortcodeMap() {
    static std::unordered_map<std::string_view, std::u8string_view> map = [] {
        std::unordered_map<std::string_view, std::u8string_view> m;
        for (auto& [name, value] : detail::shortcodeEntries()) m.emplace(name, value);
        return m;
    }();
    return map;
}

std::string translate(std::string_view text) {
    auto& map = shortcodeMap();
    std::string out;
    out.reserve(text.size());

    size_t i = 0;
    while (i < text.size()) {
        if (text[i] == ':') {
            auto end = text.find(':', i + 1);
            if (end != std::string_view::npos) {
                auto it = map.find(text.substr(i + 1, end - i - 1));
                if (it != map.end()) {
                    out.append(reinterpret_cast<const char*>(it->second.data()), it->second.size());
                    i = end + 1;
                    continue;
                }
            }
        }
        out.push_back(text[i++]);
    }

    return out;
}

static std::u32string decodeUtf8(std::u8string_view s) {
    std::u32string out;
    for (size_t i = 0; i < s.size();) {
        auto c = static_cast<uint8_t>(s[i]);
        size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xe ? 3 : 4;
        char32_t cp = len == 1 ? c : len == 2 ? (c & 0x1f) : len == 3 ? (c & 0x0f) : (c & 0x07);
        for (size_t j = 1; j < len && i + j < s.size(); j++) {
            cp = (cp << 6) | (static_cast<uint8_t>(s[i + j]) & 0x3f);
        }
        out.push_back(cp);
        i += len;
    }
    return out;
}

static std::u32string stripVariation(std::u32string_view s) {
    std::u32string out;
    for (auto c : s) {
        if (c != 0xfe0f) out.push_back(c);
    }
    return out;
}

std::vector<PickerEntry> const& pickerEntries() {
    static std::vector<PickerEntry> entries = [] {
        std::unordered_map<std::u32string, const char*> frames;
        for (auto& [seq, frame] : detail::registryEntries()) {
            frames.emplace(stripVariation(seq), frame);
        }

        std::vector<PickerEntry> out;
        std::unordered_map<std::u32string, bool> seen;

        for (auto& [name, value] : detail::shortcodeEntries()) {
            auto key = stripVariation(decodeUtf8(value));
            if (seen[key]) continue;
            seen[key] = true;

            auto it = frames.find(key);
            if (it != frames.end()) out.push_back({name, it->second});
        }
        return out;
    }();
    return entries;
}

const char* frameFor(std::string_view shortcode) {
    for (auto& e : pickerEntries()) {
        if (e.shortcode == shortcode) return e.frameName;
    }
    return nullptr;
}

Label* createLabel(std::string_view text, ZStringView font, float maxWidth) {
    // not createRich: users shouldn't be able to inject color tags
    auto label = Label::create(font);
    label->setEmojiRegistry(registry());
    if (maxWidth > 0.f) {
        label->setMaxWidth(maxWidth);
        label->setBreakWords(true);
    }
    label->setText(translate(text));
    return label;
}

}

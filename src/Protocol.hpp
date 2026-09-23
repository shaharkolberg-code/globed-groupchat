#pragma once

#include <Geode/Geode.hpp>
#include <globed/core/Event.hpp>

#include <optional>
#include <span>
#include <string>
#include <vector>

// Wire format for the group chat events sent through Globed's central server.
//
// The server caps each event payload at 1024 bytes and only delivers targeted
// events to players in the same Globed room as the sender. It also stamps the
// real sender account ID into EventOptions::sender, so we never trust IDs from
// the payload for authentication.

namespace gc {

constexpr size_t MAX_MEMBERS = 16;
constexpr size_t MAX_GROUP_NAME = 32;
constexpr size_t MAX_USERNAME = 20;
constexpr size_t MAX_MESSAGE = 200;
constexpr size_t MAX_LEVEL_NAME = 32;

/// Keeps only printable ASCII (all GD bitmap fonts can render) and truncates.
std::string sanitize(std::string_view in, size_t maxLen);

class Writer {
public:
    void u8(uint8_t v) { m_buf.push_back(v); }

    void i32(int32_t v) {
        auto u = static_cast<uint32_t>(v);
        for (int i = 0; i < 4; i++) m_buf.push_back(static_cast<uint8_t>(u >> (i * 8)));
    }

    void u64(uint64_t v) {
        for (int i = 0; i < 8; i++) m_buf.push_back(static_cast<uint8_t>(v >> (i * 8)));
    }

    void str(std::string_view s, size_t maxLen) {
        auto clean = sanitize(s, std::min<size_t>(maxLen, 255));
        this->u8(static_cast<uint8_t>(clean.size()));
        m_buf.insert(m_buf.end(), clean.begin(), clean.end());
    }

    std::vector<uint8_t> finish() && { return std::move(m_buf); }

private:
    std::vector<uint8_t> m_buf;
};

class Reader {
public:
    explicit Reader(std::span<const uint8_t> data) : m_data(data) {}

    geode::Result<uint8_t> u8() {
        if (m_pos + 1 > m_data.size()) return geode::Err("unexpected end of data");
        return geode::Ok(m_data[m_pos++]);
    }

    geode::Result<int32_t> i32() {
        if (m_pos + 4 > m_data.size()) return geode::Err("unexpected end of data");
        uint32_t u = 0;
        for (int i = 0; i < 4; i++) u |= static_cast<uint32_t>(m_data[m_pos++]) << (i * 8);
        return geode::Ok(static_cast<int32_t>(u));
    }

    geode::Result<uint64_t> u64() {
        if (m_pos + 8 > m_data.size()) return geode::Err("unexpected end of data");
        uint64_t u = 0;
        for (int i = 0; i < 8; i++) u |= static_cast<uint64_t>(m_data[m_pos++]) << (i * 8);
        return geode::Ok(u);
    }

    geode::Result<std::string> str(size_t maxLen) {
        GEODE_UNWRAP_INTO(auto len, this->u8());
        if (m_pos + len > m_data.size()) return geode::Err("string runs past end of data");
        std::string_view raw{reinterpret_cast<const char*>(m_data.data() + m_pos), len};
        m_pos += len;
        return geode::Ok(sanitize(raw, maxLen));
    }

private:
    std::span<const uint8_t> m_data;
    size_t m_pos = 0;
};

struct Member {
    int accountId = 0;
    std::string username;
};

/// Sent by the group owner to every member whenever the group changes.
/// Receiving one for an unknown group is an invite.
struct SyncEvent : globed::ServerEvent<SyncEvent, globed::EventServer::Central> {
    static constexpr auto Id = "sync"_spr;

    uint64_t groupId = 0;
    std::string name;
    int owner = 0;
    std::vector<Member> members;

    std::vector<uint8_t> encode() const;
    static geode::Result<SyncEvent> decode(std::span<const uint8_t> data);
};

/// A level attached to a chat message.
struct SharedLevel {
    int levelId = 0;
    std::string name;
    std::string creator;
};

/// A chat message, sent to every other member of the group.
/// Emojis are sent as ":shortcode:" text and rendered on the receiving end.
struct ChatEvent : globed::ServerEvent<ChatEvent, globed::EventServer::Central> {
    static constexpr auto Id = "chat"_spr;

    enum class Kind : uint8_t { Text = 0, Level = 1 };

    uint64_t groupId = 0;
    std::string text; // may be empty for a level share
    std::optional<SharedLevel> level;

    std::vector<uint8_t> encode() const;
    static geode::Result<ChatEvent> decode(std::span<const uint8_t> data);
};

/// Sent by a member who leaves a group or declines an invite.
struct LeaveEvent : globed::ServerEvent<LeaveEvent, globed::EventServer::Central> {
    static constexpr auto Id = "leave"_spr;

    uint64_t groupId = 0;

    std::vector<uint8_t> encode() const;
    static geode::Result<LeaveEvent> decode(std::span<const uint8_t> data);
};

}

#include "Protocol.hpp"

using namespace geode::prelude;

namespace gc {

std::string sanitize(std::string_view in, size_t maxLen) {
    std::string out;
    out.reserve(std::min(in.size(), maxLen));

    for (char c : in) {
        if (out.size() >= maxLen) break;
        if (c >= 0x20 && c <= 0x7e) out.push_back(c);
    }

    return out;
}

std::vector<uint8_t> SyncEvent::encode() const {
    Writer w;
    w.u64(groupId);
    w.str(name, MAX_GROUP_NAME);
    w.i32(owner);

    auto count = std::min(members.size(), MAX_MEMBERS);
    w.u8(static_cast<uint8_t>(count));
    for (size_t i = 0; i < count; i++) {
        w.i32(members[i].accountId);
        w.str(members[i].username, MAX_USERNAME);
    }

    return std::move(w).finish();
}

Result<SyncEvent> SyncEvent::decode(std::span<const uint8_t> data) {
    Reader r{data};
    SyncEvent ev;

    GEODE_UNWRAP_INTO(ev.groupId, r.u64());
    GEODE_UNWRAP_INTO(ev.name, r.str(MAX_GROUP_NAME));
    GEODE_UNWRAP_INTO(ev.owner, r.i32());
    GEODE_UNWRAP_INTO(auto count, r.u8());

    if (count > MAX_MEMBERS) return Err("too many members");

    for (uint8_t i = 0; i < count; i++) {
        Member m;
        GEODE_UNWRAP_INTO(m.accountId, r.i32());
        GEODE_UNWRAP_INTO(m.username, r.str(MAX_USERNAME));
        ev.members.push_back(std::move(m));
    }

    return Ok(std::move(ev));
}

std::vector<uint8_t> ChatEvent::encode() const {
    Writer w;
    w.u64(groupId);
    w.u8(static_cast<uint8_t>(level ? Kind::Level : Kind::Text));
    w.str(text, MAX_MESSAGE);

    if (level) {
        w.i32(level->levelId);
        w.str(level->name, MAX_LEVEL_NAME);
        w.str(level->creator, MAX_USERNAME);
    }

    return std::move(w).finish();
}

Result<ChatEvent> ChatEvent::decode(std::span<const uint8_t> data) {
    Reader r{data};
    ChatEvent ev;

    GEODE_UNWRAP_INTO(ev.groupId, r.u64());
    GEODE_UNWRAP_INTO(auto kind, r.u8());
    GEODE_UNWRAP_INTO(ev.text, r.str(MAX_MESSAGE));

    switch (static_cast<Kind>(kind)) {
        case Kind::Text: break;
        case Kind::Level: {
            SharedLevel lvl;
            GEODE_UNWRAP_INTO(lvl.levelId, r.i32());
            GEODE_UNWRAP_INTO(lvl.name, r.str(MAX_LEVEL_NAME));
            GEODE_UNWRAP_INTO(lvl.creator, r.str(MAX_USERNAME));
            if (lvl.levelId <= 0) return Err("invalid level ID");
            ev.level = std::move(lvl);
        } break;
        default: return Err("unknown message kind");
    }

    return Ok(std::move(ev));
}

std::vector<uint8_t> LeaveEvent::encode() const {
    Writer w;
    w.u64(groupId);
    return std::move(w).finish();
}

Result<LeaveEvent> LeaveEvent::decode(std::span<const uint8_t> data) {
    Reader r{data};
    LeaveEvent ev;

    GEODE_UNWRAP_INTO(ev.groupId, r.u64());

    return Ok(std::move(ev));
}

}

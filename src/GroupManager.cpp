#include "GroupManager.hpp"

#include <Geode/binding/GJAccountManager.hpp>
#include <globed/soft-link/API.hpp>

#include <algorithm>
#include <ctime>
#include <random>

using namespace geode::prelude;

namespace gc {

// Keep well under the server's event rate limit so we never get disconnected.
constexpr auto SEND_COOLDOWN = std::chrono::milliseconds(750);

bool Group::hasMember(int accountId) const {
    return std::ranges::any_of(members, [&](auto& m) { return m.accountId == accountId; });
}

std::string Group::nameOf(int accountId) const {
    for (auto& m : members) {
        if (m.accountId == accountId) return m.username;
    }
    return fmt::format("#{}", accountId);
}

GroupManager& GroupManager::get() {
    static GroupManager instance;
    return instance;
}

void GroupManager::init() {
    SyncEvent::listen([](const SyncEvent& ev, const globed::EventOptions& opts) {
        GroupManager::get().onSync(ev, opts.sender);
    }).leak();

    ChatEvent::listen([](const ChatEvent& ev, const globed::EventOptions& opts) {
        GroupManager::get().onChat(ev, opts.sender);
    }).leak();

    LeaveEvent::listen([](const LeaveEvent& ev, const globed::EventOptions& opts) {
        GroupManager::get().onLeave(ev, opts.sender);
    }).leak();
}

int GroupManager::selfId() {
    return GJAccountManager::get()->m_accountID;
}

std::string GroupManager::selfName() {
    return std::string(GJAccountManager::get()->m_username);
}

bool GroupManager::isOnline() {
    return !offlineReason().has_value();
}

std::optional<std::string> GroupManager::offlineReason() {
    if (!globed::api::available()) {
        // Globed v2.2.2 shipped without its API exported (fixed in v2.2.3)
        auto mod = Loader::get()->getInstalledMod("dankmeme.globed2");
        auto version = mod ? mod->getVersion().toVString() : std::string("?");
        return fmt::format("Globed {} has a bug that blocks other mods. Update Globed to v2.2.3 or newer.", version);
    }
    if (!globed::api::net::isConnected()) {
        return "Not connected to Globed";
    }
    return std::nullopt;
}

void GroupManager::ensureLoaded() {
    // the GD account isn't known at mod load, and the player can switch accounts
    if (m_loadedFor != selfId()) {
        this->load();
    }
}

std::vector<Group*> GroupManager::groups() {
    this->ensureLoaded();

    std::vector<Group*> out;
    for (auto& [_, g] : m_groups) out.push_back(&g);

    // pending invites first, then alphabetical
    std::ranges::sort(out, [](Group* a, Group* b) {
        if (a->pending != b->pending) return a->pending;
        return a->name < b->name;
    });
    return out;
}

Group* GroupManager::find(uint64_t id) {
    this->ensureLoaded();
    auto it = m_groups.find(id);
    return it == m_groups.end() ? nullptr : &it->second;
}

std::vector<int> GroupManager::othersIn(const Group& group) {
    std::vector<int> out;
    int self = selfId();
    for (auto& m : group.members) {
        if (m.accountId != self) out.push_back(m.accountId);
    }
    return out;
}

void GroupManager::sendSync(const Group& group, std::vector<int> extraTargets) {
    auto targets = othersIn(group);
    targets.insert(targets.end(), extraTargets.begin(), extraTargets.end());
    if (targets.empty()) return;

    SyncEvent ev;
    ev.groupId = group.id;
    ev.name = group.name;
    ev.owner = group.owner;
    ev.members = group.members;

    globed::EventOptions opts;
    opts.targetPlayers.assign(targets.begin(), targets.end());
    ev.send(std::move(opts));
}

Result<> GroupManager::createGroup(std::string name) {
    name = sanitize(name, MAX_GROUP_NAME);
    if (name.empty()) return Err("Group name can't be empty");
    if (selfId() <= 0) return Err("You need to be logged into a GD account");

    static std::mt19937_64 rng{std::random_device{}()};

    Group g;
    g.id = rng();
    g.name = std::move(name);
    g.owner = selfId();
    g.members.push_back({selfId(), sanitize(selfName(), MAX_USERNAME)});

    this->ensureLoaded();
    m_groups.emplace(g.id, std::move(g));
    this->save();
    this->changed();
    return Ok();
}

Result<> GroupManager::addMember(uint64_t groupId, int accountId, std::string username) {
    auto* g = this->find(groupId);
    if (!g) return Err("Group not found");
    if (g->owner != selfId()) return Err("Only the group owner can add members");
    if (g->hasMember(accountId)) return Err("They're already in this group");
    if (g->members.size() >= MAX_MEMBERS) return Err(fmt::format("Groups are limited to {} members", MAX_MEMBERS));
    if (auto reason = offlineReason()) return Err(*reason);

    g->members.push_back({accountId, sanitize(username, MAX_USERNAME)});
    this->sendSync(*g);
    this->save();
    this->changed();
    return Ok();
}

Result<> GroupManager::kickMember(uint64_t groupId, int accountId) {
    auto* g = this->find(groupId);
    if (!g) return Err("Group not found");
    if (g->owner != selfId()) return Err("Only the group owner can remove members");
    if (accountId == selfId()) return Err("Use Leave to leave your own group");
    if (auto reason = offlineReason()) return Err(*reason);

    std::erase_if(g->members, [&](auto& m) { return m.accountId == accountId; });
    // the removed player also gets the sync, sees they're not in it, and drops the group
    this->sendSync(*g, {accountId});
    this->save();
    this->changed();
    return Ok();
}

Result<> GroupManager::sendMessage(uint64_t groupId, std::string text, std::optional<SharedLevel> level) {
    auto* g = this->find(groupId);
    if (!g || g->pending) return Err("Group not found");

    text = sanitize(text, MAX_MESSAGE);
    if (text.empty() && !level) return Err("Message is empty");
    if (level) {
        if (level->levelId <= 0) return Err("Only uploaded levels can be shared");
        level->name = sanitize(level->name, MAX_LEVEL_NAME);
        level->creator = sanitize(level->creator, MAX_USERNAME);
    }
    if (auto reason = offlineReason()) return Err(*reason);

    auto now = std::chrono::steady_clock::now();
    if (now - m_lastSend < SEND_COOLDOWN) return Err("Slow down!");
    m_lastSend = now;

    auto targets = othersIn(*g);
    if (!targets.empty()) {
        ChatEvent ev;
        ev.groupId = groupId;
        ev.text = text;
        ev.level = level;

        globed::EventOptions opts;
        opts.targetPlayers.assign(targets.begin(), targets.end());
        ev.send(std::move(opts));
    }

    this->pushHistory(*g, {selfId(), std::move(text), std::time(nullptr), std::move(level)});
    this->save();
    this->changed();
    return Ok();
}

Result<> GroupManager::acceptInvite(uint64_t groupId) {
    auto* g = this->find(groupId);
    if (!g || !g->pending) return Err("Invite not found");

    g->pending = false;
    this->save();
    this->changed();
    return Ok();
}

void GroupManager::leaveGroup(uint64_t groupId) {
    auto* g = this->find(groupId);
    if (!g) return;

    int self = selfId();
    auto others = othersIn(*g);

    if (isOnline() && !others.empty()) {
        if (g->owner == self) {
            // hand ownership to the next member so the group survives
            Group copy = *g;
            std::erase_if(copy.members, [&](auto& m) { return m.accountId == self; });
            copy.owner = copy.members.front().accountId;
            this->sendSync(copy);
        } else {
            LeaveEvent ev;
            ev.groupId = groupId;

            globed::EventOptions opts;
            opts.targetPlayers.assign(others.begin(), others.end());
            ev.send(std::move(opts));
        }
    }

    m_groups.erase(groupId);
    this->save();
    this->changed();
}

void GroupManager::markRead(uint64_t groupId) {
    if (auto* g = this->find(groupId); g && g->unread) {
        g->unread = 0;
        this->save();
        this->changed();
    }
}

void GroupManager::onSync(const SyncEvent& ev, int sender) {
    int self = selfId();
    if (sender <= 0 || self <= 0) return;

    bool ownerIncluded = std::ranges::any_of(ev.members, [&](auto& m) { return m.accountId == ev.owner; });
    bool selfIncluded = std::ranges::any_of(ev.members, [&](auto& m) { return m.accountId == self; });
    if (!ownerIncluded && selfIncluded) return; // malformed

    auto* g = this->find(ev.groupId);

    if (g) {
        // only the current owner may change an existing group
        if (sender != g->owner) return;

        if (!selfIncluded) {
            auto name = g->name;
            m_groups.erase(ev.groupId);
            Notification::create(fmt::format("You were removed from \"{}\"", name), NotificationIcon::Info)->show();
        } else {
            g->name = ev.name;
            g->owner = ev.owner;
            g->members = ev.members;
        }
    } else {
        // a new group is only valid as an invite from its owner
        if (!selfIncluded || sender != ev.owner) return;

        Group ng;
        ng.id = ev.groupId;
        ng.name = ev.name;
        ng.owner = ev.owner;
        ng.members = ev.members;
        ng.pending = !Mod::get()->getSettingValue<bool>("auto-accept-invites");

        auto msg = ng.pending
            ? fmt::format("{} invited you to \"{}\"", ng.nameOf(sender), ng.name)
            : fmt::format("{} added you to \"{}\"", ng.nameOf(sender), ng.name);
        Notification::create(msg, NotificationIcon::Info)->show();

        m_groups.emplace(ng.id, std::move(ng));
    }

    this->save();
    this->changed();
}

void GroupManager::onChat(const ChatEvent& ev, int sender) {
    auto* g = this->find(ev.groupId);
    if (!g || g->pending || !g->hasMember(sender)) return;
    if (ev.text.empty() && !ev.level) return;

    this->pushHistory(*g, {sender, ev.text, std::time(nullptr), ev.level});

    if (m_openChat != g->id) {
        g->unread++;
        if (Mod::get()->getSettingValue<bool>("message-notifications")) {
            auto text = ev.level
                ? fmt::format("[{}] {} shared \"{}\"", g->name, g->nameOf(sender), ev.level->name)
                : fmt::format("[{}] {}: {}", g->name, g->nameOf(sender), ev.text);
            Notification::create(text, NotificationIcon::None)->show();
        }
    }

    this->save();
    this->changed();
}

void GroupManager::onLeave(const LeaveEvent& ev, int sender) {
    auto* g = this->find(ev.groupId);
    if (!g || !g->hasMember(sender)) return;

    std::erase_if(g->members, [&](auto& m) { return m.accountId == sender; });

    // owner is the source of truth, so re-broadcast the new member list
    if (g->owner == selfId()) {
        this->sendSync(*g);
    }

    this->save();
    this->changed();
}

void GroupManager::pushHistory(Group& group, ChatMessage msg) {
    group.history.push_back(std::move(msg));

    auto limit = static_cast<size_t>(Mod::get()->getSettingValue<int64_t>("history-size"));
    while (group.history.size() > limit) group.history.pop_front();
}

int GroupManager::addObserver(std::function<void()> cb) {
    int token = m_nextObserver++;
    m_observers.emplace(token, std::move(cb));
    return token;
}

void GroupManager::removeObserver(int token) {
    m_observers.erase(token);
}

void GroupManager::changed() {
    // an observer may close its popup (removing itself or others) while we iterate
    std::vector<int> tokens;
    for (auto& [token, _] : m_observers) tokens.push_back(token);

    for (int token : tokens) {
        auto it = m_observers.find(token);
        if (it != m_observers.end()) {
            auto cb = it->second;
            cb();
        }
    }
}

void GroupManager::save() {
    auto arr = matjson::Value::array();

    for (auto& [id, g] : m_groups) {
        auto members = matjson::Value::array();
        for (auto& m : g.members) {
            auto mj = matjson::Value::object();
            mj["id"] = m.accountId;
            mj["name"] = m.username;
            members.push(mj);
        }

        auto history = matjson::Value::array();
        for (auto& msg : g.history) {
            auto hj = matjson::Value::object();
            hj["from"] = msg.sender;
            hj["text"] = msg.text;
            hj["time"] = msg.timestamp;
            if (msg.level) {
                hj["levelId"] = msg.level->levelId;
                hj["levelName"] = msg.level->name;
                hj["levelCreator"] = msg.level->creator;
            }
            history.push(hj);
        }

        auto gj = matjson::Value::object();
        // u64 doesn't survive a JSON double, so store it as a string
        gj["id"] = std::to_string(g.id);
        gj["name"] = g.name;
        gj["owner"] = g.owner;
        gj["pending"] = g.pending;
        gj["unread"] = g.unread;
        gj["members"] = members;
        gj["history"] = history;
        arr.push(gj);
    }

    // keyed by account so switching GD accounts doesn't mix groups
    Mod::get()->setSavedValue(fmt::format("groups-{}", selfId()), arr);
}

void GroupManager::load() {
    m_groups.clear();
    m_loadedFor = selfId();

    auto arr = Mod::get()->getSavedValue<matjson::Value>(fmt::format("groups-{}", selfId()));
    if (!arr.isArray()) return;

    for (auto& gj : arr) {
        Group g;
        g.id = numFromString<uint64_t>(gj["id"].asString().unwrapOr("0")).unwrapOr(0);
        if (g.id == 0) continue;

        g.name = gj["name"].asString().unwrapOr("");
        g.owner = static_cast<int>(gj["owner"].asInt().unwrapOr(0));
        g.pending = gj["pending"].asBool().unwrapOr(false);
        g.unread = static_cast<int>(gj["unread"].asInt().unwrapOr(0));

        auto& membersJson = gj["members"];
        auto& historyJson = gj["history"];
        if (!membersJson.isArray() || !historyJson.isArray()) continue;

        for (auto& mj : membersJson) {
            g.members.push_back({
                static_cast<int>(mj["id"].asInt().unwrapOr(0)),
                mj["name"].asString().unwrapOr(""),
            });
        }

        for (auto& hj : historyJson) {
            ChatMessage msg{
                static_cast<int>(hj["from"].asInt().unwrapOr(0)),
                hj["text"].asString().unwrapOr(""),
                static_cast<int64_t>(hj["time"].asInt().unwrapOr(0)),
            };

            if (auto levelId = hj["levelId"].asInt().unwrapOr(0); levelId > 0) {
                msg.level = SharedLevel{
                    static_cast<int>(levelId),
                    hj["levelName"].asString().unwrapOr(""),
                    hj["levelCreator"].asString().unwrapOr(""),
                };
            }

            g.history.push_back(std::move(msg));
        }

        m_groups.emplace(g.id, std::move(g));
    }
}

}

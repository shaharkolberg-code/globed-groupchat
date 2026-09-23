#pragma once

#include "Protocol.hpp"

#include <deque>
#include <functional>
#include <map>
#include <unordered_map>

namespace gc {

struct ChatMessage {
    int sender = 0;
    std::string text;
    int64_t timestamp = 0; // unix seconds
    std::optional<SharedLevel> level;
};

struct Group {
    uint64_t id = 0;
    std::string name;
    int owner = 0;
    std::vector<Member> members; // includes the owner
    std::deque<ChatMessage> history;
    bool pending = false; // invite not accepted yet
    int unread = 0;

    bool hasMember(int accountId) const;
    std::string nameOf(int accountId) const;
};

/// Owns all group state. Groups have no server-side storage: the owner is the
/// source of truth and pushes SyncEvents to members; everything else is local.
class GroupManager {
public:
    static GroupManager& get();

    void init();

    static int selfId();
    static std::string selfName();
    static bool isOnline();
    /// Why group chats can't be used right now, or nullopt if they can.
    static std::optional<std::string> offlineReason();

    std::vector<Group*> groups();
    Group* find(uint64_t id);

    geode::Result<> createGroup(std::string name);
    geode::Result<> addMember(uint64_t groupId, int accountId, std::string username);
    geode::Result<> kickMember(uint64_t groupId, int accountId);
    geode::Result<> sendMessage(uint64_t groupId, std::string text, std::optional<SharedLevel> level = std::nullopt);
    geode::Result<> acceptInvite(uint64_t groupId);
    void leaveGroup(uint64_t groupId); // also declines a pending invite

    void markRead(uint64_t groupId);
    void setOpenChat(uint64_t groupId) { m_openChat = groupId; }
    uint64_t openChat() const { return m_openChat; }

    /// UI observers, called on any group change. Returns a token for removeObserver.
    int addObserver(std::function<void()> cb);
    void removeObserver(int token);

private:
    std::map<uint64_t, Group> m_groups;
    std::unordered_map<int, std::function<void()>> m_observers;
    int m_nextObserver = 1;
    uint64_t m_openChat = 0;
    int m_loadedFor = -1;
    std::chrono::steady_clock::time_point m_lastSend{};

    void onSync(const SyncEvent& ev, int sender);
    void onChat(const ChatEvent& ev, int sender);
    void onLeave(const LeaveEvent& ev, int sender);

    void pushHistory(Group& group, ChatMessage msg);
    void sendSync(const Group& group, std::vector<int> extraTargets = {});
    static std::vector<int> othersIn(const Group& group);

    void ensureLoaded();
    void changed();
    void save();
    void load();
};

}

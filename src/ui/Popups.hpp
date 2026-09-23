#pragma once

#include "Common.hpp"
#include "../Protocol.hpp"

#include <functional>
#include <set>

namespace gc::ui {

/// All of the player's group chats and pending invites.
class GroupListPopup : public ObservingPopup {
public:
    static GroupListPopup* create();

protected:
    geode::ScrollLayer* m_list = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;

    bool initPopup();
    void refresh() override;
};

/// Asks for a name and creates a group owned by the player.
class CreateGroupPopup : public geode::Popup {
public:
    static CreateGroupPopup* create();

protected:
    geode::TextInput* m_input = nullptr;

    bool initPopup();
    void onCreate();
};

/// The chat view for one group.
class GroupChatPopup : public ObservingPopup {
public:
    static GroupChatPopup* create(uint64_t groupId);

protected:
    uint64_t m_groupId = 0;
    geode::ScrollLayer* m_list = nullptr;
    geode::TextInput* m_input = nullptr;
    cocos2d::CCLabelBMFont* m_status = nullptr;

    ~GroupChatPopup();
    bool initPopup(uint64_t groupId);
    void refresh() override;
    void onSend();
};

/// Member list; lets the owner kick and anyone leave.
class MembersPopup : public ObservingPopup {
public:
    static MembersPopup* create(uint64_t groupId);

protected:
    uint64_t m_groupId = 0;
    geode::ScrollLayer* m_list = nullptr;

    bool initPopup(uint64_t groupId);
    void refresh() override;
};

/// Grid of emojis; `onPick` gets the shortcode (without colons).
class EmojiPickerPopup : public geode::Popup {
public:
    static EmojiPickerPopup* create(std::function<void(std::string_view)> onPick);

protected:
    std::function<void(std::string_view)> m_onPick;

    bool initPopup(std::function<void(std::string_view)> onPick);
};

/// Opened from a level page: send that level to one of your groups.
class ShareLevelPopup : public ObservingPopup {
public:
    static ShareLevelPopup* create(SharedLevel level);

protected:
    SharedLevel m_level;
    std::set<uint64_t> m_sentTo;
    geode::ScrollLayer* m_list = nullptr;

    bool initPopup(SharedLevel level);
    void refresh() override;
};

/// Opened from a GD profile: add that player to one of your groups.
class AddToGroupPopup : public ObservingPopup {
public:
    static AddToGroupPopup* create(int accountId, std::string username);

protected:
    int m_accountId = 0;
    std::string m_username;
    geode::ScrollLayer* m_list = nullptr;

    bool initPopup(int accountId, std::string username);
    void refresh() override;
};

}

#include "Popups.hpp"
#include "../GroupManager.hpp"
#include "../Emojis.hpp"

#include <globed/soft-link/API.hpp>
#include <ctime>

using namespace geode::prelude;

namespace gc::ui {

constexpr float CHAT_WIDTH = 380.f;
constexpr float TEXT_SCALE = 0.65f;

GroupChatPopup* GroupChatPopup::create(uint64_t groupId) {
    auto ret = new GroupChatPopup();
    if (ret->initPopup(groupId)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

GroupChatPopup::~GroupChatPopup() {
    auto& gm = GroupManager::get();
    if (gm.openChat() == m_groupId) gm.setOpenChat(0);
}

bool GroupChatPopup::initPopup(uint64_t groupId) {
    if (!Popup::init(420.f, 290.f)) return false;
    m_groupId = groupId;

    auto& gm = GroupManager::get();
    auto* g = gm.find(groupId);
    if (!g) return false;

    this->setTitle(g->name);
    gm.setOpenChat(groupId);

    auto membersBtn = textButton("Members", 0.5f, [groupId](auto) {
        if (auto p = MembersPopup::create(groupId)) p->show();
    });
    m_buttonMenu->addChildAtPosition(membersBtn, Anchor::TopRight, {-45.f, -20.f});

    m_list = createList({CHAT_WIDTH, 180.f});
    addList(m_mainLayer, m_list, {0.f, 12.f});

    m_status = CCLabelBMFont::create("", "chatFont.fnt");
    m_status->setScale(0.55f);
    m_mainLayer->addChildAtPosition(m_status, Anchor::Bottom, {0.f, 52.f});

    m_input = TextInput::create(270.f, "Message (try :fire:)", "chatFont.fnt");
    m_input->setMaxCharCount(MAX_MESSAGE);
    m_input->setCommonFilter(CommonFilter::Any);
    m_mainLayer->addChildAtPosition(m_input, Anchor::Bottom, {-55.f, 25.f});

    CCNode* emojiSpr = nullptr;
    if (auto frame = emoji::frameFor("slight_smile")) {
        emojiSpr = CCSprite::createWithSpriteFrameName(frame);
    }
    if (emojiSpr) {
        emojiSpr->setScale(24.f / emojiSpr->getContentSize().width);
    } else {
        emojiSpr = ButtonSprite::create(":)", "goldFont.fnt", "GJ_button_01.png", 0.6f);
    }
    auto emojiBtn = CCMenuItemExt::createSpriteExtra(emojiSpr, [this](auto) {
        EmojiPickerPopup::create([self = Ref<GroupChatPopup>(this)](std::string_view shortcode) {
            auto text = std::string(self->m_input->getString());
            if (!text.empty() && text.back() != ' ') text += ' ';
            text += fmt::format(":{}: ", shortcode);
            self->m_input->setString(text.substr(0, MAX_MESSAGE));
        })->show();
    });
    m_buttonMenu->addChildAtPosition(emojiBtn, Anchor::Bottom, {103.f, 25.f});

    auto sendBtn = textButton("Send", 0.65f, [this](auto) { this->onSend(); });
    m_buttonMenu->addChildAtPosition(sendBtn, Anchor::Bottom, {160.f, 25.f});

    this->observe();
    this->refresh();
    return true;
}

static CCNode* createLevelCard(SharedLevel const& level) {
    auto card = createRow(CHAT_WIDTH - 20.f, 32.f);

    auto name = CCLabelBMFont::create(level.name.empty() ? "Unnamed level" : level.name.c_str(), "bigFont.fnt");
    name->setAnchorPoint({0.f, 0.5f});
    name->limitLabelWidth(250.f, 0.45f, 0.2f);
    name->setPosition({8.f, 21.f});
    card->addChild(name);

    auto sub = CCLabelBMFont::create(
        fmt::format("by {}  -  ID {}", level.creator.empty() ? "?" : level.creator, level.levelId).c_str(),
        "goldFont.fnt"
    );
    sub->setAnchorPoint({0.f, 0.5f});
    sub->setScale(0.4f);
    sub->setPosition({8.f, 8.f});
    card->addChild(sub);

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    card->addChild(menu);

    auto view = textButton("View", 0.5f, [id = level.levelId](auto) { openLevel(id); });
    view->setPosition({card->getContentSize().width - 30.f, 16.f});
    menu->addChild(view);

    return card;
}

void GroupChatPopup::refresh() {
    auto& gm = GroupManager::get();
    auto* g = gm.find(m_groupId);

    // left the group, got kicked, or declined elsewhere
    if (!g) {
        this->onClose(nullptr);
        return;
    }

    gm.markRead(m_groupId);

    if (!GroupManager::isOnline()) {
        m_status->setString("Not connected to Globed - messages can't be sent");
        m_status->setColor({255, 110, 110});
    } else if (globed::api::room::isInRoom()) {
        m_status->setString("In a Globed room: only members in this room will get messages");
        m_status->setColor({255, 220, 120});
    } else {
        m_status->setString("");
    }

    auto content = m_list->m_contentLayer;
    content->removeAllChildren();

    int self = GroupManager::selfId();

    for (auto& msg : g->history) {
        char timeBuf[16] = "";
        std::time_t t = msg.timestamp;
        if (auto* tm = std::localtime(&t)) std::strftime(timeBuf, sizeof(timeBuf), "%H:%M", tm);

        std::string text;
        if (msg.level) {
            text = msg.text.empty()
                ? fmt::format("[{}] {} shared a level:", timeBuf, g->nameOf(msg.sender))
                : fmt::format("[{}] {} shared a level: {}", timeBuf, g->nameOf(msg.sender), msg.text);
        } else {
            text = fmt::format("[{}] {}: {}", timeBuf, g->nameOf(msg.sender), msg.text);
        }

        auto label = emoji::createLabel(text, "chatFont.fnt", (CHAT_WIDTH - 10.f) / TEXT_SCALE);
        label->setScale(TEXT_SCALE);
        label->setAnchorPoint({0.f, 0.f});
        if (msg.sender == self) label->setColor({170, 220, 255});

        float labelHeight = label->getScaledContentSize().height;
        CCNode* card = msg.level ? createLevelCard(*msg.level) : nullptr;
        float cardHeight = card ? card->getContentSize().height + 3.f : 0.f;

        auto row = CCNode::create();
        row->setContentSize({CHAT_WIDTH, labelHeight + cardHeight});

        label->setPosition({5.f, cardHeight});
        row->addChild(label);

        if (card) {
            card->setAnchorPoint({0.f, 0.f});
            card->setPosition({10.f, 0.f});
            row->addChild(card);
        }

        content->addChild(row);
    }

    if (g->history.empty()) {
        auto label = CCLabelBMFont::create("No messages yet. Say hi!", "chatFont.fnt");
        label->setScale(TEXT_SCALE);
        label->setOpacity(150);
        content->addChild(label);
    }

    content->updateLayout();

    // newest message at the bottom, so show the bottom of the list
    if (content->getContentSize().height > m_list->getContentSize().height) {
        content->setPositionY(0.f);
    } else {
        m_list->scrollToTop();
    }
}

void GroupChatPopup::onSend() {
    auto res = GroupManager::get().sendMessage(m_groupId, m_input->getString());
    if (res.isErr()) {
        showError(res.unwrapErr());
        return;
    }
    m_input->setString("");
}

}

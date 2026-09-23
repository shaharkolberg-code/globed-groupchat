#include "Popups.hpp"
#include "../GroupManager.hpp"
#include "../Emojis.hpp"

using namespace geode::prelude;

namespace gc::ui {

// ---- EmojiPickerPopup ----

constexpr float PICKER_WIDTH = 300.f;
constexpr float PICKER_HEIGHT = 170.f;
constexpr float CELL = 30.f;

EmojiPickerPopup* EmojiPickerPopup::create(std::function<void(std::string_view)> onPick) {
    auto ret = new EmojiPickerPopup();
    if (ret->initPopup(std::move(onPick))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool EmojiPickerPopup::initPopup(std::function<void(std::string_view)> onPick) {
    if (!Popup::init(340.f, 240.f)) return false;
    m_onPick = std::move(onPick);

    this->setTitle("Emojis");

    // plain grid, no layout: ~300 buttons are cheaper to position by hand
    auto scroll = ScrollLayer::create({PICKER_WIDTH, PICKER_HEIGHT});
    addList(m_mainLayer, scroll, {0.f, -10.f});

    auto& entries = emoji::pickerEntries();
    int columns = static_cast<int>(PICKER_WIDTH / CELL);
    int rows = (static_cast<int>(entries.size()) + columns - 1) / columns;
    float height = std::max(PICKER_HEIGHT, rows * CELL);

    auto content = scroll->m_contentLayer;
    content->setContentSize({PICKER_WIDTH, height});

    auto menu = CCMenu::create();
    menu->setPosition({0.f, 0.f});
    menu->setContentSize({PICKER_WIDTH, height});
    content->addChild(menu);

    int i = 0;
    for (auto& entry : entries) {
        auto spr = CCSprite::createWithSpriteFrameName(entry.frameName);
        if (!spr) continue;
        spr->setScale(22.f / spr->getContentSize().width);

        auto shortcode = entry.shortcode;
        auto btn = CCMenuItemExt::createSpriteExtra(spr, [this, shortcode](auto) {
            if (m_onPick) m_onPick(shortcode);
        });

        int col = i % columns;
        int row = i / columns;
        btn->setPosition({col * CELL + CELL / 2, height - row * CELL - CELL / 2});
        menu->addChild(btn);
        i++;
    }

    scroll->scrollToTop();

    auto hint = CCLabelBMFont::create("Tip: you can also type :shortcodes: like :fire:", "chatFont.fnt");
    hint->setScale(0.5f);
    hint->setOpacity(170);
    m_mainLayer->addChildAtPosition(hint, Anchor::Bottom, {0.f, 14.f});

    return true;
}

// ---- ShareLevelPopup ----

constexpr float LIST_WIDTH = 260.f;

ShareLevelPopup* ShareLevelPopup::create(SharedLevel level) {
    auto ret = new ShareLevelPopup();
    if (ret->initPopup(std::move(level))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool ShareLevelPopup::initPopup(SharedLevel level) {
    if (!Popup::init(300.f, 250.f)) return false;
    m_level = std::move(level);

    this->setTitle(fmt::format("Share \"{}\"", m_level.name), "goldFont.fnt", 0.6f);

    m_list = createList({LIST_WIDTH, 170.f});
    addList(m_mainLayer, m_list, {0.f, -10.f});

    this->observe();
    this->refresh();
    return true;
}

void ShareLevelPopup::refresh() {
    auto content = m_list->m_contentLayer;
    content->removeAllChildren();

    bool any = false;

    for (auto* g : GroupManager::get().groups()) {
        if (g->pending) continue;
        any = true;

        uint64_t groupId = g->id;
        auto row = createRow(LIST_WIDTH, 28.f);

        auto label = CCLabelBMFont::create(g->name.c_str(), "bigFont.fnt");
        label->setAnchorPoint({0.f, 0.5f});
        label->limitLabelWidth(170.f, 0.45f, 0.2f);
        label->setPosition({8.f, 14.f});
        row->addChild(label);

        if (m_sentTo.contains(groupId)) {
            auto sent = CCLabelBMFont::create("Sent", "goldFont.fnt");
            sent->setScale(0.5f);
            sent->setPosition({LIST_WIDTH - 30.f, 14.f});
            row->addChild(sent);
        } else {
            auto menu = CCMenu::create();
            menu->setPosition({0.f, 0.f});
            row->addChild(menu);

            auto send = textButton("Send", 0.5f, [this, groupId](auto) {
                auto res = GroupManager::get().sendMessage(groupId, "", m_level);
                if (res.isErr()) {
                    showError(res.unwrapErr());
                    return;
                }
                m_sentTo.insert(groupId);
                // sendMessage already triggered a (deferred) refresh, which will show "Sent"
            });
            send->setPosition({LIST_WIDTH - 30.f, 14.f});
            menu->addChild(send);
        }

        content->addChild(row);
    }

    if (!any) {
        auto label = CCLabelBMFont::create(
            "You're not in any group chats yet.",
            "bigFont.fnt", LIST_WIDTH / 0.4f, kCCTextAlignmentCenter
        );
        label->setScale(0.4f);
        content->addChild(label);
    }

    content->updateLayout();
    m_list->scrollToTop();
}

}

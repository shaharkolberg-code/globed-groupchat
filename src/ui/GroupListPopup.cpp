#include "Popups.hpp"
#include "../GroupManager.hpp"

using namespace geode::prelude;

namespace gc::ui {

constexpr float LIST_WIDTH = 320.f;

GroupListPopup* GroupListPopup::create() {
    auto ret = new GroupListPopup();
    if (ret->initPopup()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool GroupListPopup::initPopup() {
    if (!Popup::init(360.f, 270.f)) return false;

    this->setTitle("Group Chats");

    m_status = CCLabelBMFont::create("", "bigFont.fnt");
    m_status->setScale(0.35f);
    m_mainLayer->addChildAtPosition(m_status, Anchor::Top, {0.f, -40.f});

    m_list = createList({LIST_WIDTH, 170.f});
    addList(m_mainLayer, m_list, {0.f, -5.f});

    auto newBtn = textButton("New Group", 0.7f, [](auto) {
        CreateGroupPopup::create()->show();
    });
    m_buttonMenu->addChildAtPosition(newBtn, Anchor::Bottom, {0.f, 24.f});

    this->observe();
    this->refresh();
    return true;
}

void GroupListPopup::refresh() {
    auto& gm = GroupManager::get();

    if (auto reason = GroupManager::offlineReason()) {
        m_status->setString(reason->c_str());
        m_status->setColor({255, 110, 110});
    } else {
        m_status->setString("Connected to Globed");
        m_status->setColor({120, 255, 120});
    }
    m_status->limitLabelWidth(330.f, 0.35f, 0.15f);

    auto content = m_list->m_contentLayer;
    content->removeAllChildren();

    auto groups = gm.groups();

    if (groups.empty()) {
        auto label = CCLabelBMFont::create(
            "No group chats yet.\nCreate one, then add people\nfrom their GD profile.",
            "bigFont.fnt", LIST_WIDTH / 0.4f, kCCTextAlignmentCenter
        );
        label->setScale(0.4f);
        content->addChild(label);
    }

    for (auto* g : groups) {
        uint64_t id = g->id;
        auto row = createRow(LIST_WIDTH, 34.f);

        std::string text = g->name;
        if (g->pending) text = fmt::format("{} (invite from {})", g->name, g->nameOf(g->owner));

        auto name = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        name->setAnchorPoint({0.f, 0.5f});
        name->limitLabelWidth(g->pending ? 180.f : 200.f, 0.5f, 0.2f);
        name->setPosition({10.f, 21.f});
        row->addChild(name);

        auto sub = CCLabelBMFont::create(
            g->unread ? fmt::format("{} new message{}", g->unread, g->unread == 1 ? "" : "s").c_str()
                      : fmt::format("{} member{}", g->members.size(), g->members.size() == 1 ? "" : "s").c_str(),
            "goldFont.fnt"
        );
        sub->setAnchorPoint({0.f, 0.5f});
        sub->setScale(0.4f);
        sub->setPosition({10.f, 8.f});
        if (g->unread) sub->setColor({120, 255, 120});
        row->addChild(sub);

        auto menu = CCMenu::create();
        menu->setContentSize({LIST_WIDTH, 34.f});
        menu->setPosition({0.f, 0.f});
        menu->ignoreAnchorPointForPosition(false);
        menu->setAnchorPoint({0.f, 0.f});
        row->addChild(menu);

        if (g->pending) {
            auto join = textButton("Join", 0.55f, [id](auto) {
                report(GroupManager::get().acceptInvite(id));
            });
            join->setPosition({LIST_WIDTH - 85.f, 17.f});
            menu->addChild(join);

            auto decline = textButton("Decline", 0.55f, [id](auto) {
                GroupManager::get().leaveGroup(id);
            });
            decline->setPosition({LIST_WIDTH - 35.f, 17.f});
            menu->addChild(decline);
        } else {
            auto open = textButton("Open", 0.6f, [id](auto) {
                if (auto p = GroupChatPopup::create(id)) p->show();
            });
            open->setPosition({LIST_WIDTH - 35.f, 17.f});
            menu->addChild(open);
        }

        content->addChild(row);
    }

    content->updateLayout();
    m_list->scrollToTop();
}

CreateGroupPopup* CreateGroupPopup::create() {
    auto ret = new CreateGroupPopup();
    if (ret->initPopup()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool CreateGroupPopup::initPopup() {
    if (!Popup::init(280.f, 140.f)) return false;

    this->setTitle("New Group");

    m_input = TextInput::create(220.f, "Group name");
    m_input->setMaxCharCount(MAX_GROUP_NAME);
    m_mainLayer->addChildAtPosition(m_input, Anchor::Center, {0.f, 5.f});

    auto btn = textButton("Create", 0.75f, [this](auto) { this->onCreate(); });
    m_buttonMenu->addChildAtPosition(btn, Anchor::Bottom, {0.f, 24.f});

    return true;
}

void CreateGroupPopup::onCreate() {
    auto res = GroupManager::get().createGroup(m_input->getString());
    if (res.isErr()) {
        showError(res.unwrapErr());
        return;
    }
    this->onClose(nullptr);
}

}

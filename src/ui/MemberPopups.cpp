#include "Popups.hpp"
#include "../GroupManager.hpp"

#include <Geode/binding/ProfilePage.hpp>

using namespace geode::prelude;

namespace gc::ui {

constexpr float LIST_WIDTH = 260.f;

static CCMenu* rowMenu(CCNode* row) {
    auto menu = CCMenu::create();
    menu->ignoreAnchorPointForPosition(false);
    menu->setAnchorPoint({0.f, 0.f});
    menu->setPosition({0.f, 0.f});
    menu->setContentSize(row->getContentSize());
    row->addChild(menu);
    return menu;
}

MembersPopup* MembersPopup::create(uint64_t groupId) {
    auto ret = new MembersPopup();
    if (ret->initPopup(groupId)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool MembersPopup::initPopup(uint64_t groupId) {
    if (!Popup::init(300.f, 260.f)) return false;
    m_groupId = groupId;

    this->setTitle("Members");

    m_list = createList({LIST_WIDTH, 160.f});
    addList(m_mainLayer, m_list, {0.f, 5.f});

    auto leaveBtn = textButton("Leave Group", 0.6f, [this, groupId](auto) {
        createQuickPopup(
            "Leave Group",
            "Leave this group chat? If you're the owner, ownership passes to the next member.",
            "Cancel", "Leave",
            [groupId](auto, bool leave) {
                if (leave) GroupManager::get().leaveGroup(groupId);
            }
        );
    });
    m_buttonMenu->addChildAtPosition(leaveBtn, Anchor::Bottom, {0.f, 24.f});

    this->observe();
    this->refresh();
    return true;
}

void MembersPopup::refresh() {
    auto* g = GroupManager::get().find(m_groupId);
    if (!g) {
        this->onClose(nullptr);
        return;
    }

    int self = GroupManager::selfId();
    bool isOwner = g->owner == self;
    uint64_t groupId = m_groupId;

    auto content = m_list->m_contentLayer;
    content->removeAllChildren();

    for (auto& m : g->members) {
        int accountId = m.accountId;
        auto row = createRow(LIST_WIDTH, 28.f);
        auto menu = rowMenu(row);

        std::string text = m.username;
        if (accountId == g->owner) text += " (owner)";
        if (accountId == self) text += " (you)";

        auto label = CCLabelBMFont::create(text.c_str(), "bigFont.fnt");
        label->limitLabelWidth(160.f, 0.45f, 0.2f);

        // tapping a name opens their GD profile
        auto nameBtn = CCMenuItemExt::createSpriteExtra(label, [accountId](auto) {
            ProfilePage::create(accountId, accountId == GroupManager::selfId())->show();
        });
        nameBtn->setAnchorPoint({0.f, 0.5f});
        nameBtn->setPosition({8.f, 14.f});
        menu->addChild(nameBtn);

        if (isOwner && accountId != self) {
            auto kick = textButton("Kick", 0.5f, [groupId, accountId](auto) {
                report(GroupManager::get().kickMember(groupId, accountId));
            });
            kick->setPosition({LIST_WIDTH - 30.f, 14.f});
            menu->addChild(kick);
        }

        content->addChild(row);
    }

    content->updateLayout();
    m_list->scrollToTop();
}

AddToGroupPopup* AddToGroupPopup::create(int accountId, std::string username) {
    auto ret = new AddToGroupPopup();
    if (ret->initPopup(accountId, std::move(username))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool AddToGroupPopup::initPopup(int accountId, std::string username) {
    if (!Popup::init(300.f, 250.f)) return false;
    m_accountId = accountId;
    m_username = std::move(username);

    this->setTitle(fmt::format("Add {} to...", m_username), "goldFont.fnt", 0.6f);

    m_list = createList({LIST_WIDTH, 150.f});
    addList(m_mainLayer, m_list, {0.f, 5.f});

    auto newBtn = textButton("New Group", 0.6f, [](auto) {
        CreateGroupPopup::create()->show();
    });
    m_buttonMenu->addChildAtPosition(newBtn, Anchor::Bottom, {0.f, 24.f});

    this->observe();
    this->refresh();
    return true;
}

void AddToGroupPopup::refresh() {
    auto content = m_list->m_contentLayer;
    content->removeAllChildren();

    int self = GroupManager::selfId();
    int target = m_accountId;
    std::string username = m_username;
    bool any = false;

    for (auto* g : GroupManager::get().groups()) {
        if (g->pending || g->owner != self) continue;
        any = true;

        uint64_t groupId = g->id;
        auto row = createRow(LIST_WIDTH, 28.f);
        auto menu = rowMenu(row);

        auto label = CCLabelBMFont::create(g->name.c_str(), "bigFont.fnt");
        label->setAnchorPoint({0.f, 0.5f});
        label->limitLabelWidth(160.f, 0.45f, 0.2f);
        label->setPosition({8.f, 14.f});
        row->addChild(label);

        if (g->hasMember(target)) {
            auto added = CCLabelBMFont::create("Added", "goldFont.fnt");
            added->setScale(0.5f);
            added->setPosition({LIST_WIDTH - 30.f, 14.f});
            row->addChild(added);
        } else {
            auto add = textButton("Add", 0.5f, [groupId, target, username](auto) {
                report(GroupManager::get().addMember(groupId, target, username));
            });
            add->setPosition({LIST_WIDTH - 30.f, 14.f});
            menu->addChild(add);
        }

        content->addChild(row);
    }

    if (!any) {
        auto label = CCLabelBMFont::create(
            "You don't own any groups.\nCreate one first.",
            "bigFont.fnt", LIST_WIDTH / 0.4f, kCCTextAlignmentCenter
        );
        label->setScale(0.4f);
        content->addChild(label);
    }

    content->updateLayout();
    m_list->scrollToTop();
}

}

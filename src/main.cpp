#include "GroupManager.hpp"
#include "ui/Popups.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/ProfilePage.hpp>
#include <Geode/modify/LevelInfoLayer.hpp>

using namespace geode::prelude;

static CCNode* chatButtonSprite(CircleBaseSize size) {
    CCNode* spr = CircleButtonSprite::createWithSpriteFrameName(
        "accountBtn_messages_001.png", 1.f, CircleBaseColor::Green, size
    );
    if (!spr) spr = ButtonSprite::create("Chats");
    return spr;
}

$on_mod(Loaded) {
    gc::GroupManager::get().init();
}

class $modify(GCMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto menu = this->getChildByID("bottom-menu");
        if (!menu) {
            log::warn("bottom-menu not found, group chat button not added");
            return true;
        }

        auto btn = CCMenuItemExt::createSpriteExtra(chatButtonSprite(CircleBaseSize::MediumAlt), [](auto) {
            gc::ui::GroupListPopup::create()->show();
        });
        btn->setID("group-chats-button"_spr);
        menu->addChild(btn);
        menu->updateLayout();

        return true;
    }
};

class $modify(GCProfilePage, ProfilePage) {
    void loadPageFromUserInfo(GJUserScore* score) {
        ProfilePage::loadPageFromUserInfo(score);

        if (m_ownProfile || !score || score->m_accountID <= 0) return;

        // this gets called again whenever the profile refreshes
        auto menu = static_cast<CCMenu*>(this->getChildByIDRecursive("left-menu"));
        if (!menu || menu->getChildByID("add-to-group-button"_spr)) return;

        auto spr = chatButtonSprite(CircleBaseSize::Small);
        auto btn = CCMenuItemExt::createSpriteExtra(
            spr,
            [accountId = score->m_accountID, username = std::string(score->m_userName)](auto) {
                gc::ui::AddToGroupPopup::create(accountId, username)->show();
            }
        );
        btn->setID("add-to-group-button"_spr);
        menu->addChild(btn);
        menu->updateLayout();
    }
};

class $modify(GCLevelInfoLayer, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;

        int levelId = level->m_levelID.value();
        if (levelId <= 0) return true;

        auto menu = this->getChildByID("left-side-menu");
        if (!menu) return true;

        auto btn = CCMenuItemExt::createSpriteExtra(chatButtonSprite(CircleBaseSize::Medium), [this](auto) {
            gc::ui::ShareLevelPopup::create({
                m_level->m_levelID.value(),
                std::string(m_level->m_levelName),
                std::string(m_level->m_creatorName),
            })->show();
        });
        btn->setID("share-to-group-button"_spr);
        menu->addChild(btn);
        menu->updateLayout();

        return true;
    }
};

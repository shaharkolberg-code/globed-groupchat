#include "Common.hpp"
#include "../GroupManager.hpp"

using namespace geode::prelude;

namespace gc::ui {

ScrollLayer* createList(CCSize size) {
    auto scroll = ScrollLayer::create(size);
    scroll->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(4.f));
    return scroll;
}

void addList(CCNode* parent, ScrollLayer* scroll, CCPoint offset) {
    auto bg = NineSlice::create("square02b_001.png");
    bg->setColor({0, 0, 0});
    bg->setOpacity(70);
    bg->setContentSize(scroll->getContentSize() + CCSize{6.f, 6.f});
    parent->addChildAtPosition(bg, Anchor::Center, offset);

    auto size = scroll->getContentSize();
    parent->addChildAtPosition(scroll, Anchor::Center, offset - CCPoint{size.width / 2, size.height / 2});
}

CCNode* createRow(float width, float height) {
    auto row = CCNode::create();
    row->setContentSize({width, height});
    row->setAnchorPoint({0.5f, 0.5f});

    auto bg = NineSlice::create("square02b_001.png");
    bg->setColor({0, 0, 0});
    bg->setOpacity(60);
    bg->setContentSize({width, height});
    bg->setAnchorPoint({0.f, 0.f});
    row->addChild(bg, -1);

    return row;
}

CCMenuItemSpriteExtra* textButton(
    const char* text, float scale, geode::Function<void(CCMenuItemSpriteExtra*)> callback
) {
    auto spr = ButtonSprite::create(text, "goldFont.fnt", "GJ_button_01.png", 0.8f);
    spr->setScale(scale);
    return CCMenuItemExt::createSpriteExtra(spr, std::move(callback));
}

void showError(std::string_view message) {
    Notification::create(std::string(message), NotificationIcon::Error)->show();
}

void openLevel(int levelId) {
    auto search = GJSearchObject::create(SearchType::Search, std::to_string(levelId));
    CCDirector::get()->pushScene(CCTransitionFade::create(0.5f, LevelBrowserLayer::scene(search)));
}

void report(Result<> result) {
    if (result.isErr()) showError(result.unwrapErr());
}

ObservingPopup::~ObservingPopup() {
    if (m_observer) GroupManager::get().removeObserver(m_observer);
}

void ObservingPopup::observe() {
    m_observer = GroupManager::get().addObserver([this] {
        if (m_refreshQueued) return;
        m_refreshQueued = true;
        this->scheduleOnce(schedule_selector(ObservingPopup::onQueuedRefresh), 0.f);
    });
}

void ObservingPopup::onQueuedRefresh(float) {
    m_refreshQueued = false;
    this->refresh();
}

}

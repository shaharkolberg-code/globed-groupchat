#pragma once

#include <Geode/Geode.hpp>

namespace gc::ui {

geode::ScrollLayer* createList(cocos2d::CCSize size);

/// Adds `scroll` centered at `offset` in a popup's anchor-laid-out main layer.
void addList(cocos2d::CCNode* parent, geode::ScrollLayer* scroll, cocos2d::CCPoint offset);

/// Row with a translucent dark background; add children to it directly.
cocos2d::CCNode* createRow(float width, float height);

CCMenuItemSpriteExtra* textButton(
    const char* text, float scale, geode::Function<void(CCMenuItemSpriteExtra*)> callback
);

void showError(std::string_view message);

/// Opens the level browser searching for this level ID.
void openLevel(int levelId);

/// Runs `result`, showing its error as a notification if it failed.
void report(geode::Result<> result);

/// Base for popups that re-render whenever group state changes.
class ObservingPopup : public geode::Popup {
protected:
    int m_observer = 0;
    bool m_refreshQueued = false;

    ~ObservingPopup();
    /// Calls refresh() on the next frame after any change. Deferred because a
    /// change often comes from a button inside the list that refresh() rebuilds.
    void observe();
    void onQueuedRefresh(float);
    virtual void refresh() = 0;
};

}

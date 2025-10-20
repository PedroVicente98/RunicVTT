#pragma once
#include "NotesManager.h"
#include "imgui.h"
#include "MarkerRenderer.h"
#include <vector>
#include <unordered_map>
#include <memory>

struct ImGuiToaster;

class NoteEditorUI {
public:
    explicit NoteEditorUI(std::shared_ptr<NotesManager> mgr,
                          std::shared_ptr<ImGuiToaster> toaster = nullptr);

    void render(bool* pOpen = nullptr);
    void openNoteTab(const std::string& uuid);
    void setActiveTable(std::optional<std::string> tableName);

    void setVisible(bool v) { visible_ = v; }
    bool isVisible() const { return visible_; }

private:

    std::shared_ptr<NotesManager> mgr_;
    std::shared_ptr<ImGuiToaster> toaster_;
    MarkerRenderer md_;
    bool visible_ = false;
    float leftWidth_ = 260.f;

    std::vector<std::string> openTabs_;  // UUIDs
    int currentTabIndex_ = -1;

    struct TabState {
        std::string editBuffer;
        bool bufferInit = false;
    };
    std::unordered_map<std::string, TabState> tabState_;

    char searchBuf_[128] = {0};

    void renderDirectory_(float height);
    void renderTabsArea_(float width, float height);
    void renderOneTab_(const std::string& uuid, float availW, float availH);

    void actCreateNote_();
    void actSaveNote_(const std::string& uuid);
    void actDeleteNote_(const std::string& uuid);
    void actSaveInboxToLocal_(const std::string& uuid);
    void toggleOpenEditor_(const std::string& uuid);

    void addTabIfMissing_(const std::string& uuid);
    void closeTab_(int tabIndex);
    bool filterMatch_(const Note& n) const;
};

#include "NoteEditorUI.h"
#include "ImGuiToaster.h"
#include <algorithm>

NoteEditorUI::NoteEditorUI(std::shared_ptr<NotesManager> mgr,
                           std::shared_ptr<ImGuiToaster> toaster)
    : mgr_(std::move(mgr)), toaster_(std::move(toaster)) {}

void NoteEditorUI::setActiveTable(std::optional<std::string> tableName) {
    (void)tableName; // reserved for future save-locations UX
}

void NoteEditorUI::render(bool* pOpen) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (!ImGui::Begin("Notes", pOpen, flags)) {
        ImGui::End();
        return;
    }

    const float fullW = ImGui::GetContentRegionAvail().x;
    const float fullH = ImGui::GetContentRegionAvail().y;

    ImGui::BeginChild("##Dir", ImVec2(leftWidth_, fullH), true);
    renderDirectory_(fullH);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::InvisibleButton("##splitter", ImVec2(6, fullH));
    if (ImGui::IsItemActive()) {
        leftWidth_ += ImGui::GetIO().MouseDelta.x;
        leftWidth_ = std::max(180.f, std::min(leftWidth_, fullW - 240.f));
    }
    ImGui::SameLine();

    const float tabsW = std::max(0.f, fullW - leftWidth_ - 6.f);
    ImGui::BeginChild("##Tabs", ImVec2(tabsW, fullH), true);
    renderTabsArea_(tabsW, fullH);
    ImGui::EndChild();

    ImGui::End();
}

void NoteEditorUI::renderDirectory_(float /*height*/) {
    if (ImGui::Button("New")) {
        actCreateNote_();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        mgr_->loadAllFromDisk();
    }
    ImGui::Separator();

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", "Search title/author/text...", searchBuf_, sizeof(searchBuf_));
    ImGui::Separator();

    // Inbox
    if (ImGui::CollapsingHeader("Shared Inbox", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto inbox = mgr_->listInbox();
        if (inbox.empty()) {
            ImGui::TextDisabled("(empty)");
        } else {
            for (auto& n : inbox) {
                if (!n) continue;
                if (!filterMatch_(*n)) continue;

                ImGui::PushID(n->uuid.c_str());
                bool selected = (std::find(openTabs_.begin(), openTabs_.end(), n->uuid) != openTabs_.end());
                std::string label = n->title + "  [from: " + (n->shared_from ? *n->shared_from : "?") + "]";
                if (ImGui::Selectable(label.c_str(), selected)) {
                    addTabIfMissing_(n->uuid);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Save locally")) {
                    actSaveInboxToLocal_(n->uuid);
                }
                ImGui::PopID();
            }
        }
    }

    ImGui::Separator();

    // My Notes
    if (ImGui::CollapsingHeader("My Notes", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto mine = mgr_->listMyNotes();
        if (mine.empty()) {
            ImGui::TextDisabled("(none)");
        } else {
            for (auto& n : mine) {
                if (!n) continue;
                if (!filterMatch_(*n)) continue;

                ImGui::PushID(n->uuid.c_str());
                bool selected = (std::find(openTabs_.begin(), openTabs_.end(), n->uuid) != openTabs_.end());
                std::string label = n->title;
                if (n->dirty) label += " *";
                if (!n->saved_locally) label += " (unsaved)";
                if (ImGui::Selectable(label.c_str(), selected)) {
                    addTabIfMissing_(n->uuid);
                }
                ImGui::PopID();
            }
        }
    }
}

void NoteEditorUI::renderTabsArea_(float width, float height) {
    if (openTabs_.empty()) {
        ImGui::TextDisabled("No notes open. Select a note on the left or create a new one.");
        return;
    }

    if (ImGui::BeginTabBar("##NoteTabs",
                           ImGuiTabBarFlags_AutoSelectNewTabs |
                           ImGuiTabBarFlags_Reorderable)) {
        for (int i = 0; i < (int)openTabs_.size(); ++i) {
            const std::string& uuid = openTabs_[i];
            auto n = mgr_->getNote(uuid);
            if (!n) {
                if (currentTabIndex_ == i) currentTabIndex_ = -1;
                closeTab_(i);
                --i;
                continue;
            }

            std::string tabTitle = n->title.empty() ? n->uuid : n->title;
            if (n->dirty) tabTitle += " *";

            bool open = true;
            if (ImGui::BeginTabItem(tabTitle.c_str(), &open)) {
                currentTabIndex_ = i;
                renderOneTab_(uuid, width, height);
                ImGui::EndTabItem();
            }
            if (!open) {
                closeTab_(i);
                if (currentTabIndex_ >= (int)openTabs_.size())
                    currentTabIndex_ = (int)openTabs_.size() - 1;
                --i;
            }
        }
        ImGui::EndTabBar();
    }
}

void NoteEditorUI::renderOneTab_(const std::string& uuid, float availW, float availH) {
    auto n = mgr_->getNote(uuid);
    if (!n) return;

    if (ImGui::Button(n->open_editor ? "Hide Editor" : "Show Editor")) {
        toggleOpenEditor_(uuid);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        actSaveNote_(uuid);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        actDeleteNote_(uuid);
        return;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", n->saved_locally ? n->file_path.filename().string().c_str() : "(unsaved)");

    ImGui::Separator();

    float editorW = n->open_editor ? (availW * 0.5f) : 0.f;
    float viewerW = n->open_editor ? (availW - editorW - 6.f) : availW;

    if (n->open_editor) {
        ImGui::BeginChild("##editor", ImVec2(editorW, availH - 40), true);
        auto& ts = tabState_[uuid];
        if (!ts.bufferInit) {
            ts.editBuffer = n->markdown_text;
            ts.bufferInit = true;
        }

        if (ImGui::InputTextMultiline("##NoteEdit", &ts.editBuffer, ImVec2(-1, -1),
                                      ImGuiInputTextFlags_AllowTabInput)) {
            // updated in footer
        }
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::InvisibleButton("##split2", ImVec2(6, availH - 40));
        ImGui::SameLine();
    }

    ImGui::BeginChild("##viewer", ImVec2(viewerW, availH - 40), true);
    {
        const auto toMs = NotesManager::toEpochMillis;
        ImGui::TextDisabled("Author: %s", n->author.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("Created: %lld", (long long)toMs(n->creation_ts));
        ImGui::SameLine();
        ImGui::TextDisabled("Updated: %lld", (long long)toMs(n->last_update_ts));
        ImGui::Separator();

        const std::string& md = n->open_editor ? tabState_[uuid].editBuffer : n->markdown_text;
        if (!md.empty()) {
            md_.print(md.c_str(), md.c_str() + md.size());
        } else {
            ImGui::TextDisabled("(empty)");
        }
    }
    ImGui::EndChild();

    if (n->open_editor) {
        auto& ts = tabState_[uuid];
        if (ts.bufferInit && ts.editBuffer != n->markdown_text) {
            mgr_->setContent(uuid, ts.editBuffer);
        }
    }
}

void NoteEditorUI::actCreateNote_() {
    static char titleBuf[128] = {0};
    static char authorBuf[128] = {0};
    ImGui::OpenPopup("Create Note");
    if (ImGui::BeginPopupModal("Create Note", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::InputText("Title", titleBuf, sizeof(titleBuf));
        ImGui::InputText("Author", authorBuf, sizeof(authorBuf));
        if (ImGui::Button("Create")) {
            std::string title = titleBuf[0] ? titleBuf : "Untitled";
            std::string author = authorBuf[0] ? authorBuf : "unknown";
            auto id = mgr_->createNote(title, author);
            addTabIfMissing_(id);
            titleBuf[0] = 0;
            authorBuf[0] = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void NoteEditorUI::actSaveNote_(const std::string& uuid) {
    mgr_->saveNote(uuid);
}

void NoteEditorUI::actDeleteNote_(const std::string& uuid) {
    ImGui::OpenPopup("Delete Note?");
    if (ImGui::BeginPopupModal("Delete Note?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static bool alsoDisk = false;
        ImGui::Checkbox("Also delete from disk", &alsoDisk);
        if (ImGui::Button("Delete")) {
            mgr_->deleteNote(uuid, alsoDisk);
            auto it = std::find(openTabs_.begin(), openTabs_.end(), uuid);
            if (it != openTabs_.end()) {
                int idx = (int)std::distance(openTabs_.begin(), it);
                closeTab_(idx);
            }
            alsoDisk = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void NoteEditorUI::actSaveInboxToLocal_(const std::string& uuid) {
    mgr_->saveInboxToLocal(uuid, std::nullopt);
}

void NoteEditorUI::toggleOpenEditor_(const std::string& uuid) {
    auto n = mgr_->getNote(uuid);
    if (!n) return;
    n->open_editor = !n->open_editor;
    if (n->open_editor) {
        auto& ts = tabState_[uuid];
        ts.bufferInit = false;
    }
}

void NoteEditorUI::addTabIfMissing_(const std::string& uuid) {
    if (std::find(openTabs_.begin(), openTabs_.end(), uuid) == openTabs_.end()) {
        openTabs_.push_back(uuid);
        currentTabIndex_ = (int)openTabs_.size() - 1;
        if (auto n = mgr_->getNote(uuid)) {
            auto& ts = tabState_[uuid];
            ts.editBuffer = n->markdown_text;
            ts.bufferInit = true;
        }
    } else {
        currentTabIndex_ = (int)std::distance(openTabs_.begin(),
                                              std::find(openTabs_.begin(), openTabs_.end(), uuid));
    }
}

void NoteEditorUI::closeTab_(int tabIndex) {
    if (tabIndex < 0 || tabIndex >= (int)openTabs_.size()) return;
    tabState_.erase(openTabs_[tabIndex]);
    openTabs_.erase(openTabs_.begin() + tabIndex);
}

bool NoteEditorUI::filterMatch_(const Note& n) const {
    if (searchBuf_[0] == 0) return true;
    std::string needle = searchBuf_;
    std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
    auto contains = [&](const std::string& hay) {
        std::string low = hay;
        std::transform(low.begin(), low.end(), low.begin(), ::tolower);
        return low.find(needle) != std::string::npos;
    };
    return contains(n.title) || contains(n.author) || contains(n.markdown_text);
}

void NoteEditorUI::openNoteTab(const std::string& uuid) {
    addTabIfMissing_(uuid);
}

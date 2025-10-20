#include "NoteEditorUI.h"
#include "ImGuiToaster.h"
#include <algorithm>

// at top of NoteEditorUI.cpp (or a small header you include here)
static bool InputTextMultilineString(const char* label,
                                     std::string* str,
                                     const ImVec2& size = ImVec2(0, 0),
                                     ImGuiInputTextFlags flags = 0)
{
    IM_ASSERT(str);
    auto cb = [](ImGuiInputTextCallbackData* data) -> int
    {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            auto* s = static_cast<std::string*>(data->UserData);
            s->resize(static_cast<size_t>(data->BufTextLen));
            data->Buf = s->data();
        }
        return 0;
    };
    flags |= ImGuiInputTextFlags_CallbackResize;
    return ImGui::InputTextMultiline(label, str->data(), str->size() + 1, size, flags, cb, str);
}

NoteEditorUI::NoteEditorUI(std::shared_ptr<NotesManager> notes_manager, std::shared_ptr<ImGuiToaster> toaster) :
    notes_manager(std::move(notes_manager)), toaster_(std::move(toaster)) {}

void NoteEditorUI::setActiveTable(std::optional<std::string> tableName)
{
    (void)tableName; // reserved for future save-locations UX
}

void NoteEditorUI::render(bool* pOpen)
{
    if (!visible_)
        return;

    const ImU32 winBg = IM_COL32(28, 28, 30, 255);   // window bg
    const ImU32 childBg = IM_COL32(24, 24, 26, 255); // child panels bg
    ImGui::PushStyleColor(ImGuiCol_WindowBg, winBg);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, childBg);
    ImGui::SetNextWindowSizeConstraints(ImVec2(800, 600), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("Notes", pOpen);

    const float fullW = ImGui::GetContentRegionAvail().x;
    const float fullH = ImGui::GetContentRegionAvail().y;

    ImGui::BeginChild("##Dir", ImVec2(leftWidth_, fullH), true);
    renderDirectory_(fullH);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::InvisibleButton("##splitter", ImVec2(3, fullH));
    if (ImGui::IsItemActive())
    {
        leftWidth_ += ImGui::GetIO().MouseDelta.x;
        leftWidth_ = std::clamp(leftWidth_, 180.0f, fullW - 240.0f);
    }
    ImGui::SameLine();

    const float tabsW = (std::max)(0.f, fullW - leftWidth_ - 6.f);
    ImGui::BeginChild("##Tabs", ImVec2(tabsW, fullH), true);
    renderTabsArea_(tabsW, fullH);
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleColor(2);
}

void NoteEditorUI::renderDirectory_(float /*height*/)
{
    if (ImGui::Button("New"))
    {
        actCreateNote_();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload"))
    {
        notes_manager->loadAllFromDisk();
    }
    ImGui::Separator();

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", "Search title/author/text...", searchBuf_, sizeof(searchBuf_));
    ImGui::Separator();

    // My Notes
    if (ImGui::CollapsingHeader("My Notes", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto mine = notes_manager->listMyNotes();
        if (mine.empty())
        {
            ImGui::TextDisabled("(none)");
        }
        else
        {
            for (auto& n : mine)
            {
                if (!n)
                    continue;
                if (!filterMatch_(*n))
                    continue;

                ImGui::PushID(n->uuid.c_str());
                bool selected = (std::find(openTabs_.begin(), openTabs_.end(), n->uuid) != openTabs_.end());
                std::string label = n->title;
                if (n->dirty)
                    label += " *";
                if (!n->saved_locally)
                    label += " (unsaved)";
                if (ImGui::Selectable(label.c_str(), selected))
                {
                    addTabIfMissing_(n->uuid);
                }
                ImGui::PopID();
            }
        }
    }

    ImGui::Separator();

    // Inbox
    if (ImGui::CollapsingHeader("Shared Inbox", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto inbox = notes_manager->listInbox();
        if (inbox.empty())
        {
            ImGui::TextDisabled("(empty)");
        }
        else
        {
            for (auto& n : inbox)
            {
                if (!n)
                    continue;
                if (!filterMatch_(*n))
                    continue;

                ImGui::PushID(n->uuid.c_str());
                bool selected = (std::find(openTabs_.begin(), openTabs_.end(), n->uuid) != openTabs_.end());
                std::string label = n->title + "  [from: " + (n->shared_from ? *n->shared_from : "?") + "]";
                if (ImGui::Selectable(label.c_str(), selected))
                {
                    addTabIfMissing_(n->uuid);
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Save locally"))
                {
                    actSaveInboxToLocal_(n->uuid);
                }
                ImGui::PopID();
            }
        }
    }
}

void NoteEditorUI::renderTabsArea_(float width, float height)
{
    if (openTabs_.empty())
    {
        ImGui::TextDisabled("No notes open. Select a note on the left or create a new one.");
        return;
    }

    if (ImGui::BeginTabBar("##NoteTabs",
                           ImGuiTabBarFlags_AutoSelectNewTabs |
                               ImGuiTabBarFlags_Reorderable))
    {
        for (int i = 0; i < (int)openTabs_.size(); ++i)
        {
            const std::string& uuid = openTabs_[i];
            auto n = notes_manager->getNote(uuid);
            if (!n)
            {
                if (currentTabIndex_ == i)
                    currentTabIndex_ = -1;
                closeTab_(i);
                --i;
                continue;
            }

            std::string tabTitle = n->title.empty() ? n->uuid : n->title;
            if (n->dirty)
                tabTitle += " *";

            bool open = true;
            if (ImGui::BeginTabItem(tabTitle.c_str(), &open))
            {
                currentTabIndex_ = i;
                renderOneTab_(uuid, width, height);
                ImGui::EndTabItem();
            }
            if (!open)
            {
                closeTab_(i);
                if (currentTabIndex_ >= (int)openTabs_.size())
                    currentTabIndex_ = (int)openTabs_.size() - 1;
                --i;
            }
        }
        ImGui::EndTabBar();
    }
}

void NoteEditorUI::renderOneTab_(const std::string& uuid, float availW, float availH)
{
    auto n = notes_manager->getNote(uuid);
    if (!n)
        return;

    if (ImGui::Button(n->open_editor ? "Hide Editor" : "Show Editor"))
    {
        toggleOpenEditor_(uuid);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        actSaveNote_(uuid);
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete"))
    {
        actDeleteNote_(uuid);
        return;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", n->saved_locally ? n->file_path.filename().string().c_str() : "(unsaved)");

    ImGui::Separator();

    float editorW = n->open_editor ? (availW * 0.5f) : 0.f;
    float viewerW = n->open_editor ? (availW - editorW - 6.f) : availW;

    if (n->open_editor)
    {
        ImGui::BeginChild("##editor", ImVec2(editorW, availH - 40), true);
        auto& ts = tabState_[uuid];
        if (!ts.bufferInit)
        {
            ts.editBuffer = n->markdown_text;
            ts.bufferInit = true;
        }

        //if (ImGui::InputTextMultiline("##NoteEdit", &ts.editBuffer, ImVec2(-1, -1), ImGuiInputTextFlags_AllowTabInput))
        //{
        //    // updated in footer
        //}
        if (InputTextMultilineString("##NoteEdit", &ts.editBuffer, ImVec2(-1, -1), ImGuiInputTextFlags_AllowTabInput))
        {
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

        md_.onOpenExternal = [this](const std::string& url)
        {
            // you can leave default (it already opens browser), or hook a toast:
            toaster_->Push(ImGuiToaster::Level::Info, "Opening: " + url);
        };

        md_.onRoll = [this](const std::string& expr)
        {
            if (toaster_)
                toaster_->Push(ImGuiToaster::Level::Good, "Roll: " + expr);
            // TODO later: send to chat roller
        };

        md_.resolveNoteRef = [this](const std::string& ref) -> std::string
        {
            // Let NotesManager resolve: by title, alias, or short-id → UUID
            return notes_manager->resolveRef(ref); // implement: tries full uuid, short-prefix, title lookup
        };

        md_.onNoteOpen = [this](const std::string& uuid)
        {
            // Open tab by UUID
            openTabByUuid(uuid);
        };

        const std::string& md = n->open_editor ? tabState_[uuid].editBuffer : n->markdown_text;
        if (!md.empty())
        {
            md_.print(md.c_str(), md.c_str() + md.size());
        }
        else
        {
            ImGui::TextDisabled("(empty)");
        }
    }
    ImGui::EndChild();

    if (n->open_editor)
    {
        auto& ts = tabState_[uuid];
        if (ts.bufferInit && ts.editBuffer != n->markdown_text)
        {
            notes_manager->setContent(uuid, ts.editBuffer);
        }
    }
}

void NoteEditorUI::actCreateNote_()
{
    static char titleBuf[128] = {0};
    static char authorBuf[128] = {0};
    ImGui::OpenPopup("Create Note");
    if (ImGui::BeginPopupModal("Create Note", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("Title", titleBuf, sizeof(titleBuf));
        ImGui::InputText("Author", authorBuf, sizeof(authorBuf));
        if (ImGui::Button("Create"))
        {
            std::string title = titleBuf[0] ? titleBuf : "Untitled";
            std::string author = authorBuf[0] ? authorBuf : "unknown";
            auto id = notes_manager->createNote(title, author);
            addTabIfMissing_(id);
            titleBuf[0] = 0;
            authorBuf[0] = 0;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void NoteEditorUI::actSaveNote_(const std::string& uuid)
{
    notes_manager->saveNote(uuid);
}

void NoteEditorUI::actDeleteNote_(const std::string& uuid)
{
    ImGui::OpenPopup("Delete Note?");
    if (ImGui::BeginPopupModal("Delete Note?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static bool alsoDisk = false;
        ImGui::Checkbox("Also delete from disk", &alsoDisk);
        if (ImGui::Button("Delete"))
        {
            notes_manager->deleteNote(uuid, alsoDisk);
            auto it = std::find(openTabs_.begin(), openTabs_.end(), uuid);
            if (it != openTabs_.end())
            {
                int idx = (int)std::distance(openTabs_.begin(), it);
                closeTab_(idx);
            }
            alsoDisk = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void NoteEditorUI::actSaveInboxToLocal_(const std::string& uuid)
{
    notes_manager->saveInboxToLocal(uuid, std::nullopt);
}

void NoteEditorUI::toggleOpenEditor_(const std::string& uuid)
{
    auto n = notes_manager->getNote(uuid);
    if (!n)
        return;
    n->open_editor = !n->open_editor;
    if (n->open_editor)
    {
        auto& ts = tabState_[uuid];
        ts.bufferInit = false;
    }
}

void NoteEditorUI::addTabIfMissing_(const std::string& uuid)
{
    if (std::find(openTabs_.begin(), openTabs_.end(), uuid) == openTabs_.end())
    {
        openTabs_.push_back(uuid);
        currentTabIndex_ = (int)openTabs_.size() - 1;
        if (auto n = notes_manager->getNote(uuid))
        {
            auto& ts = tabState_[uuid];
            ts.editBuffer = n->markdown_text;
            ts.bufferInit = true;
        }
    }
    else
    {
        currentTabIndex_ = (int)std::distance(openTabs_.begin(),
                                              std::find(openTabs_.begin(), openTabs_.end(), uuid));
    }
}

void NoteEditorUI::closeTab_(int tabIndex)
{
    if (tabIndex < 0 || tabIndex >= (int)openTabs_.size())
        return;
    tabState_.erase(openTabs_[tabIndex]);
    openTabs_.erase(openTabs_.begin() + tabIndex);
}

bool NoteEditorUI::filterMatch_(const Note& n) const
{
    if (searchBuf_[0] == 0)
        return true;
    std::string needle = searchBuf_;
    std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
    auto contains = [&](const std::string& hay)
    {
        std::string low = hay;
        std::transform(low.begin(), low.end(), low.begin(), ::tolower);
        return low.find(needle) != std::string::npos;
    };
    return contains(n.title) || contains(n.author) || contains(n.markdown_text);
}

void NoteEditorUI::openNoteTab(const std::string& uuid)
{
    addTabIfMissing_(uuid);
}

bool NoteEditorUI::openTabByUuid(const std::string& uuid)
{
    if (!notes_manager)
        return false;

    // must exist
    auto n = notes_manager->getNote(uuid);
    if (!n)
        return false;

    // focus if already open
    auto it = std::find(openTabs_.begin(), openTabs_.end(), uuid);
    if (it != openTabs_.end())
    {
        currentTabIndex_ = static_cast<int>(std::distance(openTabs_.begin(), it));
        visible_ = true;
        return true;
    }

    // otherwise open a new tab
    addTabIfMissing_(uuid);
    visible_ = true;
    return true;
}

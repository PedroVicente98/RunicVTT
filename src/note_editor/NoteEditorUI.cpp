#include "NoteEditorUI.h"
#include "ImGuiToaster.h"
#include <algorithm>

//static bool InputTextMultilineString(const char* label,
//                                     std::string* str,
//                                     const ImVec2& size = ImVec2(0, 0),
//                                     ImGuiInputTextFlags flags = 0)
//{
//    IM_ASSERT(str);
//    auto cb = [](ImGuiInputTextCallbackData* data) -> int
//    {
//        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
//        {
//            auto* s = static_cast<std::string*>(data->UserData);
//            s->resize(static_cast<size_t>(data->BufTextLen));
//            data->Buf = s->data();
//        }
//        return 0;
//    };
//
//    flags |= ImGuiInputTextFlags_CallbackResize;
//    flags |= ImGuiInputTextFlags_NoHorizontalScroll;
//
//    return ImGui::InputTextMultiline(label, str->data(), str->size() + 1, size, flags, cb, str);
//}

struct InputTextWrapCtx
{
    std::string* buf = nullptr;
    float max_px = 0.f;
};

static int InputTextMultilineWrapCallback(ImGuiInputTextCallbackData* data)
{
    auto* ctx = static_cast<InputTextWrapCtx*>(data->UserData);
    if (!ctx || !ctx->buf)
        return 0;

    // Keep Dear ImGui's string-resize contract
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
        ctx->buf->resize(static_cast<size_t>(data->BufTextLen));
        data->Buf = ctx->buf->data();
        return 0;
    }

    if (data->EventFlag == ImGuiInputTextFlags_CallbackEdit)
    {
        // Current line range [start, end)
        const int len = data->BufTextLen;
        int cur = data->CursorPos;
        int start = cur;
        while (start > 0 && data->Buf[start - 1] != '\n')
            start--;
        int end = cur;
        while (end < len && data->Buf[end] != '\n')
            end++;

        const char* line_start = data->Buf + start;
        const char* line_end = data->Buf + end;

        // If current line fits, done.
        ImVec2 sz = ImGui::CalcTextSize(line_start, line_end, false, FLT_MAX);
        if (sz.x <= ctx->max_px || ctx->max_px <= 0.0f)
            return 0;

        // Find a nice break point (space/tab/hyphen/slash) before cursor
        int break_pos = -1;
        for (int i = cur - 1; i >= start; --i)
        {
            char c = data->Buf[i];
            if (c == ' ' || c == '\t' || c == '-' || c == '/')
            {
                // Would the line up to here fit?
                ImVec2 sz2 = ImGui::CalcTextSize(line_start, data->Buf + i, false, FLT_MAX);
                if (sz2.x <= ctx->max_px)
                {
                    break_pos = i;
                    break;
                }
            }
        }

        if (break_pos >= start)
        {
            // Replace that separator with a newline
            data->DeleteChars(break_pos, 1);
            data->InsertChars(break_pos, "\n");
            // Let Dear ImGui re-evaluate next frame; avoid chaining multiple edits now
            return 0;
        }
        else
        {
            // Force break at cursor if no separator found that fits
            data->InsertChars(cur, "\n");
            data->CursorPos = cur + 1;
            return 0;
        }
    }
    return 0;
}

static bool InputTextMultilineString_HardWrap(const char* label,
                                              std::string* str,
                                              const ImVec2& size,
                                              float max_px_line,
                                              ImGuiInputTextFlags flags = 0)
{
    IM_ASSERT(str);
    if (str->capacity() == 0)
        str->reserve(1);

    InputTextWrapCtx ctx;
    ctx.buf = str;
    ctx.max_px = std::max(0.0f, max_px_line);

    flags |= ImGuiInputTextFlags_CallbackResize;
    flags |= ImGuiInputTextFlags_CallbackEdit;
    flags |= ImGuiInputTextFlags_NoHorizontalScroll;

    return ImGui::InputTextMultiline(label,
                                     str->data(),
                                     str->capacity() + 1, // bind to capacity; resize callback will keep this valid
                                     size,
                                     flags,
                                     InputTextMultilineWrapCallback,
                                     &ctx);
}

NoteEditorUI::NoteEditorUI(std::shared_ptr<NotesManager> notes_manager, std::shared_ptr<ImGuiToaster> toaster) :
    notes_manager(std::move(notes_manager)), toaster_(std::move(toaster)) {}

void NoteEditorUI::setActiveTable(std::optional<std::string> tableName)
{
    (void)tableName; // reserved for future save-locations UX
}

void NoteEditorUI::render()
{
    if (!visible_)
        return;

    const ImU32 winBg = IM_COL32(16, 16, 18, 255);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, winBg);
    ImGui::SetNextWindowSizeConstraints(ImVec2(800, 600), ImVec2(FLT_MAX, FLT_MAX));
    bool open = true;
    ImGui::Begin("Notes", &open);

    const float fullW = ImGui::GetContentRegionAvail().x;
    const float fullH = ImGui::GetContentRegionAvail().y;

    ImGui::BeginChild("##Dir", ImVec2(leftWidth_, fullH), true);
    renderDirectory_(fullH);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::InvisibleButton("##splitter", ImVec2(6, fullH));
    if (ImGui::IsItemActive())
    {
        leftWidth_ += ImGui::GetIO().MouseDelta.x;
        leftWidth_ = std::clamp(leftWidth_, 180.0f, fullW - 240.0f);
    }
    ImGui::SameLine();

    // Let ImGui compute exact remaining width to avoid h-scroll.
    ImGui::BeginChild("##Tabs", ImVec2(0.f, fullH), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    renderTabsArea_(/*unused*/ 0.f, fullH);
    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleColor(1); // <-- only 1 was pushed

    if (!open)
        visible_ = false;

    // ---- CREATE NOTE MODAL ----
    if (showCreatePopup_)
        ImGui::OpenPopup("Create Note"); // one-shot

    bool createOpen = true;
    if (ImGui::BeginPopupModal("Create Note", &createOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::InputText("Title", createTitle_, sizeof(createTitle_));
        ImGui::InputText("Author", createAuthor_, sizeof(createAuthor_));

        if (ImGui::Button("Create"))
        {
            std::string title = createTitle_[0] ? createTitle_ : "Untitled";
            std::string author = createAuthor_[0] ? createAuthor_ : "unknown";
            auto id = notes_manager->createNote(title, author);
            openOrFocusTab(id);
            createTitle_[0] = 0;
            createAuthor_[0] = 0;
            showCreatePopup_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            showCreatePopup_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    else
    {
        // If not visible anymore (closed via ESC or X), clear flag
        showCreatePopup_ = false;
    }

    // ---- DELETE NOTE MODAL ----
    if (showDeletePopup_)
        ImGui::OpenPopup("Delete Note?");

    bool delOpen = true;
    if (ImGui::BeginPopupModal("Delete Note?", &delOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        static bool alsoDisk = false;
        ImGui::Checkbox("Also delete from disk", &alsoDisk);

        if (ImGui::Button("Delete"))
        {
            if (!pendingDeleteUuid_.empty())
            {
                notes_manager->deleteNote(pendingDeleteUuid_, alsoDisk);
                auto it = std::find(openTabs_.begin(), openTabs_.end(), pendingDeleteUuid_);
                if (it != openTabs_.end())
                {
                    int idx = (int)std::distance(openTabs_.begin(), it);
                    closeTab_(idx);
                }
            }
            pendingDeleteUuid_.clear();
            alsoDisk = false;
            showDeletePopup_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            pendingDeleteUuid_.clear();
            showDeletePopup_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
    else
    {
        // ESC or close -> reset state
        if (!delOpen)
        {
            pendingDeleteUuid_.clear();
            showDeletePopup_ = false;
        }
    }
}
void NoteEditorUI::renderDirectory_(float /*height*/)
{
    if (ImGui::Button("New"))
    {
        showCreatePopup_ = true;
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
                if (!n || !filterMatch_(*n))
                    continue;
                ImGui::PushID(n->uuid.c_str());

                const bool selected = (std::find(openTabs_.begin(), openTabs_.end(), n->uuid) != openTabs_.end());
                const std::string label = n->title.empty() ? n->uuid : n->title; // ← no '*' or '(unsaved)'
                if (ImGui::Selectable(label.c_str(), selected))
                    openOrFocusTab(n->uuid);

                ImGui::PopID();
            }
        }
    }

    ImGui::Separator();

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
                if (!n || !filterMatch_(*n))
                    continue;
                ImGui::PushID(n->uuid.c_str());

                const bool selected = (std::find(openTabs_.begin(), openTabs_.end(), n->uuid) != openTabs_.end());
                const std::string label = (n->title.empty() ? n->uuid : n->title) + "  [from: " + (n->shared_from ? *n->shared_from : "?") + "]";
                if (ImGui::Selectable(label.c_str(), selected))
                    openOrFocusTab(n->uuid);

                ImGui::SameLine();
                if (ImGui::SmallButton("Save locally"))
                    actSaveInboxToLocal_(n->uuid);

                ImGui::PopID();
            }
        }
    }
}

void NoteEditorUI::renderTabsArea_(float /*width*/, float /*height*/)
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

            // Visible label stays constant; ID is after '###'
            const std::string visible = n->title.empty() ? n->uuid : n->title;
            const std::string label = visible + "###" + uuid;

            ImGuiTabItemFlags tif = 0;
            if (n->dirty)
                tif |= ImGuiTabItemFlags_UnsavedDocument;

            bool open = true;
            if (ImGui::BeginTabItem(label.c_str(), &open, tif))
            {
                currentTabIndex_ = i;

                const float availW = ImGui::GetContentRegionAvail().x;
                const float availH = ImGui::GetContentRegionAvail().y;
                renderOneTab_(uuid, availW, availH);

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

void NoteEditorUI::renderOneTab_(const std::string& uuid, float availW, float /*availH*/)
{
    auto n = notes_manager->getNote(uuid);
    if (!n)
        return;

    ImGuiIO& io = ImGui::GetIO();
    const bool windowFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_RootAndChildWindows);
    if (windowFocused && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
        actSaveNote_(uuid);

    if (ImGui::Button(n->open_editor ? "Hide Editor" : "Show Editor"))
        toggleOpenEditor_(uuid);
    ImGui::SameLine();
    if (ImGui::Button("Save"))
        actSaveNote_(uuid);
    ImGui::SameLine();
    if (ImGui::Button("Delete"))
    {
        pendingDeleteUuid_ = uuid;
        showDeletePopup_ = true;
        return;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", n->saved_locally ? n->file_path.filename().string().c_str() : "(unsaved)");
    ImGui::Separator();

    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float splitterW = 6.f;

    float editorW = 0.f, viewerW = 0.f;
    if (n->open_editor)
    {
        const float shared = availW - splitterW - spacing;
        editorW = floorf(shared * 0.5f);
        viewerW = shared - editorW; // exact remainder => no 1px overflow
    }
    else
    {
        viewerW = availW;
    }

    // --- Editor ---
    if (n->open_editor)
    {
        ImGui::BeginChild("##editor", ImVec2(editorW, 0.f), true); // 0 height => fill
        auto& ts = tabState_[uuid];
        if (!ts.bufferInit)
        {
            ts.editBuffer = n->markdown_text;
            ts.bufferInit = true;
        }

        ImVec2 editorSize = ImGui::GetContentRegionAvail();
        float wrap_px = editorSize.x - ImGui::GetStyle().FramePadding.x * 2.0f;

        // hard-wrap (inserts '\n' when exceeding width)
        InputTextMultilineString_HardWrap("##NoteEdit",
                                          &ts.editBuffer,
                                          editorSize,
                                          wrap_px,
                                          ImGuiInputTextFlags_AllowTabInput);
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::InvisibleButton("##split2", ImVec2(splitterW, 0.f));
        ImGui::SameLine();
    }

    // --- Viewer ---
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f)); // padding to avoid clipped glyphs
    ImGui::BeginChild("##viewer", ImVec2(viewerW, 0.f), true);
    {
        const auto toMs = NotesManager::toEpochMillis;
        ImGui::TextDisabled("Author: %s", n->author.c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("Created: %lld", (long long)toMs(n->creation_ts));
        ImGui::SameLine();
        ImGui::TextDisabled("Updated: %lld", (long long)toMs(n->last_update_ts));
        ImGui::Separator();

        // only set handlers we need; leave external open to MarkdownRenderer default
        md_.onRoll = [this](const std::string& expr)
        {
            if (chat_manager)
                chat_manager->tryHandleSlashCommand(chat_manager->generalGroupId_, "/roll " + expr);
            //if (toaster_)
            //toaster_->Push(ImGuiToaster::Level::Good, "Roll: " + expr);
        };
        md_.resolveNoteRef = [this](const std::string& ref) -> std::string
        {
            return notes_manager->resolveRef(ref);
        };
        md_.onNoteOpen = [this](const std::string& id)
        { openTabByUuid(id); };

        const std::string& md = n->open_editor ? tabState_[uuid].editBuffer : n->markdown_text;

        ImGui::PushTextWrapPos(0.f); // wrap md to viewer width (safety; imgui_md usually wraps, this enforces)
        if (!md.empty())
            md_.print(md.c_str(), md.c_str() + md.size());
        else
            ImGui::TextDisabled("(empty)");
        ImGui::PopTextWrapPos();
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    // Propagate editor changes to manager
    if (n->open_editor)
    {
        auto& ts = tabState_[uuid];
        if (ts.bufferInit && ts.editBuffer != n->markdown_text)
            notes_manager->setContent(uuid, ts.editBuffer);
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
            openOrFocusTab(id);
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

void NoteEditorUI::openOrFocusTab(const std::string& uuid)
{
    auto it = std::find(openTabs_.begin(), openTabs_.end(), uuid);
    if (it == openTabs_.end())
    {
        addTabIfMissing_(uuid); // opens and selects
    }
    else
    {
        currentTabIndex_ = (int)std::distance(openTabs_.begin(), it);
        // ImGui will focus the item with BeginTabItem anyway; this ensures index is correct
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
    openOrFocusTab(uuid);
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
    openOrFocusTab(uuid);
    visible_ = true;
    return true;
}

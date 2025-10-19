#include "NoteEditorUI.h"

NoteEditorUI::NoteEditorUI(std::shared_ptr<flecs::world> world, std::shared_ptr<NoteManager> noteManager) :
    m_world(world), m_noteManager(noteManager)
{
}

void NoteEditorUI::render()
{
    float windowHeight = ImGui::GetContentRegionAvail().y;
    float windowWidth = ImGui::GetContentRegionAvail().x;

    ImGui::Begin("Note Manager", nullptr, ImGuiWindowFlags_NoCollapse);

    renderDirectoryPanel(windowHeight, m_leftPanelWidth);

    ImGui::SameLine();
    ImGui::InvisibleButton("splitter1", ImVec2(5, windowHeight));
    if (ImGui::IsItemActive())
    {
        m_leftPanelWidth += ImGui::GetIO().MouseDelta.x;
    }
    ImGui::SameLine();

    float tabsWidth = windowWidth - m_leftPanelWidth - 10;
    ImGui::BeginChild("TabsArea", ImVec2(tabsWidth, windowHeight), true);
    renderNoteTabs(tabsWidth, windowHeight);
    ImGui::EndChild();

    ImGui::End();
}

void NoteEditorUI::renderNoteTabs(float availableWidth, float height)
{
    if (ImGui::BeginTabBar("NoteTabs", ImGuiTabBarFlags_Reorderable))
    {
        m_world->each<NoteComponent>([&](flecs::entity e, NoteComponent& note)
                                     {
            if (note.selected) {
                // Tab with close button
                bool open = true;
                std::string label = note.title + "###" + note.uuid;

                if (ImGui::BeginTabItem(label.c_str(), &open)) {
                    renderNoteTab(note); // Editor + markdown preview
                    ImGui::EndTabItem();
                }

                if (!open) {
                    note.selected = false;
                }
            } });

        ImGui::EndTabBar();
    }
}

void NoteEditorUI::renderNoteTab(NoteComponent& note)
{
    float height = ImGui::GetContentRegionAvail().y;

    float editorWidth = note.open_editor ? ImGui::GetContentRegionAvail().x * 0.5f : 0.0f;
    float viewerWidth = note.open_editor ? ImGui::GetContentRegionAvail().x * 0.5f : ImGui::GetContentRegionAvail().x;

    if (note.open_editor)
    {
        ImGui::BeginChild("Editor", ImVec2(editorWidth, height), true);
        static std::vector<char> buffer(8192);
        if (note.markdown_text.size() + 1 > buffer.size())
            buffer.resize(note.markdown_text.size() + 256);
        strcpy(buffer.data(), note.markdown_text.c_str());

        if (ImGui::InputTextMultiline("##Editor", buffer.data(), buffer.size(), ImVec2(-1, height - 40)))
        {
            note.markdown_text = buffer.data();
        }

        if (ImGui::Button("Hide Editor"))
        {
            note.open_editor = false;
        }

        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::InvisibleButton("splitter2", ImVec2(5, height));
        ImGui::SameLine();
    }

    ImGui::BeginChild("MarkdownPreview", ImVec2(viewerWidth, height), true);
    if (!note.markdown_text.empty())
    {
        m_renderer.print(note.markdown_text.c_str(), note.markdown_text.c_str() + note.markdown_text.size());
    }

    if (!note.open_editor)
    {
        if (ImGui::Button("Show Editor"))
        {
            note.open_editor = true;
        }
    }

    ImGui::EndChild();
}

void NoteEditorUI::renderDirectoryPanel(float height, float& leftWidth)
{
    ImGui::BeginChild("Directory", ImVec2(leftWidth, height), true);

    ImGui::Text("📓 My Notes");
    ImGui::Separator();

    if (ImGui::Button("+ New Note"))
    {
        NoteComponent note = m_noteManager->createNewNote("local");
        m_noteManager->saveNoteToDisk(note);

        // Deselect others
        m_world->each<NoteComponent>([](flecs::entity, NoteComponent& n)
                                     { n.selected = false; });

        // Auto-select the new note
        flecs::entity new_note = m_world->lookup(note.uuid.c_str());
        if (new_note.is_alive())
        {
            new_note.get_mut<NoteComponent>()->selected = true;
        }
    }

    // Render local notes
    m_world->each<NoteComponent>([&](flecs::entity e, NoteComponent& note)
                                 {
        if (note.shared) return;

        if (ImGui::Selectable(note.title.c_str(), note.selected)) {
            note.selected = !note.selected;
        } });

    ImGui::Spacing();
    ImGui::Text("🌐 Shared with Me");
    ImGui::Separator();

    m_world->each<NoteComponent>([&](flecs::entity e, NoteComponent& note)
                                 {
        if (!note.shared) return;

        std::string label = note.title + " (@" + note.shared_from + ")";
        if (ImGui::Selectable(label.c_str(), note.selected)) {
            note.selected = !note.selected;
        }

        if (!note.saved_locally) {
            ImGui::SameLine();
            std::string saveBtnID = "##SaveBtn_" + note.uuid;
            if (ImGui::SmallButton(("💾 Save" + saveBtnID).c_str())) {
                m_noteManager->saveNoteToDisk(note);
                note.saved_locally = true;
                note.shared = false; // Move it to "My Notes"
            }
        } });

    ImGui::EndChild();
}

//imgui_md renderer--------------------------------------------------------------------

extern ImFont* g_font_regular = nullptr;
extern ImFont* g_font_bold = nullptr;
extern ImFont* g_font_bold_large = nullptr;
extern ImTextureID g_texture1 = nullptr;

ImFont* NoteEditorUI::MarkdownRenderer::get_font() const
{
    // Default fallback
    ImFont* default_font = ImGui::GetFont();

    if (m_is_table_header && g_font_bold)
        return g_font_bold;

    switch (m_hlevel)
    {
        case 0:
            return m_is_strong && g_font_bold ? g_font_bold : (g_font_regular ? g_font_regular : default_font);
        case 1:
            return g_font_bold_large ? g_font_bold_large : default_font;
        default:
            return g_font_bold ? g_font_bold : default_font;
    }
}

bool NoteEditorUI::MarkdownRenderer::get_image(image_info& nfo) const
{
    if (!g_texture1)
        return false;

    nfo.texture_id = g_texture1;
    nfo.size = {40, 20}; // Placeholder size
    nfo.uv0 = {0, 0};
    nfo.uv1 = {1, 1};
    nfo.col_tint = {1, 1, 1, 1};
    nfo.col_border = {0, 0, 0, 0};
    return true;
}

void NoteEditorUI::MarkdownRenderer::open_url() const
{
#if defined(_WIN32)
    ShellExecuteA(NULL, "open", m_href.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    std::string command = "open " + m_href;
    system(command.c_str());
#elif defined(__linux__)
    std::string command = "xdg-open " + m_href;
    system(command.c_str());
#else
    // Unsupported platform
    std::cerr << "Opening URLs not supported on this platform.\n";
#endif
}

void NoteEditorUI::MarkdownRenderer::html_div(const std::string& dclass, bool e)
{
    if (dclass == "red")
    {
        if (e)
        {
            m_table_border = false;
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
        }
        else
        {
            ImGui::PopStyleColor();
            m_table_border = true;
        }
    }
}

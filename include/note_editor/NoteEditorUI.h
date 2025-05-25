#pragma once
#include "flecs.h"
#include "NoteManager.h"
#include "NoteSyncSystem.h"
#include "Components.h"
#include "imgui.h"
#include "imgui_md.h"
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif


class NoteEditorUI {
public:
    NoteEditorUI(std::shared_ptr<flecs::world> world, std::shared_ptr<NoteManager> noteManager);
    NoteEditorUI(std::shared_ptr<flecs::world> world, std::shared_ptr<NoteManager> noteManager, std::shared_ptr<NoteSyncSystem> syncSystem);

    void render();

private:
    void renderDirectoryPanel(float height, float& leftWidth);
    void renderNoteTabs(float availableWidth, float height);
    void renderNoteTab(NoteComponent& note);

    struct MarkdownRenderer : public imgui_md {
        ImFont* get_font() const override;
        void open_url() const override;
        bool get_image(image_info& nfo) const override;
        void html_div(const std::string& dclass, bool e) override;
    };

    MarkdownRenderer m_renderer;

    std::shared_ptr<flecs::world> m_world;
    std::shared_ptr<NoteManager> m_noteManager;
    std::shared_ptr<NoteSyncSystem> m_syncSystem = nullptr;

    float m_leftPanelWidth = 200.0f;
};
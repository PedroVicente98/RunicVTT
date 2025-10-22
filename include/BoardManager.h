#pragma once
#include <vector>
#include "Renderer.h"
#include "Texture.h"
#include "Shader.h"
#include "VertexArray.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "flecs.h"
#include "IdentityManager.h"
#include "Components.h"
#include "DirectoryWindow.h"

class NetworkManager;

struct BoardImageData
{
    GLuint textureID = 0;
    glm::vec2 size{};
    std::string path; // can be empty for memory loads
    BoardImageData() = default;
    BoardImageData(GLuint id, glm::vec2 s, std::string p) :
        textureID(id), size(s), path(std::move(p)) {}
};

class Camera
{
public:
    Camera() :
        position(0.0f, 0.0f), zoom_level(1.0f), fbo_dimensions(0, 0) {} // Set initial zoom to 1.0f for no scaling by default

    void pan(glm::vec2 delta)
    {
        position += delta;
    }

    void zoom(float factor, glm::vec2 mouse_world_pos_before_zoom)
    {
        float old_zoom_level = zoom_level;
        zoom_level *= factor;
        zoom_level = glm::clamp(zoom_level, 0.05f, 20.0f); // Example limits
        if (zoom_level == old_zoom_level)
        {
            return;
        }
        float zoom_ratio = zoom_level / old_zoom_level;
        position = mouse_world_pos_before_zoom - (mouse_world_pos_before_zoom - position) / zoom_ratio;
    }

    void setPosition(glm::vec2 newPosition)
    {
        position = newPosition;
    }

    glm::vec2 getPosition() const
    {
        return position;
    }

    void setZoom(float newZoomLevel)
    {
        zoom_level = newZoomLevel;
        zoom_level = glm::clamp(zoom_level, 0.1f, 10.0f);
    }

    float getZoom() const
    {
        return zoom_level;
    }

    glm::mat4 getViewMatrix() const
    {
        return glm::translate(glm::mat4(1.0f), glm::vec3(-position.x, -position.y, 0.0f));
    }

    glm::mat4 getProjectionMatrix() const
    {
        float half_width = (fbo_dimensions.x / 2.0f) / zoom_level;
        float half_height = (fbo_dimensions.y / 2.0f) / zoom_level;
        return glm::ortho(-half_width, half_width, half_height, -half_height, -1.0f, 1.0f);
    }

    glm::vec2 worldToScreenPosition(glm::vec2 world_position) const
    {
        glm::vec4 world_homogeneous = glm::vec4(world_position.x, world_position.y, 0.0f, 1.0f);
        /*glm::vec4 camera_coords = getViewMatrix() * world_homogeneous;
        glm::vec4 clip_coords = getProjectionMatrix() * camera_coords;*/

        glm::mat4 pv_matrix = getProjectionMatrix() * getViewMatrix(); // Combine PV matrix
        glm::vec4 clip_coords = pv_matrix * world_homogeneous;

        glm::vec2 ndc;
        if (clip_coords.w != 0.0f)
        { // Avoid division by zero
            ndc.x = clip_coords.x / clip_coords.w;
            ndc.y = clip_coords.y / clip_coords.w;
        }
        else
        {
            return glm::vec2(NAN, NAN); // Indicate invalid position
        }
        glm::vec2 fbo_pixel_top_left_origin;
        fbo_pixel_top_left_origin.x = (ndc.x + 1.0f) * 0.5f * fbo_dimensions.x;
        fbo_pixel_top_left_origin.y = (1.0f - ndc.y) * 0.5f * fbo_dimensions.y;

        return fbo_pixel_top_left_origin;
    }

    glm::vec2 fboToNdcPos(glm::vec2 fbo_pixel_top_left_origin) const
    {
        glm::vec2 ndc;
        ndc.x = (2.0f * fbo_pixel_top_left_origin.x) / fbo_dimensions.x - 1.0f;
        ndc.y = 1.0f - (2.0f * fbo_pixel_top_left_origin.y) / fbo_dimensions.y;
        return ndc;
    }

    glm::vec2 screenToWorldPosition(glm::vec2 fbo_pixel_top_left_origin) const
    {

        glm::vec2 ndc = fboToNdcPos(fbo_pixel_top_left_origin);
        glm::vec4 ndc_homogeneous = glm::vec4(ndc.x, ndc.y, 0.0f, 1.0f);
        glm::mat4 inverse_pv_matrix = glm::inverse(getProjectionMatrix() * getViewMatrix());
        glm::vec4 world_homogeneous = inverse_pv_matrix * ndc_homogeneous;
        glm::vec2 world_position;
        if (world_homogeneous.w != 0.0f)
        {
            world_position.x = world_homogeneous.x / world_homogeneous.w;
            world_position.y = world_homogeneous.y / world_homogeneous.w;
        }
        else
        {
            return glm::vec2(NAN, NAN); // Indicate invalid result
        }

        return world_position;
    }

    void setFboDimensions(glm::vec2 dims)
    {
        fbo_dimensions = dims;
    }

private:
    glm::vec2 position;       // 2D position of the camera (X, Y)
    float zoom_level;         // Zoom level, where 1.0f means no zoom, > 1.0f means zoom in, < 1.0f means zoom out
    glm::vec2 fbo_dimensions; // current_fbo_dimension
};

enum class Tool
{
    MOVE,
    FOG,
    MARKER,
    SELECT
};

//MOVE - Move Camera and Markers;
//FOG - Create FOG and TOGGLE VISIBILITY
//MARKER - Toggle Marker Visibility? MIGHT NOT BE NECESSARY
//SELECT - Select Marker/Fog, opening an edit window(change size, visibility, and delete)

class BoardManager : std::enable_shared_from_this<BoardManager>
{
public:
    BoardManager(flecs::world ecs, std::weak_ptr<NetworkManager> network_manager, std::shared_ptr<IdentityManager> identity_manager, std::shared_ptr<DirectoryWindow> map_directory, std::shared_ptr<DirectoryWindow> marker_directory);
    ~BoardManager();

    void renderBoard(VertexArray& va, IndexBuffer& ib, Shader& shader, Shader& grid_shader, Renderer& renderer); // Render board elements (map, markers, fog)
    void renderToolbar(const ImVec2& window_position);
    void resetCamera();

    // Marker interactions
    flecs::entity createMarker(const std::string& imageFilePath, GLuint textureId, glm::vec2 position, glm::vec2 size);
    void deleteMarker(flecs::entity markerEntity);
    void handleMarkerDragging(glm::vec2 mousePos);
    bool isMouseOverMarker(glm::vec2 mousePos);
    bool canMoveMarker(const MarkerComponent* mc, flecs::entity markerEnt) const;

    glm::vec2 computeMarkerDrawSize_ARFit(const TextureComponent& tex, float basePx, float scale);
    // Fog of War interactions
    flecs::entity createFogOfWar(glm::vec2 startPos, glm::vec2 size);
    void deleteFogOfWar(flecs::entity fogEntity);
    void handleFogCreation(glm::vec2 end_world_position);

    // Camera manipulation
    void panBoard(glm::vec2 currentMousePos);
    //void zoomBoard(float zoomFactor);

    // Toolbar tool management
    Tool getCurrentTool() const;
    void setCurrentTool(Tool newTool);

    bool isBoardActive();
    flecs::entity createBoard(std::string board_name, std::string map_image_path, GLuint texture_id, glm::vec2 size);

    void closeBoard();
    void setActiveBoard(flecs::entity board_entity);
    bool isEditWindowOpen() const;
    void renderEditWindow();
    void renderCameraWindow();
    void startMouseDrag(glm::vec2 mousePos, bool draggingMarker);
    void endMouseDrag();
    glm::vec2 getMouseStartPosition() const;
    bool isPanning();
    bool isDraggingMarker(bool local_drag_only = true);
    flecs::entity getEntityAtMousePosition(glm::vec2 mouse_position);

    flecs::entity findBoardById(uint64_t boardId);
    bool shouldSendMarkerMove(uint64_t markerId) const;
    // Generates a unique 64-bit ID
    uint64_t generateUniqueId();
    // Finds an entity by its Identifier component with the specified ID
    flecs::entity findEntityById(uint64_t target_id);

    //Grid
    glm::vec2 snapToGrid(glm::vec2 raw_world_pos);
    void renderGridWindow();
    //Network
    void sendGameState();
    void sendEntityUpdate(flecs::entity entity /*, MessageType message_type*/);

    //flecs::entity deserializeBoard(const std::vector<unsigned char>& buffer, size_t& offset);
    //void serializeBoard(flecs::entity board, std::vector<unsigned char>& buffer);
    flecs::entity getActiveBoard() const;
    void loadActiveBoard(const std::string& filePath);
    void saveActiveBoard(const std::string& filePath);
    void saveActiveBoard(std::filesystem::path& filePath);

    std::string board_name;
    std::shared_ptr<DirectoryWindow> map_directory;
    std::shared_ptr<DirectoryWindow> marker_directory;
    Camera camera;

    bool isCreatingFog() const
    {
        return is_creating_fog;
    };
    bool getShowEditWindow() const
    {
        return showEditWindow;
    };
    void setShowEditWindow(bool show, flecs::entity edit_entity = flecs::entity())
    {
        if (showEditWindow && edit_window_entity.is_valid())
            return;
        is_edit_hovered = show;
        showEditWindow = show;
        edit_window_entity = edit_entity;
    };

    bool getIsNonMapWindowHovered() const
    {
        return is_non_map_window_hovered;
    }
    void setIsNonMapWindowHovered(bool is_non_map_window_hovered)
    {
        this->is_non_map_window_hovered = is_non_map_window_hovered;
    }

    BoardImageData LoadTextureFromMemory(const unsigned char* bytes, size_t sizeBytes);

    void killIfMouseUp(bool isMouseDown);
    void resnapAllMarkersToNearest(const Grid& grid);

    //void replaceOwnerUsernameEverywhere(const std::string& oldUsername,
    //                                    const std::string& newUsername);
    void onUsernameChanged(const std::string& uniqueId, const std::string& newUsername);

private:
    bool is_non_map_window_hovered = false;
    bool is_camera_hovered = false;
    bool is_grid_hovered = false;
    bool is_edit_hovered = false;

    bool showEditWindow = false;
    bool showGridSettings = false;
    bool showCameraSettings = false;
    float markerBasePx = 50.0f;
    flecs::entity edit_window_entity = flecs::entity();
    std::weak_ptr<NetworkManager> network_manager;
    std::shared_ptr<IdentityManager> identity_manager;
    //glm::vec2 mouseStartPos;

    glm::vec2 mouse_start_screen_pos;
    glm::vec2 mouse_start_world_pos;
    glm::vec2 mouse_current_world_pos;

    flecs::world ecs;
    flecs::entity active_board = flecs::entity();
    flecs::entity grid_entity = flecs::entity();

    bool is_creating_fog = false;
    Tool currentTool; // Active tool for interaction
};
/*ENTITY MANAGER

// BoardManager.h (add these)
bool showEntityManager = false;              // similar to showCameraSettings
flecs::entity_t entityMgrSelectedId = 0;     // current selection

//V2
void BoardManager::renderEntityManagerWindow()
{
    if (!showEntityManager)
    {
        setIsNonMapWindowHovered(false);
        return;
    }

    // place near mouse on first open (same as your camera window)
    ImVec2 mousePos = ImGui::GetMousePos();
    ImGui::SetNextWindowPos(ImVec2(mousePos.x, mousePos.y + ImGui::GetFrameHeightWithSpacing()), ImGuiCond_Appearing);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.12f, 0.16f, 1.0f));
    ImGui::Begin("Entity Manager", &showEntityManager,
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

    // important: disable board panning while on this window
    setIsNonMapWindowHovered(ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup));

    if (!isBoardActive())
    {
        ImGui::TextUnformatted("No active board.");
        ImGui::End();
        ImGui::PopStyleColor();
        return;
    }

    // filter tabs
    static int filterTab = 0; // 0=All, 1=Markers, 2=Fog
    ImGui::TextUnformatted("Show:");
    ImGui::SameLine();
    ImGui::RadioButton("All", &filterTab, 0); ImGui::SameLine();
    ImGui::RadioButton("Markers", &filterTab, 1); ImGui::SameLine();
    ImGui::RadioButton("Fog", &filterTab, 2);

    ImGui::Separator();

    // left: table of entities with inline controls
    ImGui::BeginChild("entity_list", ImVec2(600, 300), true, ImGuiWindowFlags_HorizontalScrollbar);

    if (ImGui::BeginTable("EntityTable", 5,
                          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Entity");
        ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("- Size",  ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("+ Size",  ImGuiTableColumnFlags_WidthFixed, 70.0f);
        ImGui::TableSetupColumn("Delete",  ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableHeadersRow();

        // collect candidates
        struct Row { flecs::entity e; bool isMarker; uint64_t id; };
        std::vector<Row> rows; rows.reserve(64);

        ecs.defer_begin();
        active_board.children([&](flecs::entity child)
        {
            const bool isMarker = child.has<MarkerComponent>();
            const bool isFog    = child.has<FogOfWar>();
            if (!isMarker && !isFog) return;

            if (filterTab == 1 && !isMarker) return;
            if (filterTab == 2 && !isFog)    return;

            uint64_t id = 0;
            if (child.has<Identifier>()) id = child.get<Identifier>()->id;
            rows.push_back(Row{child, isMarker, id});
        });
        ecs.defer_end();

        // simple stable sort: markers first, then by id
        std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b){
            if (a.isMarker != b.isMarker) return a.isMarker > b.isMarker;
            return a.id < b.id;
        });

        // render rows
        for (auto& r : rows)
        {
            ImGui::TableNextRow();
            ImGui::PushID((int)r.e.id());

            // col 0: label + selection (clicking the label selects)
            ImGui::TableSetColumnIndex(0);
            const bool selected = (entityMgrSelectedId == r.e.id());
            const std::string label = (r.isMarker ? "Marker_" : "Fog_") + std::to_string(r.id);
            if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
            {
                entityMgrSelectedId = r.e.id();
            }

            // col 1: visibility checkbox
            ImGui::TableSetColumnIndex(1);
            if (r.e.has<Visibility>())
            {
                auto v = r.e.get<Visibility>();
                bool vis = v->isVisible;
                if (ImGui::Checkbox("##vis", &vis))
                {
                    if (auto vm = r.e.get_mut<Visibility>())
                        vm->isVisible = vis;
                }
            }
            else
            {
                ImGui::BeginDisabled();
                bool dummy = true; ImGui::Checkbox("##vis_d", &dummy);
                ImGui::EndDisabled();
            }

            // col 2: - Size
            ImGui::TableSetColumnIndex(2);
            if (r.e.has<Size>())
            {
                if (ImGui::SmallButton("-"))
                {
                    if (auto s = r.e.get_mut<Size>())
                    {
                        s->width  *= 0.90f;
                        s->height *= 0.90f;
                    }
                }
            }
            else
            {
                ImGui::BeginDisabled(); ImGui::SmallButton("-"); ImGui::EndDisabled();
            }

            // col 3: + Size
            ImGui::TableSetColumnIndex(3);
            if (r.e.has<Size>())
            {
                if (ImGui::SmallButton("+"))
                {
                    if (auto s = r.e.get_mut<Size>())
                    {
                        s->width  *= 1.10f;
                        s->height *= 1.10f;
                    }
                }
            }
            else
            {
                ImGui::BeginDisabled(); ImGui::SmallButton("+"); ImGui::EndDisabled();
            }

            // col 4: Delete (with per-row confirmation)
            ImGui::TableSetColumnIndex(4);
            if (ImGui::SmallButton("Delete"))
            {
                ImGui::OpenPopup("Confirm##del");
            }
            bool opened = ImGui::BeginPopupModal("Confirm##del", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            if (opened)
            {
                ImGui::Text("Delete %s ?", label.c_str());
                ImGui::Separator();
                if (ImGui::Button("Yes", ImVec2(80, 0)))
                {
                    if (r.e.is_alive()) r.e.destruct();
                    if (entityMgrSelectedId == r.e.id()) entityMgrSelectedId = 0;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("No", ImVec2(80, 0)))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();

    ImGui::Separator();

    // right below: details for the selected entity (read-only info; edits are on the row)
    if (entityMgrSelectedId != 0)
    {
        flecs::entity sel = ecs.entity(entityMgrSelectedId);
        if (sel.is_valid() && sel.is_alive() && sel.has(flecs::ChildOf, active_board))
        {
            ImGui::TextUnformatted("Selected:");
            bool isMarker = sel.has<MarkerComponent>();
            uint64_t id = sel.has<Identifier>() ? sel.get<Identifier>()->id : 0;
            ImGui::SameLine();
            ImGui::Text("%s_%llu", isMarker ? "Marker" : (sel.has<FogOfWar>() ? "Fog" : "Entity"),
                        (unsigned long long)id);

            if (sel.has<TextureComponent>() && isMarker)
            {
                // show filename (full path is stored; you said that’s what you have)
                const auto* t = sel.get<TextureComponent>();
                if (t)
                {
                    ImGui::Text("Texture: %s", t->imagePath.c_str());
                }
            }

            if (sel.has<Position>())
            {
                const auto* p = sel.get<Position>();
                ImGui::Text("Position: (%d, %d)", p->x, p->y);
            }
            if (sel.has<Size>())
            {
                const auto* s = sel.get<Size>();
                ImGui::Text("Size: (%.1f, %.1f)", s->width, s->height);
            }
        }
        else
        {
            ImGui::TextUnformatted("Selected entity is no longer valid.");
        }
    }
    else
    {
        ImGui::TextUnformatted("No selection.");
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

//V1
void BoardManager::renderEntityManagerWindow()
{
    if (!showEntityManager)
    {
        setIsNonMapWindowHovered(false);
        return;
    }

    // Dock near mouse on first open, same vibe as your camera window
    ImVec2 mousePos = ImGui::GetMousePos();
    ImGui::SetNextWindowPos(ImVec2(mousePos.x, mousePos.y + ImGui::GetFrameHeightWithSpacing()), ImGuiCond_Appearing);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.12f, 0.16f, 1.0f));
    ImGui::Begin("Entity Manager", &showEntityManager,
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);

    // Important: disable board panning while over this window
    setIsNonMapWindowHovered(ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup));

    // Guard: must have an active board
    if (!isBoardActive())
    {
        ImGui::TextUnformatted("No active board.");
        ImGui::End();
        ImGui::PopStyleColor();
        return;
    }

    // --- Filter (Markers / Fog / Both) ---
    static int filterTab = 0; // 0=All, 1=Markers, 2=Fog
    ImGui::TextUnformatted("Show:");
    ImGui::SameLine();
    ImGui::RadioButton("All", &filterTab, 0); ImGui::SameLine();
    ImGui::RadioButton("Markers", &filterTab, 1); ImGui::SameLine();
    ImGui::RadioButton("Fog", &filterTab, 2);

    ImGui::Separator();

    // --- Left: list of entities ---
    ImGui::BeginChild("entity_list", ImVec2(260, 280), true, ImGuiWindowFlags_HorizontalScrollbar);

    struct Row {
        flecs::entity e;
        bool isMarker = false;
        float area = 0.f;
        std::string label;
    };
    std::vector<Row> rows; rows.reserve(64);

    // Build list
    ecs.defer_begin(); // okay to keep pattern; not strictly required for reads
    active_board.children([&](flecs::entity child)
    {
        const bool isMarker = child.has<MarkerComponent>();
        const bool isFog    = child.has<FogOfWar>();
        if (!isMarker && !isFog) return;

        if (filterTab == 1 && !isMarker) return; // Markers only
        if (filterTab == 2 && !isFog)    return; // Fog only

        // Visible name
        std::string name;
        if (child.has<Identifier>())
        {
            auto id = child.get<Identifier>()->id;
            name = std::to_string(id);
        }

        std::string display = isMarker ? "[Marker] " : "[Fog] ";
        if (isMarker && child.has<MarkerComponent>())
        {
            const auto* mc = child.get<MarkerComponent>();
            if (mc && !mc->name.empty())
                display += mc->name + " (id:" + (name.empty()? "?" : name) + ")";
            else
                display += (name.empty()? "(unnamed)" : "id:" + name);
        }
        else
        {
            display += (name.empty()? "fog" : "id:" + name);
        }

        float area = 0.f;
        if (child.has<Size>())
        {
            const auto* s = child.get<Size>();
            area = std::max(0.f, s->width) * std::max(0.f, s->height);
        }

        rows.push_back(Row{ child, isMarker, area, std::move(display) });
    });
    ecs.defer_end();

    // Sort for stable UI: Markers first, then by smaller area
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b){
        if (a.isMarker != b.isMarker) return a.isMarker > b.isMarker; // markers first
        return a.area < b.area; // smaller first
    });

    // Render list
    bool selectionStillValid = false;
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const bool selected = (entityMgrSelectedId != 0 && rows[i].e.id() == entityMgrSelectedId);
        if (ImGui::Selectable(rows[i].label.c_str(), selected))
        {
            entityMgrSelectedId = rows[i].e.id();
        }
        if (selected) selectionStillValid = true;
    }
    if (!selectionStillValid) entityMgrSelectedId = 0;

    ImGui::EndChild();

    ImGui::SameLine();

    // --- Right: details and controls for selected entity ---
    ImGui::BeginChild("entity_details", ImVec2(320, 280), true);

    if (entityMgrSelectedId == 0)
    {
        ImGui::TextUnformatted("Select an entity to edit.");
    }
    else
    {
        flecs::entity sel = ecs.entity(entityMgrSelectedId);
        if (!sel.is_valid() || !sel.is_alive() || !sel.has(flecs::ChildOf, active_board))
        {
            ImGui::TextUnformatted("Entity is no longer valid.");
        }
        else
        {
            // Basic header
            bool isMarker = sel.has<MarkerComponent>();
            ImGui::Text("%s", isMarker ? "Marker" : (sel.has<FogOfWar>() ? "Fog of War" : "Entity"));

            if (sel.has<Identifier>())
            {
                auto id = sel.get<Identifier>()->id;
                ImGui::SameLine();
                ImGui::TextDisabled(" (id: %llu)", (unsigned long long)id);
            }

            ImGui::Separator();

            // Visibility
            if (sel.has<Visibility>())
            {
                auto v = sel.get_mut<Visibility>();
                bool vis = v->isVisible;
                if (ImGui::Checkbox("Visible", &vis))
                {
                    v->isVisible = vis;
                }
            }

            // Size
            if (sel.has<Size>())
            {
                auto s = sel.get_mut<Size>();
                float w = s->width, h = s->height;

                if (ImGui::InputFloat("Width", &w, 1.0f, 10.0f, "%.1f"))  { s->width  = std::max(0.0f, w); }
                if (ImGui::InputFloat("Height", &h, 1.0f, 10.0f, "%.1f")) { s->height = std::max(0.0f, h); }

                ImGui::SameLine();
                if (ImGui::Button("+10%")) { s->width *= 1.1f; s->height *= 1.1f; }
                ImGui::SameLine();
                if (ImGui::Button("-10%")) { s->width *= 0.9f; s->height *= 0.9f; }
            }

            // Position
            if (sel.has<Position>())
            {
                auto p = sel.get_mut<Position>();
                int x = p->x, y = p->y;
                if (ImGui::InputInt("X", &x)) p->x = x;
                if (ImGui::InputInt("Y", &y)) p->y = y;
            }

            // Marker specifics
            if (isMarker && sel.has<MarkerComponent>())
            {
                auto mc = sel.get_mut<MarkerComponent>();
                char nameBuf[128] = {};
                if (!mc->name.empty())
                {
                    std::snprintf(nameBuf, sizeof(nameBuf), "%s", mc->name.c_str());
                }
                if (ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf)))
                {
                    mc->name = nameBuf;
                }
            }

            ImGui::Separator();

            // Delete (with confirmation)
            static bool openConfirm = false;
            if (ImGui::Button("Delete"))
            {
                ImGui::OpenPopup("Confirm Delete##EntityMgr");
                openConfirm = true;
            }
            if (ImGui::BeginPopupModal("Confirm Delete##EntityMgr", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::TextUnformatted("Delete this entity?");
                ImGui::Separator();
                if (ImGui::Button("Yes", ImVec2(100, 0)))
                {
                    if (sel.is_alive())
                    {
                        sel.destruct();
                        entityMgrSelectedId = 0;
                    }
                    ImGui::CloseCurrentPopup();
                    openConfirm = false;
                }
                ImGui::SameLine();
                if (ImGui::Button("No", ImVec2(100, 0)))
                {
                    ImGui::CloseCurrentPopup();
                    openConfirm = false;
                }
                ImGui::EndPopup();
            }

            // While the modal is open, ensure board panning remains disabled
            if (openConfirm) setIsNonMapWindowHovered(true);
        }
    }

    ImGui::EndChild();

    ImGui::End();
    ImGui::PopStyleColor();
}

//TOOLBAR
ImGui::PushStyleColor(ImGuiCol_Button, button_popup_color);
ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_popup_hover);
ImGui::PushStyleColor(ImGuiCol_ButtonActive, button_popup_active);
if (ImGui::Button("Entity Manager", ImVec2(110, 40)))
{
    showEntityManager = !showEntityManager;
}
ImGui::PopStyleColor(3);

// later in your frame:
renderEntityManagerWindow();


*/

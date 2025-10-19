#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "BoardManager.h"

#include "Renderer.h"
#include "Shader.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "VertexBufferLayout.h"
#include "Texture.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <glm/gtc/round.hpp>

#include "Components.h"
#include <filesystem>
#include <random>  // For random number generation
#include <atomic>  // For atomic counter
#include <cstdint> // For uint64_t and UINT64_MAX
#include "Serializer.h"
#include "NetworkManager.h"
#include "Logger.h"

#include <chrono>
#include <cstdlib>    // getenv
#include <functional> // std::hash
#include <string>
#include <thread>
#include <unordered_map>

BoardManager::BoardManager(flecs::world ecs, std::weak_ptr<NetworkManager> network_manager, std::shared_ptr<DirectoryWindow> map_directory, std::shared_ptr<DirectoryWindow> marker_directory) :
    ecs(ecs), camera(), currentTool(Tool::MOVE), mouse_start_screen_pos({0, 0}), mouse_start_world_pos({0, 0}), mouse_current_world_pos({0, 0}), marker_directory(marker_directory), map_directory(map_directory), network_manager(network_manager)
{

    std::filesystem::path map_path = std::filesystem::path(map_directory->directoryPath);
    std::filesystem::path base_path = map_path.parent_path();
    std::filesystem::path marker_directory_path = base_path / "Markers";

    marker_directory->directoryName = "MarkerDiretory";
    marker_directory->directoryPath = marker_directory_path.string();
    marker_directory->startMonitoring();
    marker_directory->generateTextureIDs();
}

BoardManager::~BoardManager()
{
}

bool BoardManager::isBoardActive()
{
    return active_board.is_valid();
}

void BoardManager::closeBoard()
{
    active_board = flecs::entity();
}

flecs::entity BoardManager::createBoard(std::string board_name, std::string map_image_path, GLuint texture_id, glm::vec2 size)
{
    auto board = ecs.entity()
                     .set(Identifier{generateUniqueId()})
                     .set(Board{board_name})
                     .set(Panning{false})
                     .set(Grid{{0, 0}, 50.0f, false, false, false, 0.5f})
                     .set(TextureComponent{texture_id, map_image_path, size})
                     .set(Size{size.x, size.y});
    setActiveBoard(board);
    return board;
}

void BoardManager::setActiveBoard(flecs::entity board_entity)
{
    active_board = board_entity;
    auto nm = network_manager.lock();
    if (!nm)
        if (nm->getPeerRole() == Role::GAMEMASTER)
            nm->broadcastBoard(active_board);
}

void BoardManager::renderToolbar(const ImVec2& window_position)
{

    ImGuiWindowFlags toolbar_child_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize;

    auto panel_color = ImVec4(0.18f, 0.22f, 0.27f, 1.00f); // toolbar panel
    auto bg_color = ImVec4(0.12f, 0.14f, 0.17f, 1.00f);
    auto old_bg_color = ImVec4(0.2f, 0.3f, 0.4f, 1.0f);

    auto green_light_color = ImVec4(0.55f, 0.95f, 0.65f, 1.00f);
    auto green_mid_color = ImVec4(0.20f, 0.75f, 0.35f, 1.00f);
    auto green_dark_color = ImVec4(0.12f, 0.45f, 0.22f, 1.00f);
    auto mint_color = ImVec4(0.50f, 0.95f, 0.85f, 1.00f);

    auto blue_light_color = ImVec4(0.55f, 0.75f, 1.00f, 1.00f);
    auto blue_mid_color = ImVec4(0.28f, 0.50f, 0.92f, 1.00f);
    auto blue_dark_color = ImVec4(0.15f, 0.30f, 0.55f, 1.00f);

    auto purple_color = ImVec4(0.70f, 0.45f, 0.95f, 1.00f);
    auto violet_color = ImVec4(0.55f, 0.35f, 0.85f, 1.00f);
    auto mid_purple_color = ImVec4(0.45f, 0.28f, 0.70f, 1.00f);

    auto orange_color = ImVec4(0.98f, 0.60f, 0.20f, 1.00f);
    auto amber_color = ImVec4(1.00f, 0.78f, 0.25f, 1.00f);
    auto yellow_color = ImVec4(0.95f, 0.90f, 0.30f, 1.00f);

    auto button_toggled_color = amber_color;
    auto button_toggled_hover = orange_color;
    auto button_toggled_active = yellow_color;

    auto button_untoggled_color = blue_mid_color;
    auto button_untoggled_hover = blue_light_color;
    auto button_untoggled_active = yellow_color;

    auto button_popup_color = green_dark_color;
    auto button_popup_hover = green_mid_color;
    auto button_popup_active = mint_color;

    ImGui::SetCursorPos(window_position);

    ImVec2 toolbar_size = ImVec2(0, 0); // Auto-size to content

    ImGui::PushStyleColor(ImGuiCol_WindowBg, panel_color);
    ImGui::BeginChild("ToolbarChild", toolbar_size, false, toolbar_child_flags);

    // Tool: Move
    ImGui::PushStyleColor(ImGuiCol_Button, currentTool == Tool::MOVE ? button_toggled_color : button_untoggled_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, currentTool == Tool::MOVE ? button_toggled_hover : button_untoggled_hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, currentTool == Tool::MOVE ? button_toggled_active : button_untoggled_active);
    if (ImGui::Button("Move Tool", ImVec2(100, 40)))
    {
        currentTool = Tool::MOVE;
    }
    ImGui::PopStyleColor(3);
    ImGui::SameLine(); // Ensure buttons are on the same row
    auto nm = network_manager.lock();
    if (!nm)
        throw std::exception("[BoardManager] NetworkManager Expired");

    if (nm->getPeerRole() == Role::GAMEMASTER)
    {
        // Tool: Fog
        ImGui::PushStyleColor(ImGuiCol_Button, currentTool == Tool::FOG ? button_toggled_color : button_untoggled_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, currentTool == Tool::FOG ? button_toggled_hover : button_untoggled_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, currentTool == Tool::FOG ? button_toggled_active : button_untoggled_active);
        if (ImGui::Button("Fog Tool", ImVec2(100, 40)))
        {
            currentTool = Tool::FOG;
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine(); // Ensure buttons are on the same row

        // Tool: Select
        ImGui::PushStyleColor(ImGuiCol_Button, currentTool == Tool::SELECT ? button_toggled_color : button_untoggled_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, currentTool == Tool::SELECT ? button_toggled_hover : button_untoggled_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, currentTool == Tool::SELECT ? button_toggled_active : button_untoggled_active);
        if (ImGui::Button("Select Tool", ImVec2(100, 40)))
        {
            currentTool = Tool::SELECT;
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine(); // Ensure buttons are on the same row
    }

    ImGui::PushStyleColor(ImGuiCol_Button, button_popup_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_popup_hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, button_popup_active);
    if (ImGui::Button("Camera Settings", ImVec2(110, 40)))
    {
        showCameraSettings = true;
    }
    ImGui::PopStyleColor(3);
    if (nm->getPeerRole() == Role::GAMEMASTER)
    {
        ImGui::SameLine(); // Ensure buttons are on the same row
        ImGui::PushStyleColor(ImGuiCol_Button, button_popup_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_popup_hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, button_popup_active);
        if (ImGui::Button("Grid Settings", ImVec2(110, 40)))
        {
            showGridSettings = !showGridSettings;
        }
        ImGui::PopStyleColor(3);
    }

    // Pop the toolbar's background color
    ImGui::PopStyleColor();
    // End the child window
    ImGui::EndChild();

    renderGridWindow();
    renderCameraWindow();
}

glm::vec2 BoardManager::computeMarkerDrawSize_ARFit(const TextureComponent& tex, float basePx /*e.g., 50.0f*/, float scale /*slider, default 1.0f*/)
{
    const glm::vec2 texPx = tex.size; // original pixels stored in your TextureComponent
    if (texPx.x <= 0.0f || texPx.y <= 0.0f)
    {
        const float box = basePx * scale;
        return {box, box};
    }
    const float box = basePx * scale;
    const float s = std::min(box / texPx.x, box / texPx.y);
    return texPx * s; // scaled to fit inside the box, AR preserved
}

void BoardManager::renderBoard(VertexArray& va, IndexBuffer& ib, Shader& shader, Shader& grid_shader, Renderer& renderer)
{
    auto nm = network_manager.lock();
    if (!nm)
        throw std::exception("[BoardManager] Network Manager expired!!");

    const TextureComponent* texture = active_board.get<TextureComponent>();
    if (texture->textureID != 0)
    {
        const Board* board = active_board.get<Board>();
        const Grid* grid = active_board.get<Grid>();
        const Size* size = active_board.get<Size>();

        glm::mat4 viewMatrix = camera.getViewMatrix(); // ObtÃ©m a matriz de visualizaÃ§Ã£o da cÃ¢mera (pan/zoom)
        glm::mat4 projection = camera.getProjectionMatrix();
        glm::mat4 board_model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
        board_model = glm::scale(board_model, glm::vec3(size->width, size->height, 1.0f));

        shader.Bind();
        shader.SetUniformMat4f("projection", projection);
        shader.SetUniformMat4f("view", viewMatrix);
        shader.SetUniformMat4f("model", board_model);
        shader.SetUniform1f("u_Alpha", 1.0f);
        shader.SetUniform1i("u_UseTexture", 1);
        shader.SetUniform1i("u_Texture", 0);
        shader.Unbind();

        GLCall(glActiveTexture(GL_TEXTURE0));
        GLCall(glBindTexture(GL_TEXTURE_2D, texture->textureID));

        renderer.Draw(va, ib, shader);

        if (grid)
        {
            if (grid->visible)
            {
                grid_shader.Bind();
                grid_shader.SetUniformMat4f("projection", projection);
                grid_shader.SetUniformMat4f("view", viewMatrix);
                grid_shader.SetUniformMat4f("model", board_model); // Grid model is the same as the board
                grid_shader.SetUniform1i("grid_type", grid->is_hex ? 1 : 0);
                grid_shader.SetUniform1f("cell_size", grid->cell_size);
                grid_shader.SetUniform2f("grid_offset", grid->offset.x, grid->offset.y);
                grid_shader.SetUniform1f("opacity", grid->opacity);
                grid_shader.Unbind();

                renderer.Draw(va, ib, grid_shader);
            }
        }

        ecs.defer_begin(); // Start deferring modifications
        active_board.children([&](flecs::entity child)
                              {
            if (child.has<MarkerComponent>()) {
                const TextureComponent* texture_marker = child.get<TextureComponent>();
                if (texture_marker->textureID != 0) {
                    const Position* position_marker = child.get<Position>();
                    const Visibility* visibility_marker = child.get<Visibility>();
                    const Size* size_marker = child.get<Size>();

                    glm::mat4 marker_model = glm::translate(glm::mat4(1.0f), glm::vec3(position_marker->x, position_marker->y, 0.0f));
                    marker_model = glm::scale(marker_model, glm::vec3(size_marker->width, size_marker->height, 1.0f));

                    //glm::mat4 mvp = projection * viewMatrix * marker_model; //Calculate Screen Position(Can use method to standize it, but alter to return the MVP
                    float alpha = 1.0f;
                    if (!visibility_marker->isVisible) {
                        if (nm->getPeerRole() == Role::GAMEMASTER)
                        {
                            alpha = 0.5f;
                        }
                        else {
                            alpha = 0.0f;
                        }
                    }

                    shader.Bind();
                    shader.SetUniformMat4f("projection", projection);
                    shader.SetUniformMat4f("view", viewMatrix);
                    shader.SetUniformMat4f("model", marker_model);
                    shader.SetUniform1f("u_Alpha", alpha);
                    shader.SetUniform1i("u_Texture", 0);
                    shader.SetUniform1i("u_UseTexture", 1);
                    shader.Unbind();

                    GLCall(glActiveTexture(GL_TEXTURE0));
                    GLCall(glBindTexture(GL_TEXTURE_2D, texture_marker->textureID));

                    renderer.Draw(va, ib, shader);
                }
            }

            if (child.has<FogOfWar>()) {
                const Position* position_marker = child.get<Position>();
                const Visibility* visibility_marker = child.get<Visibility>();
                const TextureComponent* texture_marker = child.get<TextureComponent>();
                const Size* size_marker = child.get<Size>();

                glm::mat4 fog_model = glm::translate(glm::mat4(1.0f), glm::vec3(position_marker->x, position_marker->y, 0.0f));
                fog_model = glm::scale(fog_model, glm::vec3(size_marker->width, size_marker->height, 1.0f));

                float alpha = 1.0f;
              
                if (!visibility_marker->isVisible) {
                    if (nm->getPeerRole() == Role::GAMEMASTER) {
                        alpha = 0.3f;
                    }
                    else
                    {
                        alpha = 0.0f;
                    }
                }
                else
                {
                    if (nm->getPeerRole() == Role::GAMEMASTER)
                    {
                        alpha = 0.6f;
                    }
                }

                shader.Bind();
                shader.SetUniformMat4f("projection", projection);
                shader.SetUniformMat4f("view", viewMatrix);
                shader.SetUniformMat4f("model", fog_model);
                shader.SetUniform1f("u_Alpha", alpha);
                shader.SetUniform1i("u_UseTexture", 0);
                shader.Unbind();

                renderer.Draw(va, ib, shader);

            } });
        ecs.defer_end();
    }
}

flecs::entity BoardManager::createMarker(const std::string& imageFilePath, GLuint textureId, glm::vec2 position, glm::vec2 size)
{
    auto nm = network_manager.lock();
    if (!nm)
        throw std::exception("[BoardManager] Network Manager expired!!");

    auto markerScale = marker_directory->getGlobalSizeSlider();
    auto texture_marker = TextureComponent{textureId, imageFilePath, size};
    const glm::vec2 drawSz = computeMarkerDrawSize_ARFit(texture_marker, markerBasePx, markerScale);

    flecs::entity marker = ecs.entity()
                               .set(Identifier{generateUniqueId()})
                               .set(Position{(int)position.x, (int)position.y}) //World Position
                               .set(Size{drawSz.x, drawSz.y})
                               .set(texture_marker)
                               .set(Visibility{true})
                               .set(MarkerComponent{"", false, false})
                               .set(Moving{false});

    marker.add(flecs::ChildOf, active_board);

    auto board_id = active_board.get<Identifier>()->id;
    nm->broadcastMarker(board_id, marker);
    return marker;
}

void BoardManager::deleteMarker(flecs::entity markerEntity)
{
    markerEntity.destruct();
}

// Generates a unique 64-bit ID
//uint64_t BoardManager::generateUniqueId()
//{
//    static std::atomic<uint64_t> counter{0};            // Atomic counter for thread safety
//    static std::mt19937_64 rng(std::random_device{}()); // Random number generator
//    static std::uniform_int_distribution<uint64_t> dist(1, UINT64_MAX);
//
//    uint64_t random_part = dist(rng);                                               // Generate a random 64-bit number
//    uint64_t unique_id = (random_part & 0xFFFFFFFFFFFF0000) | (counter++ & 0xFFFF); // Combine random and counter
//
//    return unique_id;
//}

uint64_t BoardManager::generateUniqueId()
{
    // 64-bit Snowflake layout:
    // [ 42 bits timestamp(ms since 2020-01-01 UTC) ][ 10 bits node ][ 12 bits sequence ]
    static constexpr uint64_t TS_BITS = 42;
    static constexpr uint64_t NODE_BITS = 10;
    static constexpr uint64_t SEQ_BITS = 12;

    static constexpr uint64_t TS_MASK = (1ULL << TS_BITS) - 1;
    static constexpr uint64_t NODE_MASK = (1ULL << NODE_BITS) - 1;
    static constexpr uint64_t SEQ_MASK = (1ULL << SEQ_BITS) - 1;

    // Custom epoch: 2020-01-01T00:00:00Z in UNIX ms
    static constexpr int64_t EPOCH_MS = 1577836800000LL;

    // Derive a stable 10-bit node id for this process/machine (once).
    // Try COMPUTERNAME/HOSTNAME; fallback to random.
    static const uint16_t NODE_ID = []() -> uint16_t
    {
        std::string basis;
        if (const char* cn = std::getenv("COMPUTERNAME"); cn && *cn)
            basis = cn;
        else if (const char* hn = std::getenv("HOSTNAME"); hn && *hn)
            basis = hn;
        else
        {
            std::random_device rd;
            basis = std::to_string(rd());
        }
        uint64_t h = std::hash<std::string>{}(basis);
        // xor-fold a bit to mix high/low
        h ^= (h >> 33) ^ (h >> 17) ^ (h >> 9);
        return static_cast<uint16_t>(h) & NODE_MASK; // 10 bits
    }();

    // Per-process state
    static std::atomic<uint64_t> lastMs{0};
    static std::atomic<uint16_t> seq{0};

    // Current ms since custom epoch
    const auto nowEpoch = std::chrono::time_point_cast<std::chrono::milliseconds>(
                              std::chrono::system_clock::now())
                              .time_since_epoch()
                              .count();
    uint64_t nowMs = (nowEpoch >= EPOCH_MS) ? static_cast<uint64_t>(nowEpoch - EPOCH_MS) : 0ULL;

    uint64_t last = lastMs.load(std::memory_order_relaxed);
    if (nowMs == last)
    {
        // Same millisecond -> increment sequence (12 bits)
        uint16_t s = static_cast<uint16_t>(seq.fetch_add(1, std::memory_order_relaxed)) & SEQ_MASK;
        if (s == 0)
        {
            // Overflowed the 12-bit sequence; wait for the next millisecond
            do
            {
                std::this_thread::yield();
                const auto now2 = std::chrono::time_point_cast<std::chrono::milliseconds>(
                                      std::chrono::system_clock::now())
                                      .time_since_epoch()
                                      .count();
                nowMs = (now2 >= EPOCH_MS) ? static_cast<uint64_t>(now2 - EPOCH_MS) : 0ULL;
            } while (nowMs == last);
            lastMs.store(nowMs, std::memory_order_relaxed);
            seq.store(0, std::memory_order_relaxed);
            s = 0;
        }
        // Pack: (ts << (10+12)) | (node << 12) | seq
        return ((nowMs & TS_MASK) << (NODE_BITS + SEQ_BITS)) | ((static_cast<uint64_t>(NODE_ID) & NODE_MASK) << SEQ_BITS) | (static_cast<uint64_t>(s) & SEQ_MASK);
    }
    else
    {
        // New millisecond -> reset sequence
        lastMs.store(nowMs, std::memory_order_relaxed);
        seq.store(0, std::memory_order_relaxed);
        return ((nowMs & TS_MASK) << (NODE_BITS + SEQ_BITS)) | ((static_cast<uint64_t>(NODE_ID) & NODE_MASK) << SEQ_BITS) | 0ULL;
    }
}

// Finds an entity by its Identifier component with the specified ID
flecs::entity BoardManager::findEntityById(uint64_t target_id)
{
    flecs::entity result;

    // Iterate over all entities with Identifier component
    ecs.each<Identifier>([&](flecs::entity e, Identifier& identifier)
                         {
        if (identifier.id == target_id) {
            result = e;  // Store matching entity
        } });

    return result; // Returns the found entity, or an empty entity if not found
}
// BoardManager.cpp
//bool BoardManager::canMoveMarker(const MarkerComponent* mc, uint64_t markerId) const
//{
//    if (!mc)
//        return false;
//
//    auto nm = network_manager.lock();
//    if (!nm)
//        return false;
//
//    if (edit_window_entity.is_valid() && edit_window_entity.has<Moving>())
//    {
//        if (edit_window_entity.get<Moving>()->isDragging)
//            return false;
//    }
//
//    const auto role = nm->getPeerRole(); // existing in your NM
//    if (role == Role::GAMEMASTER)
//        return true; // GM can always move
//
//    if (mc->locked)
//        return false; // hard lock blocks players
//    if (mc->allowAllPlayersMove)
//        return true; // GM allowed “all players move”
//
//    const std::string me = nm->getMyId();
//    if (mc->ownerPeerId.empty())
//        return false;
//
//    return (mc->ownerPeerId == me);
//}

bool BoardManager::canMoveMarker(const MarkerComponent* mc, flecs::entity markerEnt) const
{
    if (!mc || !markerEnt.is_valid())
        return false;

    auto nm = network_manager.lock();
    if (!nm)
        return false;

    // Disallow if being dragged by anyone
    if (markerEnt.has<Identifier>())
    {
        const auto mid = markerEnt.get<Identifier>()->id;
        if (nm->isMarkerBeingDragged(mid))
            return false;
    }

    const auto role = nm->getPeerRole();
    if (role == Role::GAMEMASTER)
        return true;

    // base ownership rules
    if (mc->locked)
        return false;
    if (mc->allowAllPlayersMove)
        return true;

    const std::string me = nm->getMyId();
    if (!mc->ownerPeerId.empty() && mc->ownerPeerId == me)
        return true;

    // --- Fog rule for players ---
    // Player can move if the marker is NOT visible AND is fully covered by a VISIBLE fog.
    if (auto vis = markerEnt.get<Visibility>(); vis && !vis->isVisible)
    {
        // Compute marker AABB
        auto pos = markerEnt.get<Position>();
        auto siz = markerEnt.get<Size>();
        if (pos && siz)
        {
            const float mx1 = pos->x - siz->width * 0.5f;
            const float mx2 = pos->x + siz->width * 0.5f;
            const float my1 = pos->y - siz->height * 0.5f;
            const float my2 = pos->y + siz->height * 0.5f;

            bool covered = false;
            active_board.children([&](flecs::entity child)
                                  {
                if (!child.has<FogOfWar>()) return;
                if (auto fvis = child.get<Visibility>(); !fvis || !fvis->isVisible) return; // fog must be visible

                auto fpos = child.get<Position>();
                auto fsz  = child.get<Size>();
                if (!fpos || !fsz) return;

                const float fx1 = fpos->x - fsz->width * 0.5f;
                const float fx2 = fpos->x + fsz->width * 0.5f;
                const float fy1 = fpos->y - fsz->height * 0.5f;
                const float fy2 = fpos->y + fsz->height * 0.5f;

                // full containment
                if (mx1 >= fx1 && mx2 <= fx2 && my1 >= fy1 && my2 <= fy2)
                    covered = true; });
            if (covered)
                return true;
        }
    }

    return false;
}

void BoardManager::killIfMouseUp(bool isMouseDown)
{
    if (isMouseDown)
        return;

    // Ensure panning is off
    if (active_board.is_valid())
        active_board.set<Panning>({false});

    auto nm = network_manager.lock();

    // If some marker still locally dragging (UI glitch), force-end it now
    ecs.defer_begin();
    ecs.each([&](flecs::entity entity, MarkerComponent& mc, Moving& moving, Position& pos)
             {
        if (!entity.has(flecs::ChildOf, active_board)) return;
        if (!moving.isDragging) return;

        moving.isDragging = false;

        if (nm && entity.has<Identifier>() && active_board.has<Identifier>())
        {
            const auto bid = active_board.get<Identifier>()->id;
            const auto mid = entity.get<Identifier>()->id;

            nm->markDraggingLocal(mid, false);
            nm->broadcastMarkerMoveState(bid, entity);  // end (final)
            nm->forceCloseDrag(mid);
        } });
    ecs.defer_end();
}

bool BoardManager::isMouseOverMarker(glm::vec2 world_position)
{
    bool hovered = false;

    // Query all markers that are children of the active board and have MarkerComponent
    ecs.defer_begin();
    ecs.each([&](flecs::entity entity, const MarkerComponent& marker_component, const Position& markerPos, const Size& markerSize, Moving& moving)
             {

        if (entity.has(flecs::ChildOf, active_board)) {
            bool withinXBounds = (world_position.x >= (markerPos.x - markerSize.width / 2)) &&
                (world_position.x <= (markerPos.x + markerSize.width / 2));

            bool withinYBounds = (world_position.y >= (markerPos.y - markerSize.height / 2)) &&
                (world_position.y <= (markerPos.y + markerSize.height / 2));

            if (!(withinXBounds && withinYBounds))
                return;
            if (!canMoveMarker(&marker_component, entity))
                return;

            moving.isDragging = true;
            hovered = true;

            if (auto nm = network_manager.lock())
            {
                if (entity.has<Identifier>() && active_board.has<Identifier>())
                {
                    const auto bid = active_board.get<Identifier>()->id;
                    const auto mid = entity.get<Identifier>()->id;

                    nm->markDraggingLocal(mid, true);       
                    nm->broadcastMarkerMoveState(bid, entity); 
                }
            }
        } });
    ecs.defer_end();

    return hovered;
}

// Snap to the nearest cell center in a square grid:
// centers are at: offset + (i + 0.5) * cell_size
static inline glm::vec2 snapToSquareCenter(const glm::vec2& worldPos, const glm::vec2& offset, float cell)
{
    // transform to grid-local space (subtract offset, not add)
    const glm::vec2 local = worldPos - offset;

    // center within a cell
    const glm::vec2 half(cell * 0.5f);

    // round to nearest cell center in local space, then bring back to world
    const glm::vec2 snappedLocal = glm::round((local - half) / cell) * cell + half;
    return snappedLocal + offset;
}

static inline glm::vec2 snapToGridCenter(const glm::vec2& worldPos, const Grid& grid)
{
    if (!grid.snap_to_grid || grid.cell_size <= 0.0f)
        return worldPos;
    return snapToSquareCenter(worldPos, grid.offset, grid.cell_size);
}

void BoardManager::startMouseDrag(glm::vec2 mousePos, bool draggingMap)
{
    mouse_start_world_pos = mousePos; // Captura a posiÃ§Ã£o inicial do mouse
    mouse_start_screen_pos = camera.worldToScreenPosition(mousePos);
    if (currentTool == Tool::MOVE)
    {
        if (draggingMap)
        {
            active_board.set<Panning>({true});
        }
    }
    else if (currentTool == Tool::FOG)
    {
        is_creating_fog = true;
    }
}

void BoardManager::endMouseDrag()
{
    if (!active_board.is_valid())
        return;

    active_board.set<Panning>({false});
    auto nm = network_manager.lock();

    const Grid* grid = active_board.get<Grid>();
    const bool canSnap = (grid && grid->snap_to_grid && grid->cell_size > 0.0f);

    ecs.defer_begin();
    ecs.each([&](flecs::entity entity, const MarkerComponent& marker, Moving& moving, Position& pos)
             {
        if (!entity.has(flecs::ChildOf, active_board)) return;
        if (!moving.isDragging) return;


        if (canSnap)
        {
            glm::vec2 snapped = snapToSquareCenter(glm::vec2(pos.x, pos.y), grid->offset, grid->cell_size);
            pos.x = snapped.x;
            pos.y = snapped.y;
        }

        moving.isDragging = false;
        if (nm && entity.has<Identifier>())
        {
            const auto bid = active_board.get<Identifier>()->id;
            const auto mid = entity.get<Identifier>()->id;

            nm->markDraggingLocal(mid, false);      // <- local registry
            nm->broadcastMarkerMoveState(bid, entity); // end (isDragging=false + final pos)broadcastMarkerUpdate(bid, entity); // <- final pos + mov=false
            nm->forceCloseDrag(mid);
        } });

    ecs.defer_end();
    is_creating_fog = false;
}

void BoardManager::handleMarkerDragging(glm::vec2 world_position)
{
    auto nm = network_manager.lock();
    if (!nm)
        throw std::exception("[BoardManager] Network Manager expired!!");

    ecs.defer_begin();
    ecs.each([&](flecs::entity entity, const MarkerComponent& marker, Moving& moving, Position& position)
             {
        if (!entity.has(flecs::ChildOf, active_board)) return;
        if (!moving.isDragging) return;

        glm::vec2 delta = world_position - mouse_start_world_pos;
        position.x += delta.x;
        position.y += delta.y;
        mouse_start_world_pos = world_position;

        const auto id = entity.get<Identifier>()->id;
        if (shouldSendMarkerMove(id))
        {
            //Logger::instance().log("localtunnel", Logger::Level::Info, "broadcastMarkerMove!");
            nm->broadcastMarkerMove(active_board.get<Identifier>()->id, entity);
        } });
    ecs.defer_end();
}

bool BoardManager::shouldSendMarkerMove(uint64_t markerId) const
{
    using namespace std::chrono;
    static std::unordered_map<uint64_t, steady_clock::time_point> lastSent;
    static constexpr auto kMinInterval = milliseconds(33); // ~30 Hz. Use 50ms for ~20 Hz if you prefer.

    const auto now = steady_clock::now();
    auto it = lastSent.find(markerId);
    if (it == lastSent.end() || (now - it->second) >= kMinInterval)
    {
        lastSent[markerId] = now;
        return true; // allow this send
    }
    return false; // too soon, skip this tick
}

bool BoardManager::isPanning()
{
    const Panning* panning = active_board.get<Panning>();
    return panning->isPanning;
}

bool BoardManager::isDraggingMarker()
{
    bool isDragginMarker = false;
    ecs.defer_begin();
    ecs.each([&](flecs::entity entity, const MarkerComponent& marker, Moving& moving)
             {
        if (entity.has(flecs::ChildOf, active_board) && moving.isDragging) {
            isDragginMarker =  true;
        } });
    ecs.defer_end();
    return isDragginMarker;
}

glm::vec2 BoardManager::getMouseStartPosition() const
{
    return mouse_start_world_pos;
}

void BoardManager::panBoard(glm::vec2 current_mouse_fbo_pos)
{
    glm::vec2 delta_screen = mouse_start_screen_pos - current_mouse_fbo_pos;
    float world_scale_factor = camera.getZoom();
    glm::vec2 delta_world = delta_screen / world_scale_factor;
    camera.pan(delta_world);
    mouse_start_screen_pos = current_mouse_fbo_pos;
}

void BoardManager::resetCamera()
{
    camera.setPosition(glm::vec2(0.0f, 0.0f));
    camera.setZoom(1.0f);
}

void BoardManager::deleteFogOfWar(flecs::entity fogEntity)
{
    fogEntity.destruct();
}

flecs::entity BoardManager::createFogOfWar(glm::vec2 startPos, glm::vec2 size)
{
    auto nm = network_manager.lock();
    if (!nm)
        throw std::exception("[BoardManager] Network Manager expired!!");

    auto fog = ecs.entity()
                   .set(Identifier{generateUniqueId()})
                   .set(Position{(int)startPos.x, (int)startPos.y})
                   .set(Size{size.x, size.y})
                   .set(Visibility{true});

    fog.add<FogOfWar>();
    fog.add(flecs::ChildOf, active_board);

    auto board_id = active_board.get<Identifier>()->id;
    nm->broadcastFog(board_id, fog);

    return fog;
}

void BoardManager::handleFogCreation(glm::vec2 end_world_position)
{
    glm::vec2 start_world_position = getMouseStartPosition();

    // Calculate size
    glm::vec2 size = end_world_position - start_world_position; //glm::abs(end_world_position - start_world_position);  // Make sure size is positive
    glm::vec2 corrected_start_position;
    if (size.x < 0)
    {
        corrected_start_position.x = end_world_position.x + glm::abs(size.x) / 2;
    }
    else
    {
        corrected_start_position.x = start_world_position.x + size.x / 2;
    }
    if (size.y < 0)
    {
        corrected_start_position.y = end_world_position.y + glm::abs(size.y) / 2;
    }
    else
    {
        corrected_start_position.y = start_world_position.y + size.y / 2;
    }
    createFogOfWar(corrected_start_position, glm::abs(size));
}

Tool BoardManager::getCurrentTool() const
{
    return currentTool;
}

void BoardManager::setCurrentTool(Tool newTool)
{
    currentTool = newTool;
}

flecs::entity BoardManager::getEntityAtMousePosition(glm::vec2 mouse_position)
{

    auto entity_at_mouse = flecs::entity();
    ecs.defer_begin();
    ecs.each([&](flecs::entity entity, const Position& entity_pos, const Size& entity_size)
             {
        const bool isMarker = entity.has<MarkerComponent>();
        if (isMarker) {
            glm::vec2 world_position = mouse_position;
        
            if (entity.has(flecs::ChildOf, active_board)) {

                bool withinXBounds = (world_position.x >= (entity_pos.x - entity_size.width / 2)) &&
                    (world_position.x <= (entity_pos.x + entity_size.width / 2));

                bool withinYBounds = (world_position.y >= (entity_pos.y - entity_size.height / 2)) &&
                    (world_position.y <= (entity_pos.y + entity_size.height / 2));

                if (withinXBounds && withinYBounds) {
                    entity_at_mouse = entity;
                }
            }

        } });
    ecs.defer_end();

    if (entity_at_mouse.is_valid())
        return entity_at_mouse;

    ecs.defer_begin();
    ecs.each([&](flecs::entity entity, const Position& entity_pos, const Size& entity_size)
             {
        
        glm::vec2 world_position = mouse_position;
        
        if (entity.has(flecs::ChildOf, active_board)) {

            bool withinXBounds = (world_position.x >= (entity_pos.x - entity_size.width / 2)) &&
                (world_position.x <= (entity_pos.x + entity_size.width / 2));

            bool withinYBounds = (world_position.y >= (entity_pos.y - entity_size.height / 2)) &&
                (world_position.y <= (entity_pos.y + entity_size.height / 2));

            if (withinXBounds && withinYBounds) {
                entity_at_mouse = entity;
            }
        } });
    ecs.defer_end();
    return entity_at_mouse;
}

//flecs::entity BoardManager::getEntityAtMousePosition(glm::vec2 mouse_position)
//{
//    flecs::entity best{};
//    bool bestIsMarker = false;
//    float bestArea = std::numeric_limits<float>::infinity();
//
//    // We're only reading; deferring isn't needed, but keeping your pattern:
//    ecs.defer_begin();
//    ecs.each([&](flecs::entity e, const Position& pos, const Size& sz)
//    {
//        // Must belong to the active board
//        if (!e.has(flecs::ChildOf, active_board))
//            return;
//
//        // Point-in-AABB check (centered entity with half extents)
//        const float hx = sz.width  * 0.5f;
//        const float hy = sz.height * 0.5f;
//        const bool inX = (mouse_position.x >= (pos.x - hx)) && (mouse_position.x <= (pos.x + hx));
//        const bool inY = (mouse_position.y >= (pos.y - hy)) && (mouse_position.y <= (pos.y + hy));
//        if (!inX || !inY) return;
//
//        const bool isMarker = e.has<MarkerComponent>();
//        // Only Marker and FogOfWar are relevant; ignore other types if you want:
//        // (If you want Fog only as alternative, check `e.has<FogOfWar>()` here.)
//        const float area = std::max(0.0f, sz.width) * std::max(0.0f, sz.height);
//
//        // Decide priority:
//        bool better = false;
//        if (!best.is_valid()) {
//            better = true; // first hit
//        } else if (isMarker && !bestIsMarker) {
//            better = true; // markers beat non-markers
//        } else if (isMarker == bestIsMarker) {
//            // same type -> smaller area wins
//            if (area < bestArea) better = true;
//        }
//
//        if (better) {
//            best = e;
//            bestIsMarker = isMarker;
//            bestArea = area;
//        }
//    });
//    ecs.defer_end();
//
//    return best;
//}

//GRID

//// Add this helper function to your BoardManager class
//glm::vec2 BoardManager::snapToGrid(glm::vec2 raw_world_pos) {
//    // Get grid parameters from the grid entity
//    auto grid_comp = grid_entity.get<GridComponent>();
//    auto grid_pos = grid_entity.get<Position>();
//
//    if (!grid_comp || !grid_pos) return raw_world_pos; // Safety check
//
//    float cell_size = grid_comp->cellSize;
//    glm::vec2 offset = glm::vec2(grid_pos->x, grid_pos->y);
//
//    if (grid_comp->type == GridComponent::Type::SQUARE) {
//        // Apply offset, snap, and then re-apply offset
//        glm::vec2 relative_pos = raw_world_pos - offset;
//        glm::vec2 snapped_pos = glm::round(relative_pos / cell_size) * cell_size;
//        return snapped_pos + offset;
//    }
//    else if (grid_comp->type == GridComponent::Type::HEXAGONAL) {
//        // Hex snapping logic (pointy-top orientation)
//        float S = cell_size;
//
//        // Remove the grid offset before snapping
//        glm::vec2 p = raw_world_pos - offset;
//
//        // Convert world position to floating-point axial coordinates
//        float q_float = (p.x * 0.57735f - p.y * 0.33333f) / S; // 0.577... = 1/sqrt(3), 0.333... = 1/3
//        float r_float = (p.y * 0.66667f) / S; // 0.666... = 2/3
//
//        // Round to nearest integer hex coordinates
//        int q = round(q_float);
//        int r = round(r_float);
//        int s = round(-q_float - r_float);
//
//        // Correct for rounding errors by re-adjusting the one with the smallest change
//        float q_diff = abs(q - q_float);
//        float r_diff = abs(r - r_float);
//        float s_diff = abs(s - (-q_float - r_float));
//
//        if (q_diff > r_diff && q_diff > s_diff) {
//            q = -r - s;
//        }
//        else if (r_diff > s_diff) {
//            r = -q - s;
//        }
//
//        // Convert integer axial coordinates back to world position and add the offset
//        float snapped_x = S * (1.73205f * q + 0.86603f * r); // 1.73... = sqrt(3), 0.866... = sqrt(3)/2
//        float snapped_y = S * (1.5f * r);
//
//        return glm::vec2(snapped_x, snapped_y) + offset;
//    }
//
//    return raw_world_pos; // Return original if no grid type matches
//}

//Save and Load Board --------------------------------------------------------------------

void BoardManager::saveActiveBoard(std::filesystem::path& board_directory_path)
{
    if (!active_board.is_alive())
    {
        std::cerr << "No active board to save." << std::endl;
        return;
    }
    auto board = active_board.get<Board>();
    if (!std::filesystem::exists(board_directory_path))
    {
        std::filesystem::create_directory(board_directory_path);
    }

    auto board_file_path = board_directory_path / (board->board_name + ".runic");

    std::vector<uint8_t> buffer;
    Serializer::serializeBoardEntity(buffer, active_board, ecs);

    std::ofstream outFile(board_file_path, std::ios::binary);
    if (outFile)
    {
        outFile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        outFile.close();
        std::cout << "Board saved successfully to " << board_directory_path << std::endl;
    }
    else
    {
        std::cerr << "Failed to save board to " << board_directory_path << std::endl;
    }
}

void BoardManager::loadActiveBoard(const std::string& board_file_path)
{
    std::ifstream inFile(board_file_path, std::ios::binary);
    if (inFile)
    {
        std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        inFile.close();

        size_t offset = 0;
        active_board = Serializer::deserializeBoardEntity(buffer, offset, ecs);
        auto texture = active_board.get_mut<TextureComponent>();
        auto map_image = map_directory->getImageByPath(texture->image_path);
        texture->textureID = map_image.textureID;
        texture->size = map_image.size;

        ecs.defer_begin();
        active_board.children([&](flecs::entity child)
                              {
            if (child.has<MarkerComponent>()) {
                auto child_texture = child.get_mut<TextureComponent>();
                auto marker_image = marker_directory->getImageByPath(child_texture->image_path);
                child_texture->textureID = marker_image.textureID;
                child_texture->size = marker_image.size;
            } });
        ecs.defer_end();

        setActiveBoard(active_board);
        std::cout << "Board loaded successfully from " << board_file_path << std::endl;
    }
    else
    {
        std::cerr << "Failed to load board from " << board_file_path << std::endl;
    }
}

flecs::entity BoardManager::getActiveBoard() const
{
    return active_board;
}

bool BoardManager::isEditWindowOpen() const
{
    return showEditWindow;
}

void BoardManager::renderEditWindow()
{
    if (!showEditWindow)
    {
        setIsNonMapWindowHovered(false);
        return; // If the window is closed, skip rendering it
    }
    bool is_hovered = false;
    // Get the current mouse position to set the window position
    ImVec2 mousePos = ImGui::GetMousePos();
    ImGui::SetNextWindowPos(mousePos, ImGuiCond_Appearing);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.2f, 0.3f, 0.4f, 1.0f)); // Set the background color (RGBA)
    ImGui::Begin("EditEntity", &showEditWindow, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
    auto edit_window_hover = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
    if (edit_window_hover)
    {
        setIsNonMapWindowHovered(true);
    }

    // Retrieve the Size and Visibility components of the entity
    is_hovered = ImGui::IsWindowHovered();
    auto is_popup_open = false;
    if (edit_window_entity.has<Size>() && edit_window_entity.has<Visibility>())
    {
        auto nm = network_manager.lock();
        auto boardEnt = getActiveBoard();

        auto size = edit_window_entity.get_mut<Size>();             // Mutable access to the size
        auto visibility = edit_window_entity.get_mut<Visibility>(); // Mutable access to the visibility

        ImGui::BeginGroup();
        if (ImGui::Button("+ Size"))
        {
            if (nm && boardEnt.is_valid())
            {
                size->width = size->width * 1.1;
                size->height = size->height * 1.1; // Adjust height proportionally to the width
                if (edit_window_entity.has<MarkerComponent>())
                {
                    nm->broadcastMarkerUpdate(boardEnt.get<Identifier>()->id, edit_window_entity);
                }
                else if (edit_window_entity.has<FogOfWar>())
                {
                    nm->broadcastFogUpdate(boardEnt.get<Identifier>()->id, edit_window_entity);
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("- Size"))
        {
            if (nm && boardEnt.is_valid())
            {
                size->width = size->width * 0.90;
                size->height = size->height * 0.90; // Adjust height proportionally to the width
                if (edit_window_entity.has<MarkerComponent>())
                {
                    nm->broadcastMarkerUpdate(boardEnt.get<Identifier>()->id, edit_window_entity);
                }
                else if (edit_window_entity.has<FogOfWar>())
                {
                    nm->broadcastFogUpdate(boardEnt.get<Identifier>()->id, edit_window_entity);
                }
            }
        }

        ImGui::EndGroup();
        // Checkbox for visibility change
        auto vis_temp = visibility->isVisible;
        if (ImGui::Checkbox("Visible", &vis_temp))
        {
            if (nm && boardEnt.is_valid())
            {
                visibility->isVisible = vis_temp;
                if (edit_window_entity.has<MarkerComponent>())
                {
                    nm->broadcastMarkerUpdate(boardEnt.get<Identifier>()->id, edit_window_entity);
                }
                else if (edit_window_entity.has<FogOfWar>())
                {
                    nm->broadcastFogUpdate(boardEnt.get<Identifier>()->id, edit_window_entity);
                }
            }
        }

        ImGui::Separator();

        // Button to delete the entity (with a confirmation popup)

        if (ImGui::Button("Delete"))
        {
            ImGui::OpenPopup("Confirm Delete");
            is_popup_open = true;
        }

        if (ImGui::IsPopupOpen("Confirm Delete"))
            is_popup_open = true;
        if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Are you sure you want to delete this entity?");
            ImGui::Separator();

            if (ImGui::Button("Yes", ImVec2(120, 0)))
            {
                if (edit_window_entity.is_alive())
                {
                    if (nm && boardEnt.is_valid())
                    {
                        if (edit_window_entity.has<MarkerComponent>())
                        {
                            nm->broadcastMarkerDelete(boardEnt.get<Identifier>()->id, edit_window_entity);
                        }
                        else if (edit_window_entity.has<FogOfWar>())
                        {
                            nm->broadcastFogDelete(boardEnt.get<Identifier>()->id, edit_window_entity);
                        }
                        edit_window_entity.destruct(); // Delete the entity
                        showEditWindow = false;
                    }
                }
                ImGui::CloseCurrentPopup(); // Close the popup after deletion
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup(); // Close the popup without deletion
            }
            ImGui::EndPopup();
        }
    }
    else
    {
        ImGui::Text("Invalid entity or missing components!");
    }
    // --- Ownership (only for markers) ---
    if (edit_window_entity.is_valid() && edit_window_entity.has<MarkerComponent>())
    {
        auto nm = network_manager.lock();
        auto mc = edit_window_entity.get_mut<MarkerComponent>();

        // Build options: index 0 = (none), then connected peers, then offline owner (if any)
        std::vector<std::string> options;
        options.emplace_back(""); // 0 => (none)

        // unique connected peers
        std::set<std::string> uniq;
        if (nm)
        {
            for (const auto& pid : nm->getConnectedPeerIds())
            {
                if (uniq.insert(pid).second)
                    options.emplace_back(pid);
            }
        }
        // include offline owner if not already in the list
        if (!mc->ownerPeerId.empty() && !uniq.count(mc->ownerPeerId))
        {
            options.emplace_back(mc->ownerPeerId);
        }

        // Find current selection
        int selectedIndex = 0;
        for (int i = 1; i < (int)options.size(); ++i)
        {
            if (options[i] == mc->ownerPeerId)
            {
                selectedIndex = i;
                break;
            }
        }

        ImGui::Separator();
        ImGui::TextUnformatted("Owner");
        ImGui::Spacing();

        // Pagination (no child, no scroll)
        static int ownerPage = 0;
        const int rowsPerPage = 6;
        const int totalRows = (int)options.size();
        const int totalPages = (totalRows + rowsPerPage - 1) / rowsPerPage;
        ownerPage = std::clamp(ownerPage, 0, std::max(0, totalPages - 1));

        // Helper: toggle-style full-width button
        auto ToggleRow = [&](const char* label, bool selected, int id) -> bool
        {
            ImGui::PushID(id);
            if (selected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.50f, 0.80f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.85f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.45f, 0.75f, 1.0f));
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.28f, 0.32f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.33f, 0.38f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.25f, 0.29f, 1.0f));
            }

            bool clicked = ImGui::Button(label, ImVec2(-FLT_MIN, 0)); // full width
            ImGui::PopStyleColor(3);
            ImGui::PopID();
            return clicked;
        };

        // Compute visible slice
        int start = ownerPage * rowsPerPage;
        int end = std::min(start + rowsPerPage, totalRows);

        // Render visible rows
        for (int i = start; i < end; ++i)
        {
            const std::string& pid = options[i];
            const bool isSel = (selectedIndex == i);
            const char* label = (i == 0) ? "(none)" : pid.c_str();
            if (ToggleRow(label, isSel, i))
            {
                selectedIndex = i;
            }
        }

        // Pagination controls (only show if needed)
        if (totalPages > 1)
        {
            ImGui::Spacing();
            ImGui::BeginDisabled(ownerPage <= 0);
            if (ImGui::Button("< Prev"))
            {
                ownerPage--;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::Text("Page %d / %d", ownerPage + 1, std::max(1, totalPages));
            ImGui::SameLine();
            ImGui::BeginDisabled(ownerPage >= totalPages - 1);
            if (ImGui::Button("Next >"))
            {
                ownerPage++;
            }
            ImGui::EndDisabled();
        }

        // Apply selection back to component
        if (selectedIndex == 0)
            mc->ownerPeerId.clear();
        else
            mc->ownerPeerId = options[selectedIndex];

        ImGui::Checkbox("Allow all players to move", &mc->allowAllPlayersMove);
        ImGui::Checkbox("Locked (players cannot move)", &mc->locked);

        if (ImGui::Button("Apply Ownership"))
        {

            auto boardEnt = getActiveBoard();
            // broadcast a full marker update (GM op)
            if (nm && boardEnt.is_valid())
            {
                nm->broadcastMarkerUpdate(boardEnt.get<Identifier>()->id, edit_window_entity);
            }
        }
    }

    ImGui::End();
    ImGui::PopStyleColor(); // Restore the original background color

    if (!is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !is_popup_open)
    {
        showEditWindow = false;
    }

    if (!showEditWindow)
    {
        edit_window_entity = flecs::entity();
    }
    //close edit window when clicking outside it
}

void BoardManager::renderCameraWindow()
{
    // Check if the window should be shown
    if (!showCameraSettings)
    {
        setIsNonMapWindowHovered(false);
        return;
    }

    // Begin the ImGui window
    auto mouse_pos = ImGui::GetMousePos();
    ImGui::SetNextWindowPos(ImVec2(mouse_pos.x, mouse_pos.y + ImGui::GetFrameHeightWithSpacing()), ImGuiCond_Appearing);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.1f, 0.2f, 1.0f)); // Set the background color (RGBA)
    ImGui::Begin("Camera", &showCameraSettings, ImGuiWindowFlags_AlwaysAutoResize);
    auto grid_hovered = ImGui::IsWindowHovered();
    if (grid_hovered)
    {
        setIsNonMapWindowHovered(true);
    }
    ImGui::PopStyleColor();
    auto zoom = camera.getZoom();

    ImGui::Text("Zoom Buttons");
    if (ImGui::Button("-"))
    {
        zoom = zoom - 0.01f;
    }
    ImGui::SameLine();
    if (ImGui::Button("+"))
    {
        zoom = zoom + 0.01f;
    }

    ImGui::Separator();
    ImGui::SliderFloat("Zoom Slider", &zoom, 0.1f, 10.0f);
    camera.setZoom(zoom);
    ImGui::Separator();
    if (ImGui::Button("Reset Camera"))
    {
        resetCamera();
    }

    ImGui::End();
}

BoardImageData BoardManager::LoadTextureFromMemory(const uint8_t* bytes, size_t sizeBytes)
{
    if (!bytes || sizeBytes == 0)
    {
        std::cerr << "LoadTextureFromMemory: empty buffer\n";
        return BoardImageData{};
    }

    // stb_image: ensure vertical flip matches your expectations
    stbi_set_flip_vertically_on_load(0);

    int width = 0, height = 0, nrChannels = 0;
    uint8_t* data = stbi_load_from_memory(bytes, (int)sizeBytes, &width, &height, &nrChannels, 4);
    if (!data)
    {
        std::cerr << "LoadTextureFromMemory: decode failed\n";
        return BoardImageData{};
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(data);
    return BoardImageData(tex, glm::vec2(width, height), /*path*/ "");
}

void BoardManager::renderGridWindow()
{
    // Check if the window should be shown
    if (!showGridSettings)
    {
        setIsNonMapWindowHovered(false);
        return;
    }

    // Begin the ImGui window
    auto mouse_pos = ImGui::GetMousePos();
    ImGui::SetNextWindowPos(ImVec2(mouse_pos.x, mouse_pos.y + ImGui::GetFrameHeightWithSpacing()), ImGuiCond_Appearing);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.1f, 0.2f, 1.0f)); // Set the background color (RGBA)
    ImGui::Begin("Grid", &showGridSettings, ImGuiWindowFlags_AlwaysAutoResize);
    auto grid_hovered = ImGui::IsWindowHovered();
    setIsNonMapWindowHovered(true);

    ImGui::PopStyleColor();
    // Get a mutable reference to the Grid component from the active board
    auto grid = active_board.get_mut<Grid>();

    if (grid)
    {
        // --- BOOLEAN CHECKBOXES ---
        ImGui::Checkbox("Visible", &grid->visible);
        ImGui::Checkbox("Snap to Grid", &grid->snap_to_grid);
        //ImGui::Checkbox("Hexagonal Grid", &grid->is_hex);

        // --- FLOAT SLIDERS ---
        ImGui::SliderFloat("Cell Size", &grid->cell_size, 10.0f, 200.0f);
        ImGui::SameLine();
        if (ImGui::Button("-"))
        {
            grid->cell_size = grid->cell_size - 0.01f;
        }
        ImGui::SameLine();
        if (ImGui::Button("+"))
        {
            grid->cell_size = grid->cell_size + 0.01f;
        }
        // --- OFFSET CONTROLS (SIMPLIFIED WITH SLIDERS) ---
        ImGui::Text("Grid Offset");
        ImGui::SliderFloat("Offset X", &grid->offset.x, -500.0f, 500.0f);
        ImGui::SameLine();
        if (ImGui::Button("-"))
        {
            grid->offset.x = grid->offset.x - 0.01f;
        }
        ImGui::SameLine();
        if (ImGui::Button("+"))
        {
            grid->offset.x = grid->offset.x + 0.01f;
        }
        ImGui::SliderFloat("Offset Y", &grid->offset.y, -500.0f, 500.0f);
        ImGui::SameLine();
        if (ImGui::Button("-"))
        {
            grid->offset.y = grid->offset.y - 0.01f;
        }
        ImGui::SameLine();
        if (ImGui::Button("+"))
        {
            grid->offset.y = grid->offset.y + 0.01f;
        }

        // Button to reset the offset
        if (ImGui::Button("Reset Offset"))
        {
            grid->offset = glm::vec2(0.0f);
        }
    }
    else
    {
        ImGui::Text("Active board entity does not have a Grid component.");
    }

    ImGui::End();
}

flecs::entity BoardManager::findBoardById(uint64_t boardId)
{
    std::cout << "Find Board By Id: " << boardId << "\n";
    flecs::entity result;
    ecs.each([&](flecs::entity e, const Board& b, const Identifier& id)
             {
    Logger::instance().log("localtunnel", Logger::Level::Info, "Board Name:  " + b.board_name);
		if (e.is_valid() && id.id == boardId) {
			result = e;
            Logger::instance().log("localtunnel", Logger::Level::Info, "Found Board By ID!!");
		} });

    return result; // will be invalid if not found
}

//glm::vec2 BoardManager::worldToScreenPosition(glm::vec2 world_position) {
//    // Step 1: Get the combined MVP matrix
//    glm::mat4 MVP = camera.getProjectionMatrix() * camera.getViewMatrix();
//
//    // Step 2: Transform world position to clip space (NDC)
//    glm::vec4 clipPos = MVP * glm::vec4(world_position, 0.0f, 1.0f);
//
//    // Step 3: Convert NDC to screen coordinates
//    glm::vec2 windowSize = camera.getWindowSize();
//    float screenX = ((clipPos.x / clipPos.w) + 1.0f) * 0.5f * windowSize.x;
//    float screenY = (1.0f - (clipPos.y / clipPos.w)) * 0.5f * windowSize.y;  // Flip Y-axis
//
//    // Step 4: Return screen position as 2D (x, y) coordinates
//
//    glm::vec2 screen_position =  glm::vec2(screenX, screenY);
//
//    return screen_position;
//}
//
//void BoardManager::sendEntityUpdate(flecs::entity entity, MessageType message_type)
//{
//    if (message_type == MessageType::MarkerUpdate) {
//        Message markerMessage;
//        markerMessage.type = MessageType::MarkerUpdate;
//        std::vector<uint8_t> buffer;
//        auto pos =  entity.get<Position>();
//        auto size =  entity.get<Size>();
//        auto texture =  entity.get<TextureComponent>();
//        auto visibility =  entity.get<Visibility>();
//        auto moving =  entity.get<Moving>();
//        // Serialize marker components
//        Serializer::serializePosition(buffer, pos);
//        Serializer::serializeSize(buffer, size);
//        Serializer::serializeTextureComponent(buffer, texture);
//        Serializer::serializeVisibility(buffer, visibility);
//        Serializer::serializeMoving(buffer, moving);
//
//        markerMessage.payload = buffer;
//        // Queue the message for the network manager
//        network_manager->queueMessage(markerMessage);
//    }
//    else if (message_type == MessageType::FogUpdate)
//    {
//        Message fogMessage;
//        fogMessage.type = MessageType::FogUpdate;
//        std::vector<uint8_t> buffer;
//
//        auto pos = entity.get<Position>();
//        auto size = entity.get<Size>();
//        auto visibility = entity.get<Visibility>();
//        // Serialize marker components
//        Serializer::serializePosition(buffer, pos);
//        Serializer::serializeSize(buffer, size);
//        Serializer::serializeVisibility(buffer, visibility);
//
//        fogMessage.payload = buffer;
//        // Queue the message for the network manager
//        network_manager->queueMessage(fogMessage);
//
//    }
//    else if (message_type == MessageType::CreateMarker)
//    {
//        Message markerMessage;
//        markerMessage.type = MessageType::MarkerUpdate;
//        std::vector<uint8_t> buffer;
//        auto pos = entity.get<Position>();
//        auto size = entity.get<Size>();
//        auto texture = entity.get<TextureComponent>();
//        auto visibility = entity.get<Visibility>();
//        auto moving = entity.get<Moving>();
//        // Serialize marker components
//        Serializer::serializePosition(buffer, pos);
//        Serializer::serializeSize(buffer, size);
//        Serializer::serializeTextureComponent(buffer, texture);
//        Serializer::serializeVisibility(buffer, visibility);
//        Serializer::serializeMoving(buffer, moving);
//
//        markerMessage.payload = buffer;
//        // Queue the message for the network manager
//        network_manager->queueMessage(markerMessage);
//
//    }
//    else if (message_type == MessageType::CreateFog)
//    {
//        Message fogMessage;
//        fogMessage.type = MessageType::FogUpdate;
//        std::vector<uint8_t> buffer;
//
//        auto pos = entity.get<Position>();
//        auto size = entity.get<Size>();
//        auto visibility = entity.get<Visibility>();
//        // Serialize marker components
//        Serializer::serializePosition(buffer, pos);
//        Serializer::serializeSize(buffer, size);
//        Serializer::serializeVisibility(buffer, visibility);
//
//        fogMessage.payload = buffer;
//        // Queue the message for the network manager
//        network_manager->queueMessage(fogMessage);
//    }
//}

//
//glm::vec2 BoardManager::screenToWorldPosition(glm::vec2 screen_position) {
//
//    glm::vec2 relative_screen_position = { screen_position.x - camera.getWindowPosition().x, screen_position.y - camera.getWindowPosition().y};
//
//    // Get the view matrix (which handles panning and zoom)
//    glm::mat4 view_matrix = camera.getViewMatrix();
//
//    // Get the projection matrix (which might handle window size)
//    glm::mat4 proj_matrix = camera.getProjectionMatrix();
//
//    // Create a normalized device coordinate from the screen position
//    glm::vec2 window_size = camera.getWindowSize();
//
//    // Convert the screen position to normalized device coordinates (NDC)
//    float ndc_x = (2.0f * relative_screen_position.x) / window_size.x - 1.0f;
//    float ndc_y = 1.0f - (2.0f * relative_screen_position.y) / window_size.y; // Inverting y-axis for OpenGL
//    glm::vec4 ndc_position = glm::vec4(ndc_x, ndc_y, 0.0f, 1.0f);
//
//    // Calculate the inverse MVP matrix (to map from NDC back to world space)
//    glm::mat4 mvp = proj_matrix * view_matrix;
//    glm::mat4 inverse_mvp = glm::inverse(mvp);
//
//    // Transform the NDC position back to world coordinates
//    glm::vec4 world_position = inverse_mvp * ndc_position;
//
//    // Perform perspective divide to get the correct world position
//    if (world_position.w != 0.0f) {
//        world_position /= world_position.w;
//    }
//
//    // Return the world position as a 2D vector (we're ignoring the Z-axis for 2D rendering)
//    return glm::vec2(world_position.x, world_position.y);
//}

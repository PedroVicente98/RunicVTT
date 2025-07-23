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
#include "Components.h"
#include <filesystem>
#include <random>    // For random number generation
#include <atomic>    // For atomic counter
#include <cstdint>   // For uint64_t and UINT64_MAX
#include "Serializer.h"

BoardManager::BoardManager(flecs::world ecs,/* NetworkManager* network_manager,*/ std::shared_ptr<DirectoryWindow> map_directory, std::shared_ptr<DirectoryWindow> marker_directory)
    : ecs(ecs), camera(), currentTool(Tool::MOVE), mouseStartPos({0,0}), marker_directory(marker_directory), map_directory(map_directory)/*, network_manager(network_manager)*/ {
    
    
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

void BoardManager::closeBoard() {
    active_board = flecs::entity();
}

flecs::entity BoardManager::createBoard(std::string board_name, std::string map_image_path, GLuint texture_id, glm::vec2 size)
{   
    //Texture texture = Texture(map_image_path);
    auto board = ecs.entity()
        .set(Identifier{ generateUniqueId() })
        .set(Board{ board_name })
        .set(Panning{false})
        .set(Position{0,0})
        .set(Grid{ {0.0f,0.0f},{1.0f,1.0f} })
        .set(TextureComponent{ texture_id, map_image_path, size})
        .set(Size{ size.x, size.y });
    active_board = board;

    //Create FILE

    return board;
}


void BoardManager::setActiveBoard(flecs::entity board_entity)
{
    active_board = board_entity;
}

void BoardManager::renderToolbar() {

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse 
        | ImGuiWindowFlags_NoDocking 
        | ImGuiWindowFlags_NoMove 
        | ImGuiWindowFlags_NoResize 
        | ImGuiWindowFlags_NoScrollbar 
        | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_AlwaysAutoResize;
    auto window_position = camera.getPosition(); ///
    auto offset = ImGui::GetFrameHeight();
    ImGui::SetNextWindowPos(ImVec2(window_position.x, window_position.y + offset), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.2f, 0.3f, 0.4f, 1.0f)); // Set the background color (RGBA)

    ImGui::Begin("Toolbar", 0, window_flags);
    ImVec4 defaultColor = ImGui::GetStyleColorVec4(ImGuiCol_Button);
    ImVec4 activeColor = ImVec4(0.2f, 0.7f, 1.0f, 1.0f); // A custom color to highlight the active tool
    // Tool: Move
    ImGui::PushStyleColor(ImGuiCol_Button, currentTool == Tool::MOVE ? activeColor : defaultColor);
    if (ImGui::Button("Move Tool", ImVec2(80, 40))) {
        currentTool = Tool::MOVE;
    }
    ImGui::PopStyleColor();
    ImGui::SameLine(); // Ensure buttons are on the same row

    //if (network_manager->getPeerRole() == Role::GAMEMASTER) {
        // Tool: Fog
        ImGui::PushStyleColor(ImGuiCol_Button, currentTool == Tool::FOG ? activeColor : defaultColor);
        if (ImGui::Button("Fog Tool", ImVec2(80, 40))) {
            currentTool = Tool::FOG;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine(); // Ensure buttons are on the same row
        //// Tool: Marker
        //ImGui::PushStyleColor(ImGuiCol_Button, currentTool == Tool::MARKER ? activeColor : defaultColor);
        //if (ImGui::Button("Marker Tool", ImVec2(80, 40))) {
        //    currentTool = Tool::MARKER;
        //}
        //ImGui::PopStyleColor();
        //ImGui::SameLine(); // Ensure buttons are on the same row
        // Tool: Select
        ImGui::PushStyleColor(ImGuiCol_Button, currentTool == Tool::SELECT ? activeColor : defaultColor);
        if (ImGui::Button("Select Tool", ImVec2(80, 40))) {
            currentTool = Tool::SELECT;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine(); // Ensure buttons are on the same row
    //}

    if (ImGui::Button("Reset Camera", ImVec2(90, 40))) {
        resetCamera();
    }

    ImGui::End();
    ImGui::PopStyleColor();
}

void BoardManager::renderBoard(VertexArray& va, IndexBuffer& ib, Shader& shader, Renderer& renderer) {
    
    const TextureComponent* texture = active_board.get<TextureComponent>();
    if (texture->textureID != 0) {
        const Board* board = active_board.get<Board>();
        const Panning* panning = active_board.get<Panning>();
        const Position* position = active_board.get<Position>();
        const Grid* grid = active_board.get<Grid>();
        const Size* size = active_board.get<Size>();

        /*ImVec2 window_size = ImGui::GetWindowSize();
        ImVec2 window_position = ImGui::GetWindowPos();
        camera.setWindowSize(glm::vec2(window_size.x, window_size.y));
        camera.setWindowPosition(glm::vec2(window_position.x, window_position.y));*/

        glm::mat4 viewMatrix = camera.getViewMatrix();  // Obtém a matriz de visualização da câmera (pan/zoom)
        glm::mat4 projection = camera.getProjectionMatrix();

        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position->x, position->y, 0.0f));
        model = glm::scale(model, glm::vec3(size->width, size->height, 1.0f));

        glm::mat4 mvp = projection * viewMatrix * model; //Calculate Screen Position(Can use method to standize it, but alter to return the MVP

        shader.Bind();
        shader.SetUniformMat4f("u_MVP", mvp);
        shader.SetUniform1f("u_Alpha", 1.0f);
        shader.SetUniform1f("u_UseTexture", 1);
        shader.SetUniform1i("u_Texture", 0);
        shader.Unbind();


        GLCall(glActiveTexture(GL_TEXTURE0)); //https://learnopengl.com/Getting-started/Coordinate-Systems REDO THE SHADER FOR THE COORDINATE SYSTEM, MAKE MORE READABLE AND KEEP THE MATRIXES SEPARATE IN THE UNIFORMS
        GLCall(glBindTexture(GL_TEXTURE_2D, texture->textureID));

        renderer.Draw(va, ib, shader);

        ecs.defer_begin(); // Start deferring modifications
        active_board.children([&](flecs::entity child) {
            //if (child.has<MarkerComponent>()) {
            if (child.has<MarkerComponent>()) {
                const TextureComponent* texture_marker = child.get<TextureComponent>();
                if (texture_marker->textureID != 0) {
                    const Position* position_marker = child.get<Position>();
                    const Visibility* visibility_marker = child.get<Visibility>();
                    const Size* size_marker = child.get<Size>();

                    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position_marker->x, position_marker->y, 0.0f));
                    model = glm::scale(model, glm::vec3(size_marker->width, size_marker->height, 1.0f));

                    glm::mat4 mvp = projection * viewMatrix * model; //Calculate Screen Position(Can use method to standize it, but alter to return the MVP
                    float alpha = 1.0f;
                    if (!visibility_marker->isVisible) {
                        //if (network_manager->getPeerRole() == Role::GAMEMASTER) {
                            alpha = 0.5f;
                        //}
                        //else {
                            //alpha = 0.0f;
                        //}
                    }

                    shader.Bind();
                    shader.SetUniformMat4f("u_MVP", mvp);
                    shader.SetUniform1f("u_Alpha", alpha);
                    shader.SetUniform1i("u_Texture", 0);
                    shader.SetUniform1f("u_UseTexture", 1);
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

                glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position_marker->x, position_marker->y, 0.0f));
                model = glm::scale(model, glm::vec3(size_marker->width, size_marker->height, 1.0f));

                glm::mat4 mvp = projection * viewMatrix * model; //Calculate Screen Position(Can use method to standize it, but alter to return the MVP
                float alpha = 1.0f;
                if (!visibility_marker->isVisible) {
                    //if (network_manager->getPeerRole() == Role::GAMEMASTER) {
                        alpha = 0.3f;
                    //}
                    //else
                    //{
                        //alpha = 0.0f;
                    //}
                }

                shader.Bind();
                shader.SetUniformMat4f("u_MVP", mvp);
                shader.SetUniform1f("u_Alpha", alpha);
                shader.SetUniform1i("u_UseTexture", 0);
                shader.Unbind();

                renderer.Draw(va, ib, shader);

            }
        });
        ecs.defer_end();
    }

}


flecs::entity BoardManager::createMarker(const std::string& imageFilePath, GLuint textureId, glm::vec2 position, glm::vec2 size) {

    flecs::entity marker = ecs.entity()
        .set(Identifier{ generateUniqueId() })
        .set(Position{ (int)position.x, (int)position.y }) //World Position
        .set(Size{ size.x , size.y })
        .set(TextureComponent{ textureId , imageFilePath, size })
        .set(Visibility{ true })
        .set(Moving{ false });

    marker.add<MarkerComponent>();
    marker.add(flecs::ChildOf, active_board);

    //auto message = network_manager->buildCreateMarkerMessage(marker);
    //network_manager->queueMessage(message);
    return marker;
}

void BoardManager::deleteMarker(flecs::entity markerEntity) {
    markerEntity.destruct();
}


void BoardManager::handleMarkerDragging(glm::vec2 mousePos) {
    //mousePos(Screen Position) use screenToWorldPosition(mousePos)  position(World Position) 
    ecs.defer_begin();
    ecs.each([&](flecs::entity entity, const MarkerComponent& marker, Moving& moving, Position& position) {
        if (entity.has(flecs::ChildOf, active_board) && moving.isDragging) {
            glm::vec2 world_position = camera.screenToWorldPosition(mousePos);
            glm::vec2 start_world_position = camera.screenToWorldPosition(mouseStartPos);
            glm::vec2 delta = world_position - start_world_position;
            position.x += delta.x;
            position.y += delta.y;
            mouseStartPos = mousePos;

            //auto message = network_manager->buildUpdateMarkerMessage(entity);
            //network_manager->queueMessage(message);
        }
     });
    ecs.defer_end();
}

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

// Generates a unique 64-bit ID
uint64_t BoardManager::generateUniqueId() {
    static std::atomic<uint64_t> counter{0};            // Atomic counter for thread safety
    static std::mt19937_64 rng(std::random_device{}()); // Random number generator
    static std::uniform_int_distribution<uint64_t> dist(1, UINT64_MAX);

    uint64_t random_part = dist(rng);                   // Generate a random 64-bit number
    uint64_t unique_id = (random_part & 0xFFFFFFFFFFFF0000) | (counter++ & 0xFFFF);  // Combine random and counter

    return unique_id;
}

// Finds an entity by its Identifier component with the specified ID
flecs::entity BoardManager::findEntityById(uint64_t target_id) {
    flecs::entity result;

    // Iterate over all entities with Identifier component
    ecs.each<Identifier>([&](flecs::entity e, Identifier& identifier) {
        if (identifier.id == target_id) {
            result = e;  // Store matching entity
        }
    });

    return result;  // Returns the found entity, or an empty entity if not found
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


bool BoardManager::isMouseOverMarker(glm::vec2 mousePos) {
    bool hovered = false;
    
    // Query all markers that are children of the active board and have MarkerComponent
    ecs.defer_begin();
    ecs.each([&](flecs::entity entity, const MarkerComponent& marker, const Position& markerPos, const Size& markerSize, Moving& moving) {
        glm::vec2 world_position = camera.screenToWorldPosition(mousePos);


        if (entity.has(flecs::ChildOf, active_board)) {
            bool withinXBounds = (world_position.x >= (markerPos.x - markerSize.width / 2)) &&
                (world_position.x <= (markerPos.x + markerSize.width / 2));

            bool withinYBounds = (world_position.y >= (markerPos.y - markerSize.height / 2)) &&
                (world_position.y <= (markerPos.y + markerSize.height / 2));

            if (withinXBounds && withinYBounds) {
                moving.isDragging = true;
                hovered = true;           // Mark as hovered
            }
        }
    });
    ecs.defer_end();
    return hovered;
}


void BoardManager::startMouseDrag(glm::vec2 mousePos, bool draggingMap) {
    mouseStartPos = mousePos;  // Captura a posição inicial do mouse
    if (currentTool == Tool::MOVE) {
        if (draggingMap) {
            active_board.set<Panning>({ true });
        }
    }
    else if (currentTool == Tool::FOG)
    {
        is_creating_fog = true;
    }
}



void BoardManager::endMouseDrag() {
    active_board.set<Panning>({ false });
    ecs.defer_begin();
    ecs.each([&](flecs::entity entity, const MarkerComponent& marker, Moving& moving) {
        if (entity.has(flecs::ChildOf, active_board)) {
            moving.isDragging = false;
        }
    });
    ecs.defer_end();
    is_creating_fog = false;
}

bool BoardManager::isPanning() {
    const Panning* panning = active_board.get<Panning>();
    return panning->isPanning;
}

bool BoardManager::isDragginMarker() {
    bool isDragginMarker = false;
    ecs.defer_begin();
    ecs.each([&](flecs::entity entity, const MarkerComponent& marker, Moving& moving) {
        if (entity.has(flecs::ChildOf, active_board) && moving.isDragging) {
            isDragginMarker =  true;
        }
    });
    ecs.defer_end();
    return isDragginMarker;
}

glm::vec2 BoardManager::getMouseStartPosition() const {
    return mouseStartPos;
}

void BoardManager::panBoard(glm::vec2 currentMousePos) {
    glm::vec2 delta =  mouseStartPos - currentMousePos;
    camera.pan(delta);
    mouseStartPos = currentMousePos;
}


void BoardManager::resetCamera() {
    camera.setPosition(glm::vec2(0.0f, 0.0f));
    camera.setZoom(1.0f);
}

void BoardManager::deleteFogOfWar(flecs::entity fogEntity) {
    fogEntity.destruct();
}

flecs::entity BoardManager::createFogOfWar(glm::vec2 startPos, glm::vec2 size) {
    auto fog = ecs.entity()
        .set(Identifier{ generateUniqueId() })
        .set(Position{ (int)startPos.x, (int)startPos.y })
        .set(Size{ size.x, size.y })
        .set(Visibility{ true });

    fog.add<FogOfWar>();
    fog.add(flecs::ChildOf, active_board);
    return fog;
}

void BoardManager::handleFogCreation(glm::vec2 mousePos) {
    glm::vec2 startPos = getMouseStartPosition();  // Tracks the starting drag point
    glm::vec2 start_world_position = camera.screenToWorldPosition(startPos);
    glm::vec2 end_world_position = camera.screenToWorldPosition(mousePos);

    // Calculate size
    glm::vec2 size = glm::abs(end_world_position - start_world_position);  // Make sure size is positive

    // Determine the corrected top-left position for the fog
    glm::vec2 corrected_start_position;
    corrected_start_position.x = start_world_position.x + size.x/2;
    corrected_start_position.y = start_world_position.y + size.y / 2;

    createFogOfWar(corrected_start_position, size);
}

Tool BoardManager::getCurrentTool() const {
    return currentTool;
}

void BoardManager::setCurrentTool(Tool newTool) {
    currentTool = newTool;
}


flecs::entity BoardManager::getEntityAtMousePosition(glm::vec2 mouse_position) {

    auto entity_at_mouse = flecs::entity();
    ecs.defer_begin();
    ecs.each([&](flecs::entity entity, const Position& entity_pos, const Size& entity_size) {
        
        glm::vec2 world_position = camera.screenToWorldPosition(mouse_position);
        
        if (entity.has(flecs::ChildOf, active_board)) {

            bool withinXBounds = (world_position.x >= (entity_pos.x - entity_size.width / 2)) &&
                (world_position.x <= (entity_pos.x + entity_size.width / 2));

            bool withinYBounds = (world_position.y >= (entity_pos.y - entity_size.height / 2)) &&
                (world_position.y <= (entity_pos.y + entity_size.height / 2));

            if (withinXBounds && withinYBounds) {
                entity_at_mouse = entity;
            }
        }
    });
    ecs.defer_end();
    return entity_at_mouse;
}

//
//void BoardManager::sendEntityUpdate(flecs::entity entity, MessageType message_type) 
//{
//    if (message_type == MessageType::MarkerUpdate) {
//        Message markerMessage;
//        markerMessage.type = MessageType::MarkerUpdate;
//        std::vector<unsigned char> buffer;
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
//        std::vector<unsigned char> buffer;
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
//        std::vector<unsigned char> buffer;
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
//        std::vector<unsigned char> buffer;
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

//Save and Load Board --------------------------------------------------------------------

void BoardManager::saveActiveBoard(std::filesystem::path& filePath) {
    if (!active_board.is_alive()) {
        std::cerr << "No active board to save." << std::endl;
        return;
    }
    auto board = active_board.get<Board>();
    if (!std::filesystem::exists(filePath)) {
        std::filesystem::create_directory(filePath);
    }

    auto board_file_path = filePath / (board->board_name + ".runic");

    std::vector<unsigned char> buffer;
    Serializer::serializeBoardEntity(buffer, active_board, ecs);

    std::ofstream outFile(board_file_path, std::ios::binary);
    if (outFile) {
        outFile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        outFile.close();
        std::cout << "Board saved successfully to " << filePath << std::endl;
    }
    else {
        std::cerr << "Failed to save board to " << filePath << std::endl;
    }
}

void BoardManager::loadActiveBoard(const std::string& filePath) {
    std::ifstream inFile(filePath, std::ios::binary);
    if (inFile) {
        std::vector<unsigned char> buffer((std::istreambuf_iterator<char>(inFile)), std::istreambuf_iterator<char>());
        inFile.close();

        size_t offset = 0;
        active_board = Serializer::deserializeBoardEntity(buffer, offset, ecs);
        auto texture = active_board.get_mut<TextureComponent>();
        auto map_image = map_directory->getImageByPath(texture->image_path);
        texture->textureID = map_image.textureID;
        texture->size = map_image.size;

        ecs.defer_begin();
        active_board.children([&](flecs::entity child) {
            if (child.has<MarkerComponent>()) {
                auto child_texture = child.get_mut<TextureComponent>();
                auto marker_image = marker_directory->getImageByPath(child_texture->image_path);
                child_texture->textureID = marker_image.textureID;
                child_texture->size = marker_image.size;
            }
        });
        ecs.defer_end();

        std::cout << "Board loaded successfully from " << filePath << std::endl;
    }
    else {
        std::cerr << "Failed to load board from " << filePath << std::endl;
    }
}


flecs::entity BoardManager::getActiveBoard() const {
    return active_board;
}


bool BoardManager::isEditWindowOpen() const {
    return showEditWindow;
}

void BoardManager::renderEditWindow() {
    if (!showEditWindow) return;  // If the window is closed, skip rendering it

    // Get the current mouse position to set the window position
    ImVec2 mousePos = ImGui::GetMousePos();
    ImGui::SetNextWindowPos(mousePos, ImGuiCond_Appearing);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.2f, 0.3f, 0.4f, 1.0f)); // Set the background color (RGBA)
    ImGui::Begin("EditEntity", &showEditWindow, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
    // Retrieve the Size and Visibility components of the entity
    if (edit_window_entity.has<Size>() && edit_window_entity.has<Visibility>()) {
        auto size = edit_window_entity.get_mut<Size>();  // Mutable access to the size
        auto visibility = edit_window_entity.get_mut<Visibility>();  // Mutable access to the visibility

        ImGui::BeginGroup();
        if (ImGui::Button("+ Size")) {
            size->width = size->width * 1.1;
            size->height = size->height * 1.1;  // Adjust height proportionally to the width
        }
        ImGui::SameLine();
        if (ImGui::Button("- Size")) {
            size->width = size->width * 0.90;
            size->height = size->height * 0.90;  // Adjust height proportionally to the width
        }

        ImGui::EndGroup();
        // Checkbox for visibility change
        ImGui::Checkbox("Visible", &visibility->isVisible);

        ImGui::Separator();

        // Button to delete the entity (with a confirmation popup)
        if (ImGui::Button("Delete")) {
            ImGui::OpenPopup("Confirm Delete");
        }
        // Confirm delete popup
        if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to delete this entity?");
            ImGui::Separator();

            if (ImGui::Button("Yes", ImVec2(120, 0))) {
                if (edit_window_entity.is_alive()) {
                    edit_window_entity.destruct();  // Delete the entity
                    showEditWindow = false;
                }
                ImGui::CloseCurrentPopup();  // Close the popup after deletion
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();  // Close the popup without deletion
            }
            ImGui::EndPopup();
        }
    } else {
        ImGui::Text("Invalid entity or missing components!");
    }

    ImGui::End();
    ImGui::PopStyleColor(); // Restore the original background color

    if (!showEditWindow) {
        edit_window_entity = flecs::entity();
    }
}


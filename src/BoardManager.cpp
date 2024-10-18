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

BoardManager::BoardManager(flecs::world ecs)
    : ecs(ecs), camera(), currentTool(Tool::MOVE), mouseStartPos({0,0}), marker_directory(std::string(), std::string()){
    
    std::filesystem::path base_path = std::filesystem::current_path();
    std::filesystem::path marker_directory_path = base_path / "res" / "markers";

    marker_directory.directoryName = "MarkerDiretory";
    marker_directory.directoryPath = marker_directory_path.string();
    marker_directory.startMonitoring();

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
        .set(Board{ board_name })
        .set(Panning{false})
        .set(Zoom{1.0f})
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
    ImVec4 defaultColor = ImGui::GetStyleColorVec4(ImGuiCol_Button);
    ImVec4 activeColor = ImVec4(0.2f, 0.7f, 1.0f, 1.0f); // A custom color to highlight the active tool
    // Tool: Move
    ImGui::PushStyleColor(ImGuiCol_Button, currentTool == Tool::MOVE ? activeColor : defaultColor);
    if (ImGui::Button("Move Tool", ImVec2(80, 40))) {
        currentTool = Tool::MOVE;
    }
    ImGui::PopStyleColor();
    ImGui::SameLine(); // Ensure buttons are on the same row
    // Tool: Fog
    ImGui::PushStyleColor(ImGuiCol_Button, currentTool == Tool::FOG ? activeColor : defaultColor);
    if (ImGui::Button("Fog Tool", ImVec2(80, 40))) {
        currentTool = Tool::FOG;
    }
    ImGui::PopStyleColor();
    ImGui::SameLine(); // Ensure buttons are on the same row
    // Tool: Marker
    ImGui::PushStyleColor(ImGuiCol_Button, currentTool == Tool::MARKER ? activeColor : defaultColor);
    if (ImGui::Button("Marker Tool", ImVec2(80, 40))) {
        currentTool = Tool::MARKER;
    }
    ImGui::PopStyleColor();
    ImGui::SameLine(); // Ensure buttons are on the same row
    // Tool: Select
    ImGui::PushStyleColor(ImGuiCol_Button, currentTool == Tool::SELECT ? activeColor : defaultColor);
    if (ImGui::Button("Select Tool", ImVec2(80, 40))) {
        currentTool = Tool::SELECT;
    }
    ImGui::PopStyleColor();
}

void BoardManager::renderBoard(VertexArray& va, IndexBuffer& ib, Shader& shader, Renderer& renderer) {

    const Board* board = active_board.get<Board>();
    const Panning* panning = active_board.get<Panning>();
    const Zoom* zoom = active_board.get<Zoom>();
    const Position* position = active_board.get<Position>();
    const Grid* grid = active_board.get<Grid>();
    const TextureComponent* texture = active_board.get<TextureComponent>();
    const Size* size = active_board.get<Size>();
        
    ImVec2 window_size = ImGui::GetWindowSize();
    camera.setWindowSize(glm::vec2(window_size.x, window_size.y));

    glm::mat4 viewMatrix = camera.getViewMatrix();  // Obtém a matriz de visualização da câmera (pan/zoom)
    glm::mat4 projection = camera.getProjectionMatrix();

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position->x, position->y, 0.0f));
    model = glm::scale(model, glm::vec3(size->width, size->height, 1.0f));

    glm::mat4 mvp = projection * viewMatrix * model;


    shader.Bind();
   /* shader.SetUniformMat4f("u_Projection", projection);
    shader.SetUniformMat4f("u_View", viewMatrix);
    shader.SetUniformMat4f("u_Model", model);*/
    shader.SetUniformMat4f("u_MVP", mvp);
    shader.SetUniform1f("u_Alpha", 1.0f);
    shader.SetUniform1i("u_Texture", 0);
    shader.Unbind();


    GLCall(glActiveTexture(GL_TEXTURE0)); //https://learnopengl.com/Getting-started/Coordinate-Systems REDO THE SHADER FOR THE COORDINATE SYSTEM, MAKE MORE READABLE AND KEEP THE MATRIXES SEPARATE IN THE UNIFORMS
    GLCall(glBindTexture(GL_TEXTURE_2D, texture->textureID));

    renderer.Draw(va, ib, shader);

    active_board.children([&](flecs::entity child){
        if (child.has<MarkerComponent>()) {
            const MarkerComponent* marker_component = child.get<MarkerComponent>();
            const Moving* moving_marker = child.get<Moving>();
            const Position* position_marker = child.get<Position>();
            const Visibility* visibility_marker = child.get<Visibility>();
            const TextureComponent* texture_marker = child.get<TextureComponent>();
            const Size* size_marker = child.get<Size>();

            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position_marker->x, position_marker->y, 0.0f));
            model = glm::scale(model, glm::vec3(size_marker->width, size_marker->height, 1.0f));
            glm::mat4 mvp = projection * viewMatrix * model;
            camera.setWindowSize(glm::vec2(window_size.x, window_size.y));

            shader.Bind();
            shader.SetUniformMat4f("u_MVP", mvp);
            //shader.SetUniformMat4f("u_Projection", projection);
            //shader.SetUniformMat4f("u_View", viewMatrix);
            //shader.SetUniformMat4f("u_Model", model);
            shader.SetUniform1f("u_Alpha", 1.0f);
            shader.SetUniform1i("u_Texture", 0);
            shader.Unbind();

            GLCall(glActiveTexture(GL_TEXTURE0));
            GLCall(glBindTexture(GL_TEXTURE_2D, texture_marker->textureID));

            renderer.Draw(va, ib, shader);
        }
    });


    ////RENDER GRID

    //// Renderizar os marcadores
    //ecs.each([&](flecs::entity e, const MarkerComponent& marker, const Position& pos, const Size& size, const TextureComponent& texture, const Visibility& visibility) {
    //    if (visibility.isVisible) {
    //        renderMarker(texture.textureID, pos, size, viewMatrix);  // Renderizar o marcador
    //    }
    //    });
    //// Renderizar a névoa de guerra (Fog of War)
    //ecs.each([&](flecs::entity e, const FogOfWar& fog, const Position& pos, const Size& size, const Visibility& visibility) {
    //    if (visibility.isVisible) {
    //        renderFog(pos, size, viewMatrix, 1.0f);  // Renderizar a névoa com transparência
    //    }
    //    });
}

//// Render Marker
//void BoardManager::renderMarker(GLuint textureID, const Position& pos, const Size& size, glm::mat4& viewMatrix) {
//    shader.Bind();
//    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, 0.0f));
//    model = glm::scale(model, glm::vec3(size.width, size.height, 1.0f));
//    glm::mat4 mvp = viewMatrix * model;
//    shader.SetUniformMat4f("u_MVP", mvp);
//    shader.SetUniform1i("u_Texture", 0);
//    shader.SetUniform1i("useTexture", 1);
//    Texture markerTexture("");  // Assuming you have the path to the marker's texture
//    markerTexture.Bind();
//    Renderer renderer;
//    renderer.Draw(vertexArray, indexBuffer, shader);
//    markerTexture.Unbind();
//    shader.Unbind();
//}

//// Render Fog with transparency
//void BoardManager::renderFog(const Position& pos, const Size& size, const glm::mat4& viewMatrix, const float alpha) {
//    shader.Bind();
//    shader.SetUniform1i("useTexture", 0);
//    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, 0.0f));
//    model = glm::scale(model, glm::vec3(size.width, size.height, 1.0f));
//    glm::mat4 mvp = viewMatrix * model;
//    shader.SetUniformMat4f("u_MVP", mvp);
//    shader.SetUniform1f("u_Alpha", alpha);  // Set transparency for fog
//
//    Renderer renderer;
//    renderer.Draw(vertexArray, indexBuffer, shader);
//    shader.Unbind();
//}

flecs::entity BoardManager::createMarker(const std::string& imageFilePath, GLuint textureId, glm::vec2 position, glm::vec2 size) {

    flecs::entity marker = ecs.entity()
        .set(MarkerComponent{ false })
        .set(Position{ (int)position.x, (int)position.y }) //World Position
        .set(Size{ size.x , size.y })
        .set(TextureComponent{ textureId , imageFilePath, size })
        .set(Visibility{ true })
        .set(Moving{ false });

    marker.add(flecs::ChildOf, active_board);

    return marker;
}

void BoardManager::deleteMarker(flecs::entity markerEntity) {
    markerEntity.destruct();
}


void BoardManager::handleMarkerDragging(glm::vec2 mousePos) {
    auto position = hovered_marker->get_mut<Position>();
    position->x = mousePos.x;
    position->y = mousePos.y;
}

//TODO
//ANALISAR AS COORDENADAS NO CODIGO, DEFINIR CADA TIPO DE COODENADA QUANDO SALVO(WORLD OR SCREEN) 
//Ver todos os lugares que usam e revisar, ver os componentes que tem positions e sizes, ver o que ta sendo usado e como
//Fazer camada que transforma coordenadas quando interagindo com o mundo(para os Marker e FogOfWar


glm::vec2 BoardManager::screenToWorldPosition(glm::vec2 screen_position) {
    // Step 1: Get window size
    glm::vec2 windowSize = camera.getWindowSize();

    // Step 2: Normalize screen coordinates to NDC (Normalized Device Coordinates)
    float ndcX = (2.0f * screen_position.x) / windowSize.x - 1.0f;
    float ndcY = 1.0f - (2.0f * screen_position.y) / windowSize.y;  // Invert Y-axis

    // Step 3: Create NDC position vector (z = 0, w = 1)
    glm::vec4 ndcPos = glm::vec4(ndcX, ndcY, 0.0f, 1.0f);

    // Step 4: Get the combined MVP matrix and calculate its inverse
    glm::mat4 MVP = camera.getProjectionMatrix() * camera.getViewMatrix();
    glm::mat4 inverseMVP = glm::inverse(MVP);

    // Step 5: Transform NDC to world coordinates
    glm::vec4 world_position = inverseMVP * ndcPos;

    // Step 6: Return the world position as 2D (x, y) coordinates (ignore z)
    return glm::vec2(world_position.x, world_position.y);
}


glm::vec2 BoardManager::worldToScreenPosition(glm::vec2 world_position) {
    // Step 1: Get the combined MVP matrix
    glm::mat4 MVP = camera.getProjectionMatrix() * camera.getViewMatrix();

    // Step 2: Transform world position to clip space (NDC)
    glm::vec4 clipPos = MVP * glm::vec4(world_position, 0.0f, 1.0f);

    // Step 3: Convert NDC to screen coordinates
    glm::vec2 windowSize = camera.getWindowSize();
    float screenX = ((clipPos.x / clipPos.w) + 1.0f) * 0.5f * windowSize.x;
    float screenY = (1.0f - (clipPos.y / clipPos.w)) * 0.5f * windowSize.y;  // Flip Y-axis

    // Step 4: Return screen position as 2D (x, y) coordinates
    glm::vec2 screen_position =  glm::vec2(screenX, screenY);

    return screen_position;
}


bool BoardManager::isMouseOverMarker(glm::vec2 mousePos) {
    bool hovered = false;
    
    // Query all markers that are children of the active board and have MarkerComponent
    ecs.each([&](flecs::entity entity, const MarkerComponent& marker, const Position& markerPos, const Size& markerSize) {
        // Check if the marker is a child of the active board
        if (entity.has(flecs::ChildOf, active_board)) {
            bool withinXBounds = mousePos.x >= markerPos.x && mousePos.x <= (markerPos.x + markerSize.width);
            bool withinYBounds = mousePos.y >= markerPos.y && mousePos.y <= (markerPos.y + markerSize.height);

            if (withinXBounds && withinYBounds) {
                hovered_marker = &entity;  // Set the hovered marker
                hovered = true;           // Mark as hovered
            }
        }
    });

    return hovered;
}


void BoardManager::startMouseDrag(glm::vec2 mousePos, bool draggingMarker) {
    mouseStartPos = mousePos;  // Captura a posição inicial do mouse
    if (draggingMarker) {
        hovered_marker->set<Moving>({ true });
    }
    else {
        active_board.set<Panning>({ true });
    }

}


void BoardManager::endMouseDrag() {
    active_board.set<Panning>({ false });
    hovered_marker = nullptr;
}

bool BoardManager::isPanning() {
    const Panning* panning = active_board.get<Panning>();
    return panning->isPanning;
}

bool BoardManager::isDragginMarker() {
    return hovered_marker != nullptr;
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
}


//void BoardManager::renderGrid(const glm::mat4& viewMatrix, float windowWidth, float windowHeight) {
//    ecs.each([&](flecs::entity entity, const Grid& grid) {
//        glm::mat4 mvp = viewMatrix;  // Use viewMatrix as part of the MVP
//
//        if (grid.is_hex) {
//            renderHexGrid(mvp, windowWidth, windowHeight, grid);
//        }
//        else {
//            renderSquareGrid(mvp, windowWidth, windowHeight, grid);
//        }
//        });
//}


flecs::entity BoardManager::createFogOfWar(glm::vec2 startPos, glm::vec2 size) {
    auto fog = ecs.entity()
        .set(FogOfWar{ false })
        .set(Position{ (int)startPos.x, (int)startPos.y })
        .set(Size{ size.x, size.y })
        .set(Visibility{ true });
    fog.add(flecs::ChildOf, active_board);
    return fog;
}

void BoardManager::deleteFogOfWar(flecs::entity fogEntity) {
    fogEntity.destruct();
}

void BoardManager::handleFogCreation(glm::vec2 mousePos) {
    glm::vec2 startPos = getMouseStartPosition();  // Assuming this tracks the starting drag point
    glm::vec2 size = mousePos - startPos;
    createFogOfWar(startPos, size);
}


void BoardManager::zoomBoard(float zoomFactor) {
    camera.zoom(zoomFactor);
}

Tool BoardManager::getCurrentTool() const {
    return currentTool;
}

void BoardManager::setCurrentTool(Tool newTool) {
    currentTool = newTool;
}

void BoardManager::renderEditWindow(flecs::entity entity) {
    // Get the current mouse position to set the window position
    ImVec2 mousePos = ImGui::GetMousePos();
    // Set the cursor position to the current mouse position
    ImGui::SetNextWindowPos(mousePos, ImGuiCond_Always);
    // Define the window title (you can customize this as needed)
    ImGui::Begin("Edit Entity", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);
    // Retrieve the Size and Visibility components of the entity
    if (entity.has<Size>() && entity.has<Visibility>()) {
        auto size = entity.get_mut<Size>();  // Mutable access to the size
        auto visibility = entity.get_mut<Visibility>()->isVisible;  // Mutable access to the visibility
        float scale = 1.0f;
        // Slider for size change (adjust range as needed)
        if (ImGui::SliderFloat("Size", &scale, 0.1f, 10.0f, "%.1fx")) {
            // Apply the scale change, adjusting the height to maintain aspect ratio
            size->width = size->width * scale;
            size->height = size->height * scale;  // Adjust height proportionally to the width
        }
        // Checkbox for visibility change
        ImGui::Checkbox("Visible", &visibility);

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
                entity.destruct();  // Delete the entity
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
}


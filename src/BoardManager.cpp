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

BoardManager::BoardManager(flecs::world ecs, NetworkManager* network_manager)
    : ecs(ecs), camera(), currentTool(Tool::MOVE), mouseStartPos({0,0}), marker_directory(std::string(), std::string()), network_manager(network_manager){
    
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
    const Position* position = active_board.get<Position>();
    const Grid* grid = active_board.get<Grid>();
    const TextureComponent* texture = active_board.get<TextureComponent>();
    const Size* size = active_board.get<Size>();
        
    ImVec2 window_size = ImGui::GetWindowSize();
    ImVec2 window_position = ImGui::GetWindowPos();
    camera.setWindowSize(glm::vec2(window_size.x, window_size.y));
    camera.setWindowPosition(glm::vec2(window_position.x, window_position.y));

    glm::mat4 viewMatrix = camera.getViewMatrix();  // Obtém a matriz de visualização da câmera (pan/zoom)
    glm::mat4 projection = camera.getProjectionMatrix();

    //glm::mat4 model = glm::scale(model, glm::vec3(size->width, size->height, 1.0f));
    //model = glm::translate(glm::mat4(1.0f), glm::vec3(position->x, position->y, 0.0f));

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position->x, position->y, 0.0f));
    model = glm::scale(model, glm::vec3(size->width, size->height, 1.0f));


    glm::mat4 mvp = projection * viewMatrix * model; //Calculate Screen Position(Can use method to standize it, but alter to return the MVP


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
            const Position* position_marker = child.get<Position>();
            const Visibility* visibility_marker = child.get<Visibility>();
            const TextureComponent* texture_marker = child.get<TextureComponent>();
            const Size* size_marker = child.get<Size>();

            //glm::mat4 model = glm::scale(model, glm::vec3(size_marker->width, size_marker->height, 1.0f));
            //model = glm::translate(glm::mat4(1.0f), glm::vec3(position_marker->x, position_marker->y, 0.0f));
            
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position_marker->x, position_marker->y, 0.0f));
            model = glm::scale(model, glm::vec3(size_marker->width, size_marker->height, 1.0f));

            glm::mat4 mvp = projection * viewMatrix * model; //Calculate Screen Position(Can use method to standize it, but alter to return the MVP
            float alpha = 1.0f;
            //if(!visibility_marker->isVisible){
            //    //NETWORK ROLE CHECK - alpha GAMEMASTER 0.5f - alpha PLAYER 0.0f
            //    alpha = 0.5f;
            //}

            shader.Bind();
            shader.SetUniformMat4f("u_MVP", mvp);
            //shader.SetUniformMat4f("u_Projection", projection);
            //shader.SetUniformMat4f("u_View", viewMatrix);
            //shader.SetUniformMat4f("u_Model", model);
            shader.SetUniform1f("u_Alpha", alpha);
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
    //mousePos(Screen Position) use screenToWorldPosition(mousePos)  position(World Position) 
    ecs.each([&](flecs::entity entity, const MarkerComponent& marker, Moving& moving, Position& position) {
        if (entity.has(flecs::ChildOf, active_board) && moving.isDragging) {
            glm::vec2 world_position = screenToWorldPosition(mousePos);
            glm::vec2 start_world_position = screenToWorldPosition(mouseStartPos);
            glm::vec2 delta = world_position - start_world_position;
            position.x += delta.x;
            position.y += delta.y;
            mouseStartPos = mousePos;
        }
     });


 /*   if (hovered_marker != nullptr) {
        auto position = hovered_marker->get_mut<Position>();
        glm::vec2 world_position = screenToWorldPosition(mousePos);
        glm::vec2 start_world_position = screenToWorldPosition(mouseStartPos);
        glm::vec2 delta = world_position - start_world_position;
        position->x += delta.x;
        position->y += delta.y;
    }*/
}

//TODO
//ANALISAR AS COORDENADAS NO CODIGO, DEFINIR CADA TIPO DE COODENADA QUANDO SALVO(WORLD OR SCREEN) 
//Ver todos os lugares que usam e revisar, ver os componentes que tem positions e sizes, ver o que ta sendo usado e como
//Fazer camada que transforma coordenadas quando interagindo com o mundo(para os Marker e FogOfWar


//glm::vec2 BoardManager::screenToWorldPosition(glm::vec2 screen_position) {
//    // Step 1: Get window size
//    glm::vec2 windowSize = camera.getWindowSize();
//
//    // Step 2: Normalize screen coordinates to NDC (Normalized Device Coordinates)
//    float ndcX = (2.0f * screen_position.x) / windowSize.x - 1.0f;
//    float ndcY = 1.0f - (2.0f * screen_position.y) / windowSize.y;  // Invert Y-axis
//
//    // Step 3: Create NDC position vector (z = 0, w = 1)
//    glm::vec4 ndcPos = glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
//
//    // Step 4: Get the combined MVP matrix and calculate its inverse
//    glm::mat4 MVP = camera.getProjectionMatrix() * camera.getViewMatrix();
//    glm::mat4 inverseMVP = glm::inverse(MVP);
//
//    // Step 5: Transform NDC to world coordinates
//    glm::vec4 world_position = inverseMVP * ndcPos;
//
//    // Step 6: Return the world position as 2D (x, y) coordinates (ignore z)
//    return glm::vec2(world_position.x, world_position.y);
//}

glm::vec2 BoardManager::screenToWorldPosition(glm::vec2 screen_position) {

    glm::vec2 relative_screen_position = { screen_position.x - camera.getWindowPosition().x, screen_position.y - camera.getWindowPosition().y};
    
    // Get the view matrix (which handles panning and zoom)
    glm::mat4 view_matrix = camera.getViewMatrix();

    // Get the projection matrix (which might handle window size)
    glm::mat4 proj_matrix = camera.getProjectionMatrix();

    // Create a normalized device coordinate from the screen position
    glm::vec2 window_size = camera.getWindowSize();

    // Convert the screen position to normalized device coordinates (NDC)
    float ndc_x = (2.0f * relative_screen_position.x) / window_size.x - 1.0f;
    float ndc_y = 1.0f - (2.0f * relative_screen_position.y) / window_size.y; // Inverting y-axis for OpenGL
    std::cout << "X: " << ndc_x << " | " << "Y: " << ndc_y << std::endl;
    glm::vec4 ndc_position = glm::vec4(ndc_x, ndc_y, 0.0f, 1.0f);

    // Calculate the inverse MVP matrix (to map from NDC back to world space)
    glm::mat4 mvp = proj_matrix * view_matrix;
    glm::mat4 inverse_mvp = glm::inverse(mvp);

    // Transform the NDC position back to world coordinates
    glm::vec4 world_position = inverse_mvp * ndc_position;

    // Perform perspective divide to get the correct world position
    if (world_position.w != 0.0f) {
        world_position /= world_position.w;
    }

    // Return the world position as a 2D vector (we're ignoring the Z-axis for 2D rendering)
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
    ecs.each([&](flecs::entity entity, const MarkerComponent& marker, const Position& markerPos, const Size& markerSize, Moving& moving) {
        // Check if the marker is a child of the active board
        //use mousePos(Screen Position) screenToWorldPosition(mousePos) when comparing to markerPos(World Position)
        glm::vec2 world_position = screenToWorldPosition(mousePos);
      

        if (entity.has(flecs::ChildOf, active_board)) {
            auto text = entity.get<TextureComponent>();
            //bool withinXBounds = world_position.x >= markerPos.x - markerSize.width/2 && world_position.x <= (markerPos.x + markerSize.width/2);
            //bool withinYBounds = world_position.y >= markerPos.y - markerSize.height/2 && world_position.y <= (markerPos.y + markerSize.height/2);

            bool withinXBounds = (world_position.x >= (markerPos.x - markerSize.width / 2)) &&
                (world_position.x <= (markerPos.x + markerSize.width / 2));

            bool withinYBounds = (world_position.y >= (markerPos.y - markerSize.height / 2)) &&
                (world_position.y <= (markerPos.y + markerSize.height / 2));


            std::cout << "File Name: " << text->image_path << std::endl;
            std::cout << "Mouse ScreenPosition: " << mousePos.x << " | " << mousePos.y << std::endl;
            std::cout << "Mouse WorldPosition: " << world_position.x << " | " << world_position.y << std::endl;
            std::cout << "Marker WorldPosition: " << markerPos.x << " | " << markerPos.y << std::endl;
            std::cout << "Marker Size: " << markerSize.width << " | " << markerSize.height << std::endl;
            std::cout << "BOUNDS x : " << markerPos.x - markerSize.width / 2 << " | " << markerPos.x + markerSize.width / 2 << std::endl;
            std::cout << "BOUNDS y : " << markerPos.y - markerSize.height / 2 << " | " << markerPos.y + markerSize.height / 2 << std::endl;
            std::cout << "IN BOUNDS: " << withinXBounds << " | " << withinYBounds << std::endl;
            std::cout << "-------------------------------------------------------------------" << std::endl;
            if (withinXBounds && withinYBounds) {
                moving.isDragging = true;
                hovered = true;           // Mark as hovered
            }
        }
    });

    return hovered;
}


void BoardManager::startMouseDrag(glm::vec2 mousePos, bool draggingMarker) {
    mouseStartPos = mousePos;  // Captura a posição inicial do mouse
    if (not draggingMarker) {
        active_board.set<Panning>({ true });
    }
}


void BoardManager::endMouseDrag() {
    active_board.set<Panning>({ false });
    ecs.each([&](flecs::entity entity, const MarkerComponent& marker, Moving& moving) {
        if (entity.has(flecs::ChildOf, active_board)) {
            moving.isDragging = false;
        }
    });
}

bool BoardManager::isPanning() {
    const Panning* panning = active_board.get<Panning>();
    return panning->isPanning;
}

bool BoardManager::isDragginMarker() {
    bool isDragginMarker = false;
    ecs.each([&](flecs::entity entity, const MarkerComponent& marker, Moving& moving) {
        if (entity.has(flecs::ChildOf, active_board) && moving.isDragging) {
            isDragginMarker =  true;
        }
    });
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


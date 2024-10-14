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

BoardManager::BoardManager(flecs::world ecs, std::string shader_file_path)
    : ecs(ecs), camera(), currentTool(Tool::MOVE), vertexArray(), indexBuffer(nullptr, 0), shader(shader_file_path), mouseStartPos({0,0}), marker_directory(std::string(), std::string()){
    
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

flecs::entity BoardManager::createBoard()
{
    auto board = ecs.entity()
        .set(Board{})
        .set(Panning{false})
        .set(Zoom{1.0f})
        .set(Position{0,0})
        .set(Grid{ {0.0f,0.0f},{1.0f,1.0f} })
        .set(TextureComponent{})
        .set(Size{1.0f, 1.0f});
    active_board = board;
    return board;
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
    ImGui::Begin("Toolbar");

    if (ImGui::Button("Move Tool")) {
        setCurrentTool(Tool::MOVE);
    }
    if (ImGui::Button("Marker Tool")) {
        setCurrentTool(Tool::MARKER);
    }
    if (ImGui::Button("Fog Tool")) {
        setCurrentTool(Tool::FOG);
    }
    if (ImGui::Button("Select Tool")) {
        setCurrentTool(Tool::SELECT);
    }

    ImGui::End();
}

void BoardManager::renderBoard(VertexArray& va, IndexBuffer& ib, Shader& shader, Renderer& renderer) {

    const Board* board = active_board.get<Board>();
    const Panning* panning = active_board.get<Panning>();
    const Zoom* zoom = active_board.get<Zoom>();
    const Position* position = active_board.get<Position>();
    const Grid* grid = active_board.get<Grid>();
    const TextureComponent* texture = active_board.get<TextureComponent>();
    const Size* size = active_board.get<Size>();
    glm::mat4 viewMatrix = camera.getViewMatrix();  // Obtém a matriz de visualização da câmera (pan/zoom)


    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position->x, position->y, 0.0f));
    model = glm::scale(model, glm::vec3(size->width, size->height, 1.0f));
        
    ImVec2 window_size = ImGui::GetWindowSize();
    glm::mat4 projection = glm::ortho(0.0f, window_size.x, 0.0f, window_size.y, -1.0f, 1.0f);
    glm::mat4 mvp = projection * viewMatrix * model;
    
    shader.Bind();
    shader.SetUniformMat4f("u_MVP", mvp);
    shader.SetUniform1f("u_Alpha", 1.0f);
    shader.SetUniform1i("u_Texture", 0);
    shader.Unbind();


    GLCall(glActiveTexture(GL_TEXTURE0));
    GLCall(glBindTexture(GL_TEXTURE_2D, texture->textureID));

    renderer.Draw(va, ib, shader);

    //ImVec2 map_window_size = ImGui::GetWindowSize();
    //// Use `each()` to query and apply the function to the entity with the necessary components
    //ecs.each([&](flecs::entity entity, const TextureComponent& texture, const Position& pos, const Size& size, const Zoom& zoom, const Panning& panning, const Board& board, const Grid& grid) {
    //    // Rendering the board image with texture
    //    //renderGrid(viewMatrix, map_window_size.x, map_window_size.y);
    //    renderImage(texture.textureID, pos, size, viewMatrix);
    //});


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
//
//// Render Image (Board background)
//void BoardManager::renderImage(GLuint textureID, const Position& pos, const Size& size, glm::mat4& viewMatrix) {
//    
//  
//    // Model matrix: based on the position and size of the object
//    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, 0.0f));
//    model = glm::scale(model, glm::vec3(size.width, size.height, 1.0f));
//    
//    ImVec2 window_size = ImGui::GetWindowSize();
//    glm::mat4 projection = glm::ortho(0.0f, window_size.x, 0.0f, window_size.y, -1.0f, 1.0f);
//    // MVP: Multiply Projection * View * Model in this order
//    glm::mat4 mvp = projection * viewMatrix * model;
//
//
//    shader.Bind();
//    shader.SetUniformMat4f("u_MVP", mvp);
//    shader.SetUniform1f("u_Alpha", 1.0f);
//    shader.SetUniform1i("u_Texture", 0);
//    shader.Unbind();
//
//    GLCall(glBindTexture(GL_TEXTURE_2D, textureID));
//    Renderer renderer;
//    renderer.Draw(vertexArray, indexBuffer, shader);  // Render using the VAO/IBO
//    GLCall(glBindTexture(GL_TEXTURE_2D, 0));
//    shader.Unbind();
//}

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

flecs::entity BoardManager::createMarker(const std::string& imageFilePath, glm::vec2 position) {
    Texture texture(imageFilePath);

     flecs::entity marker = ecs.entity()
        .set(MarkerComponent{ false })
        .set(Position{ (int)position.x, (int)position.y })
        .set(Size{ 1.0f, 1.0f })
        .set(TextureComponent{ texture.GetRendererID(), imageFilePath })
        .set(Visibility{ true })
        .set(Moving{ false });

    marker.add(flecs::ChildOf, active_board);

    return marker;
}

void BoardManager::deleteMarker(flecs::entity markerEntity) {
    markerEntity.destruct();
}

void BoardManager::toggleMarkerVisibility(flecs::entity markerEntity) {
    markerEntity.get([](Visibility& visibility) {
        visibility.isVisible = !visibility.isVisible;
        });
}

//void BoardManager::changeMarkerSize(flecs::entity markerEntity, glm::vec2 newSize) {
//    markerEntity.set([newSize](Size& size) {
//        size.width = newSize.x;
//        size.height = newSize.y;
//        });
//}

void BoardManager::handleMarkerDragging(glm::vec2 mousePos) {
    ecs.each([&](flecs::entity e, Position& pos, Moving& moving) {
        if (moving.isDragging) {
            pos.x = (int)mousePos.x;
            pos.y = (int)mousePos.y;
        }
        });
}

bool BoardManager::isMouseOverMarker(const Position& markerPos, const Size& markerSize, glm::vec2 mousePos) {
    // Verifica se o mouse está dentro dos limites do marcador
    bool withinXBounds = mousePos.x >= markerPos.x && mousePos.x <= (markerPos.x + markerSize.width);
    bool withinYBounds = mousePos.y >= markerPos.y && mousePos.y <= (markerPos.y + markerSize.height);

    return withinXBounds && withinYBounds;
}


void BoardManager::handleMarkerSelection(glm::vec2 mousePos) {
    ecs.each([&](flecs::entity e, const Position& pos, const Size& size, MarkerComponent& marker) {
        if (isMouseOverMarker(pos, size, mousePos)) {
            marker.isWindowActive = true;  // Abre a janela de configuração do marcador
        }
        });
}


void BoardManager::startMouseDrag(glm::vec2 mousePos) {
    mouseStartPos = mousePos;  // Captura a posição inicial do mouse
}

glm::vec2 BoardManager::getMouseStartPosition() const {
    return mouseStartPos;
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
//
//void BoardManager::renderSquareGrid(glm::mat4& mvp, float windowWidth, float windowHeight, const Grid& grid) {
//    std::vector<glm::vec2> vertices;
//
//    // Generate vertical and horizontal lines for square grid
//    for (float x = -grid.offset.x; x < windowWidth; x += grid.scale.x) {
//        vertices.push_back(glm::vec2(x, 0.0f));
//        vertices.push_back(glm::vec2(x, windowHeight));
//    }
//    for (float y = -grid.offset.y; y < windowHeight; y += grid.scale.y) {
//        vertices.push_back(glm::vec2(0.0f, y));
//        vertices.push_back(glm::vec2(windowWidth, y));
//    }
//
//    renderLineVertices(vertices, mvp);
//}
//
//void BoardManager::renderHexGrid(glm::mat4& mvp, float windowWidth, float windowHeight, const Grid& grid) {
//    std::vector<glm::vec2> vertices;
//    float hexWidth = grid.scale.x;
//    float hexHeight = grid.scale.y;
//    float threeQuarterHexHeight = hexHeight * 0.75f;
//
//    for (float y = -grid.offset.y; y < windowHeight; y += threeQuarterHexHeight) {
//        for (float x = -grid.offset.x; x < windowWidth; x += hexWidth * 0.75f) {
//            float adjustedX = (int(y / hexHeight) % 2 == 0) ? x : x + (hexWidth * 0.5f);
//            addHexagonVertices(adjustedX, y, hexWidth * 0.5f, vertices);
//        }
//    }
//
//    renderLineVertices(vertices, mvp);
//}
//
//void BoardManager::addHexagonVertices(float x, float y, float radius, std::vector<glm::vec2>& vertices) {
//    constexpr float angleIncrement  = glm::radians(60.0f);  // 60 degrees for hexagon angles
//
//    for (int i = 0; i < 6; ++i) {
//        float angle1 = i * angleIncrement;
//        float angle2 = (i + 1) * angleIncrement;
//
//        glm::vec2 p1 = glm::vec2(x + radius * cos(angle1), y + radius * sin(angle1));
//        glm::vec2 p2 = glm::vec2(x + radius * cos(angle2), y + radius * sin(angle2));
//
//        vertices.push_back(p1);
//        vertices.push_back(p2);  // Line from p1 to p2
//    }
//}

//void BoardManager::renderLineVertices(const std::vector<glm::vec2>& vertices, glm::mat4& mvp) {
//    shader.Bind();
//    shader.SetUniformMat4f("u_MVP", mvp);
//    shader.SetUniform4f("u_Color", 0.0f, 0.0f, 0.0f, 1.0f);
//    shader.SetUniform1i("useTexture", 1);
//
//    VertexArray va;
//    VertexBuffer vb(vertices.data(), vertices.size() * sizeof(glm::vec2));
//    VertexBufferLayout layout;
//    layout.Push<float>(2);  // Each vertex has 2 components (x, y)
//    va.AddBuffer(vb, layout);
//
//    glDrawArrays(GL_LINES, 0, vertices.size());
//
//    va.Unbind();
//    vb.Unbind();
//    shader.Unbind();
//}

// Handles mouse input for moving and resizing the grid
void BoardManager::handleGridTool(const glm::vec2& mouseDelta, float scrollDelta) {
    ecs.each([&](flecs::entity entity, Grid& grid) {
        // Move grid (change offset) with mouse dragging
        grid.offset += mouseDelta;

        // Scale the grid with scroll input
        grid.scale += glm::vec2(scrollDelta) * 0.1f;
        grid.scale = glm::max(grid.scale, glm::vec2(1.0f));  // Prevent grid cells from becoming too small
        });
}

// Toggles between square and hex grid
void BoardManager::toggleGridType() {
    ecs.each([&](flecs::entity entity, Grid& grid) {
        grid.is_hex = !grid.is_hex;
        });
}

// Toggle snap to grid functionality
void BoardManager::toggleSnapToGrid() {
    ecs.each([&](flecs::entity entity, Grid& grid) {
        grid.snap_to_grid = !grid.snap_to_grid;
        });
}

// Snap the position to the nearest grid point
glm::vec2 BoardManager::snapToGrid(const glm::vec2& position) {
    glm::vec2 snappedPos = position;

    ecs.each([&](flecs::entity entity, const Grid& grid) {
        if (grid.snap_to_grid) {
            if (grid.is_hex) {
                // Snap to hex grid (use axial coordinates)
                snappedPos = snapToHexGrid(position, grid);
            }
            else {
                // Snap to square grid
                snappedPos = snapToSquareGrid(position, grid);
            }
        }
        });

    return snappedPos;
}

// Snap to square grid
glm::vec2 BoardManager::snapToSquareGrid(const glm::vec2& position, const Grid& grid) {
    float snappedX = round((position.x - grid.offset.x) / grid.scale.x) * grid.scale.x + grid.offset.x;
    float snappedY = round((position.y - grid.offset.y) / grid.scale.y) * grid.scale.y + grid.offset.y;
    return glm::vec2(snappedX, snappedY);
}

// Snap to hex grid using axial coordinates
glm::vec2 BoardManager::snapToHexGrid(const glm::vec2& pos, const Grid& grid) {
    glm::vec2 axialPos = worldToAxial(pos, grid);
    glm::vec2 roundedAxial = roundAxial(axialPos);
    return axialToWorld(roundedAxial, grid);
}

glm::vec2 BoardManager::worldToAxial(const glm::vec2& pos, const Grid& grid) {
    float q = (pos.x * sqrt(3) / 3 - pos.y / 3) / (grid.scale.x / 2);
    float r = (pos.y * 2 / 3) / (grid.scale.y / 2);
    return glm::vec2(q, r);
}

glm::vec2 BoardManager::axialToWorld(const glm::vec2& axialPos, const Grid& grid) {
    float x = grid.scale.x * (sqrt(3) * axialPos.x + sqrt(3) / 2 * axialPos.y);
    float y = grid.scale.y * (3 / 2 * axialPos.y);
    return glm::vec2(x, y);
}

glm::vec2 BoardManager::roundAxial(const glm::vec2& axial) {
    float rx = round(axial.x);
    float ry = round(axial.y);
    float rz = round(-axial.x - axial.y);

    float x_diff = fabs(rx - axial.x);
    float y_diff = fabs(ry - axial.y);
    float z_diff = fabs(rz - (-axial.x - axial.y));

    if (x_diff > y_diff && x_diff > z_diff) rx = -ry - rz;
    else if (y_diff > z_diff) ry = -rx - rz;

    return glm::vec2(rx, ry);
}

flecs::entity BoardManager::createFogOfWar(glm::vec2 startPos, glm::vec2 size) {
    return ecs.entity()
        .set(FogOfWar{ false })
        .set(Position{ (int)startPos.x, (int)startPos.y })
        .set(Size{ size.x, size.y })
        .set(Visibility{ true });
}

void BoardManager::deleteFogOfWar(flecs::entity fogEntity) {
    fogEntity.destruct();
}

void BoardManager::toggleFogVisibility(flecs::entity fogEntity) {
    fogEntity.get([](Visibility& visibility) {
        visibility.isVisible = !visibility.isVisible;
        });
}

void BoardManager::handleFogCreation(glm::vec2 mousePos) {
    glm::vec2 startPos = getMouseStartPosition();  // Assuming this tracks the starting drag point
    glm::vec2 size = mousePos - startPos;
    createFogOfWar(startPos, size);
}

void BoardManager::panBoard(glm::vec2 delta) {
    camera.pan(delta);
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



//================OLD ===================================

//void BoardManager::updateCamera(float zoomLevel, const glm::vec2& panOffset) {
//    // Set up orthographic projection based on zoom and pan
//    float left = -1.0f * zoomLevel + panOffset.x;
//    float right = 1.0f * zoomLevel + panOffset.x;
//    float bottom = -1.0f * zoomLevel + panOffset.y;
//    float top = 1.0f * zoomLevel + panOffset.y;
//
//    viewProjectionMatrix = glm::ortho(left, right, bottom, top, -1.0f, 1.0f);
//}
/*
  viewProjectionMatrix = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);

    float positions[] = {
        -1.0f, -1.0f, 0.0f, 0.0f,  // Bottom left
         1.0f, -1.0f, 1.0f, 0.0f,  // Bottom right
         1.0f,  1.0f, 1.0f, 1.0f,  // Top right
        -1.0f,  1.0f, 0.0f, 1.0f   // Top left
    };
    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    vb = VertexBuffer(positions, 4 * 4 * sizeof(float));
    VertexBufferLayout layout;
    layout.Push<float>(2);  // Position
    layout.Push<float>(2);  // Texture coordinates
    va.AddBuffer(vb, layout);
    this->ib = IndexBuffer(indices, 6);

    glm::mat4 proj = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    shader.Bind();
    shader.SetUniform1i("u_Texture", 0);
    shader.SetUniformMat4f("u_MVP", proj);
    shader.Unbind();
    va.Unbind();
    vb.Unbind();
    ib.Unbind();


*/


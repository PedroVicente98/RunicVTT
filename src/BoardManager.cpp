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
    std::string marker_directory_name = "MarkerDiretory";
    marker_directory.directoryName = marker_directory_name;
    marker_directory.directoryPath = marker_directory_path.string();
    marker_directory.startMonitoring();


    // Initialize vertexArray and indexBuffer once (assuming basic quad for rendering)
    float vertices[] = {
        // Position       // TexCoord
        -0.5f, -0.5f,    0.0f, 0.0f,
         0.5f, -0.5f,    1.0f, 0.0f,
         0.5f,  0.5f,    1.0f, 1.0f,
        -0.5f,  0.5f,    0.0f, 1.0f
    };

    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    // Create and bind vertex buffer
    VertexBuffer vb(vertices, sizeof(vertices));
    VertexBufferLayout layout;
    layout.Push<float>(2);  // Position
    layout.Push<float>(2);  // TexCoord
    vertexArray.AddBuffer(vb, layout);

    // Initialize the index buffer
    indexBuffer = IndexBuffer(indices, 6);  // 6 indices for 2 triangles in the quad

    // Shader initialization (assuming a vertex and fragment shader)
    shader.Bind();
    shader.SetUniform1i("u_Texture", 0);  // Use texture slot 0 by default
    shader.SetUniform1f("u_Alpha", 1.0f);  // Use texture slot 0 by default
    shader.Unbind();
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

flecs::entity BoardManager::createBoard(std::string board_name, std::string map_image_path, GLuint texture_id)
{   
    //Texture texture = Texture(map_image_path);
    auto board = ecs.entity()
        .set(Board{ board_name })
        .set(Panning{false})
        .set(Zoom{1.0f})
        .set(Position{0,0})
        .set(Grid{ {0.0f,0.0f},{1.0f,1.0f} })
        .set(TextureComponent{ texture_id, map_image_path})
        .set(Size{1.0f, 1.0f});
    active_board = board;
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

void BoardManager::renderBoard() {
    glm::mat4 viewMatrix = camera.getViewMatrix();  // Obtém a matriz de visualização da câmera (pan/zoom)

    // Renderizar o fundo do tabuleiro (mapa)
    active_board.get([&](const TextureComponent& texture, const Position& pos, const Size& size, const Visibility& visibility) {
        if (visibility.isVisible) {
            renderImage(texture.textureID, pos, size, viewMatrix);  // Renderizar a imagem de fundo
        }
        });
    // Renderizar os marcadores
    ecs.each([&](flecs::entity e, const MarkerComponent& marker, const Position& pos, const Size& size, const TextureComponent& texture, const Visibility& visibility) {
        if (visibility.isVisible) {
            renderMarker(texture.textureID, pos, size, viewMatrix);  // Renderizar o marcador
        }
        });
    // Renderizar a névoa de guerra (Fog of War)
    ecs.each([&](flecs::entity e, const FogOfWar& fog, const Position& pos, const Size& size, const Visibility& visibility) {
        if (visibility.isVisible) {
            renderFog(pos, size, viewMatrix, 1.0f);  // Renderizar a névoa com transparência
        }
        });
}

// Render Image (Board background)
void BoardManager::renderImage(GLuint textureID, const Position& pos, const Size& size, glm::mat4& viewMatrix) {
    shader.Bind();
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, 0.0f));
    model = glm::scale(model, glm::vec3(size.width, size.height, 1.0f));
    glm::mat4 mvp = viewMatrix * model;
    shader.SetUniformMat4f("u_MVP", mvp);
    shader.SetUniform1i("u_Texture", 0);

    Texture texture("");  // Assuming you have the path to the texture
    texture.Bind();
    Renderer renderer;
    renderer.Draw(vertexArray, indexBuffer, shader);  // Render using the VAO/IBO
    texture.Unbind();
    shader.Unbind();
}

// Render Marker
void BoardManager::renderMarker(GLuint textureID, const Position& pos, const Size& size, glm::mat4& viewMatrix) {
    shader.Bind();
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, 0.0f));
    model = glm::scale(model, glm::vec3(size.width, size.height, 1.0f));
    glm::mat4 mvp = viewMatrix * model;
    shader.SetUniformMat4f("u_MVP", mvp);
    shader.SetUniform1i("u_Texture", 0);

    Texture markerTexture("");  // Assuming you have the path to the marker's texture
    markerTexture.Bind();
    Renderer renderer;
    renderer.Draw(vertexArray, indexBuffer, shader);
    markerTexture.Unbind();
    shader.Unbind();
}

// Render Fog with transparency
void BoardManager::renderFog(const Position& pos, const Size& size, const glm::mat4& viewMatrix, const float alpha) {
    shader.Bind();
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(pos.x, pos.y, 0.0f));
    model = glm::scale(model, glm::vec3(size.width, size.height, 1.0f));
    glm::mat4 mvp = viewMatrix * model;
    shader.SetUniformMat4f("u_MVP", mvp);
    shader.SetUniform1f("u_Alpha", alpha);  // Set transparency for fog

    Renderer renderer;
    renderer.Draw(vertexArray, indexBuffer, shader);
    shader.Unbind();
}

flecs::entity BoardManager::createMarker(const std::string& imageFilePath, glm::vec2 position) {
    Texture texture(imageFilePath);

    return ecs.entity()
        .set(MarkerComponent{ false })
        .set(Position{ (int)position.x, (int)position.y })
        .set(Size{ 1.0f, 1.0f })
        .set(TextureComponent{ texture.GetRendererID(), imageFilePath })
        .set(Visibility{ true })
        .set(Moving{ false });
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


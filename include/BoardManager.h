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

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "flecs.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "Components.h"
#include "DirectoryWindow.h"

class Camera {
public:
    Camera() : position(0.0f, 0.0f), zoomLevel(1.0f) {}

    void pan(glm::vec2 delta) {
        position += delta;
    }
    void zoom(float zoomFactor) {
        zoomLevel *= zoomFactor;
    }
    glm::mat4 getViewMatrix() const {
        return glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(position, 0.0f)), glm::vec3(zoomLevel));
    }

private:
    glm::vec2 position;
    float zoomLevel;
};


enum class Tool{ MOVE, FOG, MARKER, SELECT};

class BoardManager {
public:
	BoardManager(flecs::world ecs, std::string shader_file_path);
	~BoardManager();

	void renderBoard();  // Render board elements (map, markers, fog)
	void renderToolbar();  // Render toolbar

    // Marker interactions
    flecs::entity createMarker(const std::string& imageFilePath, glm::vec2 position);
    void deleteMarker(flecs::entity markerEntity);
    void toggleMarkerVisibility(flecs::entity markerEntity);
    void changeMarkerSize(flecs::entity markerEntity, glm::vec2 newSize);
    void handleMarkerDragging(glm::vec2 mousePos);
    void handleMarkerSelection(glm::vec2 mousePos);
    bool isMouseOverMarker(const Position& markerPos, const Size& markerSize, glm::vec2 mousePos);

    // Fog of War interactions
    flecs::entity createFogOfWar(glm::vec2 startPos, glm::vec2 size);
    void deleteFogOfWar(flecs::entity fogEntity);
    void toggleFogVisibility(flecs::entity fogEntity);
    void handleFogCreation(glm::vec2 mousePos);

    // Camera manipulation
    void panBoard(glm::vec2 delta);
    void zoomBoard(float zoomFactor);

    // Toolbar tool management
    Tool getCurrentTool() const;
    void setCurrentTool(Tool newTool);

	bool isBoardActive();
    flecs::entity createBoard();
    flecs::entity createBoard(std::string board_name, std::string map_image_path, GLuint texture_id);
	//flecs::entity createBoard(std::string map_image_path);

    void closeBoard();
    void setActiveBoard(flecs::entity board_entity);


    void startMouseDrag(glm::vec2 mousePos);
    glm::vec2 getMouseStartPosition() const;


	std::string board_name;
    DirectoryWindow marker_directory;
private:
    // Render functions (private)
    void renderImage(GLuint textureID, const Position &pos, const Size &size, glm::mat4 &viewMatrix);
    void renderMarker(GLuint textureID, const Position &pos, const Size &size, glm::mat4 &viewMatrix);
    void renderFog(const Position &pos, const Size &size, const glm::mat4 &viewMatrix, const float alpha);

    glm::vec2 mouseStartPos;

	flecs::world ecs;
	flecs::entity active_board = flecs::entity();
    Camera camera;
    Tool currentTool;  // Active tool for interaction

    // Rendering resources
    VertexArray vertexArray;
    IndexBuffer indexBuffer;
    Shader shader;  // Shared shader used across renderings

};
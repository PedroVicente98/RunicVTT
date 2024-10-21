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
    Camera() : position(0.0f, 0.0f), zoomLevel(1.0f) {}  // Set initial zoom to 1.0f for no scaling by default
    
    void pan(glm::vec2 delta) {
        position += delta;
    }
    void zoom(float zoomFactor) {
        zoomLevel *= zoomFactor;
    }
    void setPosition(glm::vec2 newPosition) {
        position = newPosition;
    }
    glm::vec2 getPosition() const {
        return position;
    }
    void setZoom(float newZoomLevel) {
        zoomLevel = newZoomLevel;
    }
    float getZoom() const {
        return zoomLevel;
    }
    glm::mat4 getViewMatrix() const {
        // First translate the view by the camera position, then scale it by the zoom level
        return glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-position, 0.0f)), glm::vec3(zoomLevel));
    } 

    glm::mat4 getProjectionMatrix() const {
        return  glm::ortho(-window_size.x, window_size.x, window_size.y, -window_size.y, -1.0f, 1.0f);
    }

    glm::vec2 getWindowSize() {
        return window_size;
    }

    void setWindowSize(glm::vec2 window_size) {
        this->window_size = window_size;
    }

    glm::vec2 getWindowPosition() {
        return window_position;
    }

    void setWindowPosition(glm::vec2 window_position) {
        this->window_position = window_position;
    }


private:
    glm::vec2 position;  // 2D position of the camera (X, Y)
    float zoomLevel;     // Zoom level, where 1.0f means no zoom, > 1.0f means zoom in, < 1.0f means zoom out
    glm::vec2 window_size;
    glm::vec2 window_position;
};


enum class Tool{ MOVE, FOG, MARKER, SELECT};
//MOVE - Move Camera and Markers;
//FOG - Create FOG and TOGGLE VISIBILITY
//MARKER - Toggle Marker Visibility? MIGHT NOT BE NECESSARY
//SELECT - Select Marker/Fog, opening an edit window(change size, visibility, and delete)


class BoardManager {
public:
	BoardManager(flecs::world ecs);
	~BoardManager();

	void renderBoard(VertexArray& va, IndexBuffer& ib, Shader& shader, Renderer& renderer);  // Render board elements (map, markers, fog)
	void renderToolbar();  // Render toolbar

    void resetCamera();
	
    // Marker interactions
    flecs::entity createMarker(const std::string& imageFilePath, GLuint textureId, glm::vec2 position, glm::vec2 size);
    void deleteMarker(flecs::entity markerEntity);
    void handleMarkerDragging(glm::vec2 mousePos);
    bool isMouseOverMarker(glm::vec2 mousePos);

    // Fog of War interactions
    flecs::entity createFogOfWar(glm::vec2 startPos, glm::vec2 size);
    void deleteFogOfWar(flecs::entity fogEntity);
    void handleFogCreation(glm::vec2 mousePos);

    // Camera manipulation
    void panBoard(glm::vec2 currentMousePos);
    void zoomBoard(float zoomFactor);

    // Toolbar tool management
    Tool getCurrentTool() const;
    void setCurrentTool(Tool newTool);

	bool isBoardActive();
    flecs::entity createBoard(std::string board_name, std::string map_image_path, GLuint texture_id, glm::vec2 size);

    void closeBoard();
    void setActiveBoard(flecs::entity board_entity);

    void renderEditWindow(flecs::entity entity);
    void startMouseDrag(glm::vec2 mousePos, bool draggingMarker);
    void endMouseDrag();
    glm::vec2 getMouseStartPosition() const;
    bool isPanning();
    bool isDragginMarker();
	std::string board_name;
    DirectoryWindow marker_directory;
    glm::vec2 screenToWorldPosition(glm::vec2 screen_position);
    glm::vec2 worldToScreenPosition(glm::vec2 world_position);

    Camera camera;
private:

    glm::vec2 mouseStartPos;

	flecs::world ecs;
	flecs::entity active_board = flecs::entity();
	//flecs::entity* hovered_marker = nullptr;
    Tool currentTool;  // Active tool for interaction
};

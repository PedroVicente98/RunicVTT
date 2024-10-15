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

    glm::vec2 getWindowSize() {
        return window_size;
    }

    void setWindowSize(glm::vec2 window_size) {
        this->window_size = window_size;
    }
    

private:
    glm::vec2 position;  // 2D position of the camera (X, Y)
    float zoomLevel;     // Zoom level, where 1.0f means no zoom, > 1.0f means zoom in, < 1.0f means zoom out
    glm::vec2 window_size;
};


enum class Tool{ MOVE, FOG, MARKER, SELECT};

class BoardManager {
public:
	BoardManager(flecs::world ecs);
	~BoardManager();

	void renderBoard(VertexArray& va, IndexBuffer& ib, Shader& shader, Renderer& renderer);  // Render board elements (map, markers, fog)
	void renderToolbar();  // Render toolbar

    void resetCamera();

    // Marker interactions
    flecs::entity createMarker(const std::string& imageFilePath, glm::vec2 position);
    void deleteMarker(flecs::entity markerEntity);
    void toggleMarkerVisibility(flecs::entity markerEntity);
    void changeMarkerSize(flecs::entity markerEntity, glm::vec2 newSize);
    void handleMarkerDragging(glm::vec2 mousePos);
    void handleMarkerSelection(glm::vec2 mousePos);
    bool isMouseOverMarker(const Position& markerPos, const Size& markerSize, glm::vec2 mousePos);

    // Grid-related methods
   
    void handleGridTool(const glm::vec2& mouseDelta, float scrollDelta);
    void toggleGridType();  // Switch between square and hex grid
    void toggleSnapToGrid();  // Enable/disable snap to grid
    glm::vec2 snapToGrid(const glm::vec2& position);

    // Fog of War interactions
    flecs::entity createFogOfWar(glm::vec2 startPos, glm::vec2 size);
    void deleteFogOfWar(flecs::entity fogEntity);
    void toggleFogVisibility(flecs::entity fogEntity);
    void handleFogCreation(glm::vec2 mousePos);

    // Camera manipulation
    void panBoard(glm::vec2 currentMousePos);
    void zoomBoard(float zoomFactor);

    // Toolbar tool management
    Tool getCurrentTool() const;
    void setCurrentTool(Tool newTool);

	bool isBoardActive();
    flecs::entity createBoard();
    flecs::entity createBoard(std::string board_name, std::string map_image_path, GLuint texture_id, glm::vec2 size);
	//flecs::entity createBoard(std::string map_image_path);

    void closeBoard();
    void setActiveBoard(flecs::entity board_entity);


    void startMouseDrag(glm::vec2 mousePos);
    void endMouseDrag();
    glm::vec2 getMouseStartPosition() const;
    bool isDragging();
	std::string board_name;
    DirectoryWindow marker_directory;
private:

    //Shader grid_shader;  // Shader used for grid rendering

    //void renderSquareGrid(glm::mat4& mvp, float windowWidth, float windowHeight, const Grid& grid);
    //void renderHexGrid(glm::mat4& mvp, float windowWidth, float windowHeight, const Grid& grid);
    //void addHexagonVertices(float x, float y, float radius, std::vector<glm::vec2>& vertices);
    //void renderLineVertices(const std::vector<glm::vec2>& vertices, glm::mat4& mvp);

    // Snap to grid helper functions
    glm::vec2 snapToSquareGrid(const glm::vec2& position, const Grid& grid);
    glm::vec2 snapToHexGrid(const glm::vec2& position, const Grid& grid);

    // Helper functions for hex grid math
    glm::vec2 worldToAxial(const glm::vec2& pos, const Grid& grid);
    glm::vec2 axialToWorld(const glm::vec2& axialPos, const Grid& grid);
    glm::vec2 roundAxial(const glm::vec2& axial);

    // Render functions (private)
    //void renderImage(GLuint textureID, const Position &pos, const Size &size, glm::mat4 &viewMatrix);
    //void renderGrid(const glm::mat4& viewMatrix, float windowWidth, float windowHeight);
    //void renderMarker(GLuint textureID, const Position &pos, const Size &size, glm::mat4 &viewMatrix);
    //void renderFog(const Position &pos, const Size &size, const glm::mat4 &viewMatrix, const float alpha);

    glm::vec2 mouseStartPos;

	flecs::world ecs;
	flecs::entity active_board = flecs::entity();
    Camera camera;
    Tool currentTool;  // Active tool for interaction
};
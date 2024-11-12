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
#include "NetworkManager.h"

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
	    auto view = glm::scale(glm::mat4(1.0f), glm::vec3(zoomLevel, zoomLevel, 1));
        return glm::translate(view, glm::vec3(-position, 0.0f));
        //return glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(-position, 0.0f)), glm::vec3(zoomLevel));
    } 

    glm::mat4 getProjectionMatrix() const {
	    //auto projection = glm::ortho(0.0f, window_size.x, 0.0f, window_size.y, -1.0f, 1.0f);
	    auto projection = glm::ortho(-window_size.x, window_size.x, window_size.y, -window_size.y, -1.0f, 1.0f);
        return projection;
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
	BoardManager(flecs::world ecs, NetworkManager* network_manager, DirectoryWindow* map_directory);
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
    bool isEditWindowOpen() const;
    void renderEditWindow();
    void startMouseDrag(glm::vec2 mousePos, bool draggingMarker);
    void endMouseDrag();
    glm::vec2 getMouseStartPosition() const;
    bool isPanning();
    bool isDragginMarker();
    glm::vec2 screenToWorldPosition(glm::vec2 screen_position);
    glm::vec2 worldToScreenPosition(glm::vec2 world_position);
    flecs::entity getEntityAtMousePosition(glm::vec2 mouse_position);
    
    // Generates a unique 64-bit ID
    uint64_t generateUniqueId();
    // Finds an entity by its Identifier component with the specified ID
    flecs::entity findEntityById(uint64_t target_id);


    //Network 
    void sendGameState();
    void sendEntityUpdate(flecs::entity entity, MessageType message_type);

    //flecs::entity deserializeBoard(const std::vector<unsigned char>& buffer, size_t& offset);
    //void serializeBoard(flecs::entity board, std::vector<unsigned char>& buffer);
    flecs::entity getActiveBoard() const;
    void loadActiveBoard(const std::string& filePath);
    void saveActiveBoard(const std::string& filePath);
    void saveActiveBoard(std::filesystem::path& filePath);

	std::string board_name;
    DirectoryWindow marker_directory;
    Camera camera;

    bool isCreatingFog() const { return is_creating_fog; };
    bool getShowEditWindow() const { return showEditWindow; };
    void setShowEditWindow(bool show, flecs::entity edit_entity = flecs::entity()) {
        showEditWindow = show; 
        edit_window_entity = edit_entity;
    };

private:
    bool showEditWindow = false;
	flecs::entity edit_window_entity = flecs::entity();
	NetworkManager* network_manager;
    glm::vec2 mouseStartPos;
    DirectoryWindow* map_directory;
	flecs::world ecs;
	flecs::entity active_board = flecs::entity();
    bool is_creating_fog = false;
    Tool currentTool;  // Active tool for interaction
};

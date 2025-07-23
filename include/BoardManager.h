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

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "flecs.h"

#include "Components.h"
#include "DirectoryWindow.h"
//#include "NetworkManager.h"

class Camera {
public:
    Camera() : position(0.0f, 0.0f), zoom_level(1.0f), fbo_dimensions(0,0) {}  // Set initial zoom to 1.0f for no scaling by default

    void pan(glm::vec2 delta) {
        position += delta;
    }

    glm::vec2 fboToNdcPos(glm::vec2 fbo_pixel_top_left_origin) const {
        glm::vec2 ndc;
        // Convert X from [0, fbo_dimensions.x] to [-1, 1]
        ndc.x = (fbo_pixel_top_left_origin.x / fbo_dimensions.x) * 2.0f - 1.0f;

        // Convert Y from [0, fbo_dimensions.y] (top-left, Y-down) to [-1, 1] (center, Y-up)
        // First, normalize Y to [0, 1]: (fbo_pixel_top_left_origin.y / fbo_dimensions.y)
        // Then, invert Y for OpenGL's Y-up: (1.0f - normalized_y)
        // Finally, scale to [-1, 1]: (inverted_normalized_y * 2.0f - 1.0f)
        ndc.y = 1.0f - (fbo_pixel_top_left_origin.y / fbo_dimensions.y) * 2.0f;

        return ndc;
    }

    void zoom(float factor, glm::vec2 mouse_pos_fbo_pixels) {
        glm::vec2 mouse_world_pos_before_zoom = screenToWorldPosition(mouse_pos_fbo_pixels);
        float old_zoom_level = zoom_level;
        zoom_level *= factor;
        zoom_level = glm::clamp(zoom_level, 0.05f, 20.0f); // Example limits
        if (zoom_level == old_zoom_level) {
            return;
        }
        float zoom_ratio = zoom_level / old_zoom_level;
        position = mouse_world_pos_before_zoom - (mouse_world_pos_before_zoom - position) / zoom_ratio;
    }

    void setPosition(glm::vec2 newPosition) {
        position = newPosition;
    }

    glm::vec2 getPosition() const {
        return position;
    }

    void setZoom(float newZoomLevel) {
        zoom_level = newZoomLevel;
        zoom_level = glm::clamp(zoom_level, 0.1f, 10.0f);
    }

    float getZoom() const {
        return zoom_level;
    }

    glm::mat4 getViewMatrix() const {
        return glm::translate(glm::mat4(1.0f), glm::vec3(-position.x, -position.y, 0.0f));
    }

    glm::mat4 getProjectionMatrix() const {
        float half_width = (fbo_dimensions.x / 2.0f) / zoom_level;
        float half_height = (fbo_dimensions.y / 2.0f) / zoom_level;
        return glm::ortho(-half_width, half_width, -half_height, half_height, -1.0f, 1.0f);
    }

    glm::vec2 worldToScreenPosition(glm::vec2 world_position) const {
        glm::vec4 world_homogeneous = glm::vec4(world_position.x, world_position.y, 0.0f, 1.0f);
        glm::vec4 camera_coords = getViewMatrix() * world_homogeneous;
        glm::vec4 clip_coords = getProjectionMatrix() * camera_coords;
        glm::vec2 ndc;
        if (clip_coords.w != 0.0f) { // Avoid division by zero
            ndc.x = clip_coords.x / clip_coords.w;
            ndc.y = clip_coords.y / clip_coords.w;
        }
        else {
            return glm::vec2(NAN, NAN); // Indicate invalid position
        }
        glm::vec2 fbo_pixel_top_left_origin;
        fbo_pixel_top_left_origin.x = (ndc.x + 1.0f) * 0.5f * fbo_dimensions.x;
        fbo_pixel_top_left_origin.y = (1.0f - ndc.y) * 0.5f * fbo_dimensions.y;

        return fbo_pixel_top_left_origin;
    }

    glm::vec2 screenToWorldPosition(glm::vec2 fbo_pixel_top_left_origin) const {
        glm::vec2 ndc;
        ndc.x = (fbo_pixel_top_left_origin.x / fbo_dimensions.x) * 2.0f - 1.0f;
        ndc.y = 1.0f - (fbo_pixel_top_left_origin.y / fbo_dimensions.y) * 2.0f;
        glm::vec4 ndc_homogeneous = glm::vec4(ndc.x, ndc.y, 0.0f, 1.0f);
        glm::mat4 inverse_pv_matrix = glm::inverse(getProjectionMatrix() * getViewMatrix());
        glm::vec4 world_homogeneous = inverse_pv_matrix * ndc_homogeneous;
        glm::vec2 world_position;
        if (world_homogeneous.w != 0.0f) {
            world_position.x = world_homogeneous.x / world_homogeneous.w;
            world_position.y = world_homogeneous.y / world_homogeneous.w;
        }
        else {
            return glm::vec2(NAN, NAN); // Indicate invalid result
        }

        return world_position;
    }

    void setFboDimensions(glm::vec2 dims) {
        fbo_dimensions = dims;
    }

private:
    glm::vec2 position;  // 2D position of the camera (X, Y)
    float zoom_level;     // Zoom level, where 1.0f means no zoom, > 1.0f means zoom in, < 1.0f means zoom out
    glm::vec2 fbo_dimensions; // current_fbo_dimension
};


enum class Tool{ MOVE, FOG, MARKER, SELECT};

//MOVE - Move Camera and Markers;
//FOG - Create FOG and TOGGLE VISIBILITY
//MARKER - Toggle Marker Visibility? MIGHT NOT BE NECESSARY
//SELECT - Select Marker/Fog, opening an edit window(change size, visibility, and delete)

class BoardManager {
public:
	BoardManager(flecs::world ecs, /*NetworkManager* network_manager,*/ std::shared_ptr<DirectoryWindow> map_directory, std::shared_ptr<DirectoryWindow> marker_directory);
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
    //void zoomBoard(float zoomFactor);

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
    //glm::vec2 screenToWorldPosition(glm::vec2 screen_position);
    //glm::vec2 worldToScreenPosition(glm::vec2 world_position);
    flecs::entity getEntityAtMousePosition(glm::vec2 mouse_position);
    
    // Generates a unique 64-bit ID
    uint64_t generateUniqueId();
    // Finds an entity by its Identifier component with the specified ID
    flecs::entity findEntityById(uint64_t target_id);


    //Network 
    void sendGameState();
    void sendEntityUpdate(flecs::entity entity/*, MessageType message_type*/);

    //flecs::entity deserializeBoard(const std::vector<unsigned char>& buffer, size_t& offset);
    //void serializeBoard(flecs::entity board, std::vector<unsigned char>& buffer);
    flecs::entity getActiveBoard() const;
    void loadActiveBoard(const std::string& filePath);
    void saveActiveBoard(const std::string& filePath);
    void saveActiveBoard(std::filesystem::path& filePath);

	std::string board_name;
    std::shared_ptr<DirectoryWindow> map_directory;
    std::shared_ptr<DirectoryWindow> marker_directory;
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
	//NetworkManager* network_manager;
    glm::vec2 mouseStartPos;
	flecs::world ecs;
	flecs::entity active_board = flecs::entity();
    bool is_creating_fog = false;
    Tool currentTool;  // Active tool for interaction
};

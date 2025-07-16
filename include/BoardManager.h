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
    Camera() : position(0.0f, 0.0f), zoom_level(1.0f), window_size(0,0) {}  // Set initial zoom to 1.0f for no scaling by default

    void pan(glm::vec2 delta) {
        position += delta;
    }

    void zoom(float zoomFactor) {
        zoom_level *= zoomFactor;
    }

    void setPosition(glm::vec2 newPosition) {
        position = newPosition;
    }

    glm::vec2 getPosition() const {
        return position;
    }

    void setZoom(float newZoomLevel) {
        zoom_level = newZoomLevel;
    }

    float getZoom() const {
        return zoom_level;
    }

    glm::mat4 getViewMatrix() const {
	    auto view = glm::scale(glm::mat4(1.0f), glm::vec3(zoom_level, zoom_level, 1));
        return glm::translate(view, glm::vec3(-position, 0.0f));
    }

    glm::mat4 getProjectionMatrix() const {
	    auto projection = glm::ortho(-window_size.x, window_size.x, window_size.y, -window_size.y, -1.0f, 1.0f);
        return projection;
    }

    // worldToScreenPosition: Converts a 3D world coordinate to a 2D pixel coordinate
//                         within the FBO, with (0,0) at the top-left.
    glm::vec2 worldToScreenPosition(glm::vec2 world_position) const {
        // 1. Convert World (XY plane, Z=0) to homogeneous coordinates
        glm::vec4 world_homogeneous = glm::vec4(world_position.x, world_position.y, 0.0f, 1.0f);

        // 2. Apply View Matrix: World Space -> Camera Space
        glm::vec4 camera_coords = getViewMatrix() * world_homogeneous;

        // 3. Apply Projection Matrix: Camera Space -> Clip Space
        glm::vec4 clip_coords = getProjectionMatrix() * camera_coords;

        // 4. Convert to Normalized Device Coordinates (NDC)
        //    Divide by w component (perspective divide)
        glm::vec2 ndc;
        if (clip_coords.w != 0.0f) { // Avoid division by zero
            ndc.x = clip_coords.x / clip_coords.w;
            ndc.y = clip_coords.y / clip_coords.w;
        }
        else {
            // Handle error or return a sentinel value if w is zero
            // For orthographic projection, w should always be 1.0, so this path is unlikely
            // to be taken unless there's an issue with matrix construction.
            return glm::vec2(NAN, NAN); // Indicate invalid position
        }

        // 5. Convert NDC to FBO pixels (TOP-LEFT origin)
        //    NDC range: X [-1, 1], Y [-1, 1]
        //    FBO pixel range: X [0, window_size.x], Y [0, window_size.y]
        glm::vec2 fbo_pixel_top_left_origin;
        fbo_pixel_top_left_origin.x = (ndc.x + 1.0f) * 0.5f * window_size.x;
        // For Y, (1.0 - ndc.y) flips the Y-axis from OpenGL's bottom-up NDC to FBO's top-down.
        fbo_pixel_top_left_origin.y = (1.0f - ndc.y) * 0.5f * window_size.y;

        return fbo_pixel_top_left_origin;
    }

    // screenToWorldPosition: Converts a 2D pixel coordinate from the FBO (top-left origin)
    //                        back to a 3D world coordinate (on the Z=0 plane).
    glm::vec2 screenToWorldPosition(glm::vec2 fbo_pixel_top_left_origin) const {
        // 1. Convert FBO pixels (TOP-LEFT origin) to Normalized Device Coordinates (NDC)
        //    FBO pixel range: X [0, window_size.x], Y [0, window_size.y]
        //    NDC range: X [-1, 1], Y [-1, 1]
        glm::vec2 ndc;
        ndc.x = (fbo_pixel_top_left_origin.x / window_size.x) * 2.0f - 1.0f;
        // For Y, (fbo_pixel_top_left_origin.y / window_size.y) gives 0-1, then (1.0 - ...) flips back.
        ndc.y = 1.0f - (fbo_pixel_top_left_origin.y / window_size.y) * 2.0f;

        // For 2D map, assume we're picking on the Z=0 plane in world coordinates.
        // In NDC, this typically means a Z value of 0.0 (middle of the near/far plane).
        // The Z component is less critical for a purely 2D unprojection, but needed for the vec4.
        glm::vec4 ndc_homogeneous = glm::vec4(ndc.x, ndc.y, 0.0f, 1.0f);

        // 2. Compute the Inverse Projection * View matrix
        glm::mat4 inverse_pv_matrix = glm::inverse(getProjectionMatrix() * getViewMatrix());

        // 3. Transform NDC back to World Space
        glm::vec4 world_homogeneous = inverse_pv_matrix * ndc_homogeneous;

        // 4. Perform perspective divide (if necessary, though for orthographic w should be 1.0)
        glm::vec2 world_position;
        if (world_homogeneous.w != 0.0f) {
            world_position.x = world_homogeneous.x / world_homogeneous.w;
            world_position.y = world_homogeneous.y / world_homogeneous.w;
        }
        else {
            // This case should ideally not happen with a valid orthographic matrix.
            return glm::vec2(NAN, NAN); // Indicate invalid result
        }

        return world_position;
    }

    void setFboDimensions(glm::vec2 dims) {
        window_size = dims;
    }

private:
    glm::vec2 position;  // 2D position of the camera (X, Y)
    float zoom_level;     // Zoom level, where 1.0f means no zoom, > 1.0f means zoom in, < 1.0f means zoom out
    glm::vec2 window_size; // current_fbo_dimension
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

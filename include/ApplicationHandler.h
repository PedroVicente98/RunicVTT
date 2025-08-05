#pragma once

#include <GL/glew.h> 
#include <GLFW/glfw3.h>

#include "Renderer.h"
#include "Shader.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "VertexBufferLayout.h"
#include "Texture.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include <filesystem>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "GameTableManager.h"
#include "DirectoryWindow.h"
#include "flecs.h"
#include "PathManager.h"

namespace fs = std::filesystem;

struct MapFBO {
	GLuint fboID;
	GLuint textureID;
	GLuint rboID; // Renderbuffer for depth/stencil (optional but recommended for 3D)
	int width;
	int height;

	MapFBO() {
		this->fboID = 0;
		this->textureID = 0;
		this->rboID = 0;
		this->width = 0;
		this->height = 0;
	}

};
class ApplicationHandler {

public:
	ApplicationHandler(GLFWwindow* window, std::shared_ptr<DirectoryWindow> map_directory, std::shared_ptr<DirectoryWindow> marker_directory);
	~ApplicationHandler();
	int run();

	
private:
	void renderMainMenuBar();
	void renderDockSpace();
	void renderMapFBO(VertexArray &va, IndexBuffer& ib, Shader& shader, Shader& grid_shader, Renderer& renderer);
	void renderActiveGametable();


	void CreateMapFBO(int width, int height);
	void ResizeMapFBO(int newWidth, int newHeight);
	void DeleteMapFBO();

	void GetMousePosInItem(ImVec2& out_item_size, ImVec2& item_screen_pos);

	std::shared_ptr<MapFBO> map_fbo;
	GLFWwindow* window;
	flecs::world ecs;
	GameTableManager game_table_manager;
	std::shared_ptr<DirectoryWindow> map_directory;
	std::shared_ptr<DirectoryWindow> marker_directory;
	bool g_dockspace_initialized = false;

	glm::vec2 current_fbo_mouse_pos = glm::vec2(0,0);
	ImVec2 current_map_relative_mouse_pos = ImVec2(0, 0);


	//DEBUG STUFF --------------------------------------
	bool g_draw_debug_circle = true;
	void DrawDebugCircle(ImVec2 coords, bool is_relative_to_window, ImU32 color, float radius = 10.0f) {
		ImDrawList* draw_list = ImGui::GetForegroundDrawList();
		ImVec2 final_screen_pos = coords;

		if (is_relative_to_window) {
			// This conversion means 'coords' is relative to the current ImGui window's top-left.
			// For debugging mouse clicks on an ImGui::Image, you generally calculate
			// relative to the image, and then add the image's global position.
			// So, for mouse click debug, you'll usually pass false for is_relative_to_window
			// after converting your calculated points back to global screen space.
			final_screen_pos.x += ImGui::GetWindowPos().x;
			final_screen_pos.y += ImGui::GetWindowPos().y;
		}

		draw_list->AddCircleFilled(final_screen_pos, radius, color, 16);
	}

};
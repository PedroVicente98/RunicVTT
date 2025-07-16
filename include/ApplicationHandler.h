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
	void renderMapFBO(VertexArray &va, IndexBuffer& ib, Shader& shader, Renderer& renderer);
	void renderActiveGametable();


	void CreateMapFBO(int width, int height);
	void ResizeMapFBO(int newWidth, int newHeight);
	void DeleteMapFBO();

	bool GetMousePosInItem(ImVec2& out_mouse_pos_in_item, ImVec2& out_item_size);

	std::shared_ptr<MapFBO> map_fbo;
	GLFWwindow* window;
	flecs::world ecs;
	GameTableManager game_table_manager;
	std::shared_ptr<DirectoryWindow> map_directory;
	std::shared_ptr<DirectoryWindow> marker_directory;
	bool g_dockspace_initialized = false;
};
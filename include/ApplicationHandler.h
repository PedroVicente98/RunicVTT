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

class ApplicationHandler {

public:
	ApplicationHandler(GLFWwindow* window, std::shared_ptr<DirectoryWindow> map_directory, std::shared_ptr<DirectoryWindow> marker_directory);
	~ApplicationHandler();
	int run();

	
private:
	void renderMainMenuBar();
	void renderDockSpace();
	void renderActiveGametable(VertexArray &va, IndexBuffer& ib, Shader& shader, Renderer& renderer);
	ImGuiID dockspace_id;
	GLFWwindow* window;
	flecs::world ecs;
	GameTableManager game_table_manager;
	std::shared_ptr<DirectoryWindow> map_directory;
	std::shared_ptr<DirectoryWindow> marker_directory;
	PathManager pathManager = PathManager();
};
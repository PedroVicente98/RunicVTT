#pragma once
#include <GL/glew.h> 
#include <GLFW/glfw3.h>
#include "Shader.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "VertexBufferLayout.h"
#include "Texture.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include "Window.h"
#include <unordered_map>

class MainWindow : public Window::Window {
public:
	MainWindow(GLFWwindow* window_context);
	~MainWindow();
	int setup();
	int create_window(std::string window_name, int window_type);

	void draw();
	void clear();

private:

	struct WindowStruct {
		Window window;
		std::string name;
		WindowType window_type;
	};

	Board* active_board;

	std::unordered_map<std::string, WindowStruct> window_list;
	GLFWwindow* window_context;
};
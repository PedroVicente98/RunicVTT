#pragma once
#include <string>
#include "Renderer.h"
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

class Marker {
public:
	Marker();
	Marker(const std::string& path);
	~Marker();

	void renderEditWindow();
	void setMarkerImage();
	void changeSize();
	void changePosition();
	void addColorCircle();

private:
	std::string image_path;
	unsigned char* image_buffer;
	ImVec2 position;
	ImVec2 size;
	Texture texture;
};
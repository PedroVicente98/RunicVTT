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
private:

	float x; // x-coordinate
	float y; // y-coordinate
	float width; // width of the marker
	float height; // height of the marker
	std::string image_path;
	unsigned char* image_buffer;
	ImVec2 position = { x , y };

	Texture texture;
	Shader shader;
	VertexArray vertex_array;
	VertexBuffer vertex_buffer;
	IndexBuffer index_buffer;
};
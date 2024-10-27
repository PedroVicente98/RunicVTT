#pragma once
#include <string>
#include "Renderer.h"
#include "Shader.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "VertexBufferLayout.h"
#include "Texture.h"

class Marker {
public:
	Marker(const std::string& texturePath, const glm::vec2& pos, const glm::vec2& sz, Renderer& rend);
	~Marker();
	void draw(Shader& shader, const glm::mat4& viewProjMatrix);
	/*void renderEditWindow();
	void setMarkerImage();
	void changeSize();
	void changePosition();
	void addColorCircle();*/
	Renderer renderer;
	glm::vec2 position;
	glm::vec2 size;
	Texture texture;
	VertexArray va;
	VertexBuffer vb;
	IndexBuffer ib;
private:
};

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

class BoardManager {
public:
	BoardManager(flecs::world ecs);
	~BoardManager();
	void renderToolbar();
	void renderMap();
	void handleInput();
	void renderFogOfWar();
	void addMarkerOnClick();
	bool isBoardActive();
	flecs::entity createBoard();

	std::string board_name;
private:
	flecs::world ecs;
	flecs::entity active_board = flecs::entity();

};
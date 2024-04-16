#pragma once
#include <GL/glew.h> 
#include <GLFW/glfw3.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <unordered_map>

#include "GameTable.h"

class ApplicationHandler {
public:
	ApplicationHandler(GLFWwindow* window_context);
	~ApplicationHandler();

	void renderMainMenuBar();
	void renderActiveGameTable();

private:

	void renderCreateGameTablePopup();
	void renderLoadGameTablePopup();
	void renderSaveGameTablePopup();

	void createGameTable();
	void saveGameTable();
	void loadGameTable();
	void setActiveGameTable(GameTable* game_table);
	void closeGameTable();

	GameTable active_game_table;
	GLFWwindow* window_context;
};
	//bool m_ShowNewGamePopup;
	//bool m_ShowLoadGamePopup;
	//std::string m_NewGameName;
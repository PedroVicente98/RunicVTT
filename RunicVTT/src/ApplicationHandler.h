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
	int setup();

	void handleInput();
	void render();
	void clear();
	void renderMainMenuBar();
private:

	GameTable createGameTable();
	void setActiveGameTable(GameTable* game_table);
	void loadRecentGameTables();

	void renderLoadGamePopup();

	void renderNewGamePopup();


	bool m_ShowNewGamePopup;
	bool m_ShowLoadGamePopup;
	std::string m_NewGameName;

	std::unordered_map<unsigned int, GameTable> recent_game_tables;
	GameTable* active_game_table;
	GLFWwindow* window_context;
};
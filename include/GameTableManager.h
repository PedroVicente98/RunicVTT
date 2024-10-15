#pragma once
#include "BoardManager.h"
#include "Chat.h"
#include <vector>

#include "flecs.h"
#include "Components.h"
#include "NetworkManager.h"

class GameTableManager {
public:
	GameTableManager(flecs::world ecs);
	~GameTableManager();

	void saveGameTable();
	void loadGameTable();

	bool isBoardActive();
	bool isGameTableActive();
	bool isConnectionActive();
	
	void openConnection(unsigned short port);
	void closeConnection();

	void resetCamera();

	void createGameTablePopUp();
	void closeGameTablePopUp();
	void createBoardPopUp();

	void closeBoardPopUp();
	void createNetworkPopUp();
	void closeNetworkPopUp();

	void render(VertexArray& va, IndexBuffer& ib, Shader& shader, Renderer& renderer);


	void setInputCallbacks(GLFWwindow* window);
	bool isMouseInsideMapWindow();
	static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
	static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);


	std::string game_table_name; 
	Chat chat;
	DirectoryWindow map_directory;

private:
	NetworkManager network_manager;
	BoardManager board_manager;
	flecs::entity active_game_table = flecs::entity();
	flecs::world ecs;
	glm::vec2 current_mouse_pos;  // Posi��o atual do mouse em snake_case

	char buffer[124] = "";
	char pass_buffer[124] = "";
	char port_buffer[124] = "";
	char network_password[124] = "";

	// Vari�vel para armazenar o caminho do arquivo selecionado
	std::string map_image_path = "";

};
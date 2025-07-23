#pragma once
#include "BoardManager.h"
#include "Chat.h"
#include <vector>

#include "flecs.h"
#include "Components.h"
#include "PathManager.h"

#include "NetworkManager.h"

class GameTableManager {
public:
	GameTableManager(flecs::world ecs, std::shared_ptr<DirectoryWindow> map_directory, std::shared_ptr<DirectoryWindow> marker_directory);
	~GameTableManager();

	void saveGameTable();
	void loadGameTable(std::filesystem::path game_table_file_path);
	void setCameraFboDimensions(glm::vec2 fbo_dimensions);
	bool isBoardActive();
	bool isGameTableActive();
	//bool isConnectionActive();

	//bool isConnected() const;
	
	//void openConnection(unsigned short port);
	//void closeConnection();

	//void processSentMessages();
	//void processReceivedMessages();

	void createGameTablePopUp();
	void closeGameTablePopUp();
	void connectToGameTablePopUp();
	void createBoardPopUp();

	void closeBoardPopUp();
	void createNetworkPopUp();
	void closeNetworkPopUp();
	void openNetworkInfoPopUp();
	void saveBoardPopUp();
	void loadGameTablePopUp();
	void loadBoardPopUp();

	void render(VertexArray& va, IndexBuffer& ib, Shader& shader, Renderer& renderer);


	void handleInputs(glm::vec2 current_mouse_fbo_pos);
	void handleCursorInputs();
	void handleMouseButtonInputs();
	void handleScrollInputs();

	//void setInputCallbacks(GLFWwindow* window);
	/*bool isMouseInsideMapWindow();
	static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
	static void cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
	static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);*/

	void createGameTableFile(flecs::entity game_table);

	std::vector<std::string> listBoardFiles();
	std::vector<std::string> listGameTableFiles();
	void setCameraWindowSizePos(glm::vec2 window_size, glm::vec2 window_pos);


	std::string game_table_name; 
	Chat chat;
	std::shared_ptr<DirectoryWindow> map_directory;
	BoardManager board_manager;
private:

	glm::vec2 convertImGuiScreenToFboPixelsBL(ImVec2 mouse_screen_pos, ImVec2 map_image_min_screen_pos, ImVec2 map_image_actual_displayed_size, int fbo_width, int fbo_height) const;

	// Helper functions for clearer separation of concerns within handleInput
	void handleMouseButtons(glm::vec2 current_mouse_fbo_pixels_bl_origin,int fbo_height);
	void handleCursorMovement(glm::vec2 current_mouse_fbo_pixels_bl_origin);
	void handleScroll(glm::vec2 current_mouse_fbo_pixels_bl_origin);

	NetworkManager network_manager;
	flecs::entity active_game_table = flecs::entity();
	flecs::world ecs;
	glm::vec2 current_mouse_pos;  // Posição atual do mouse em snake_case 

	glm::vec2 current_mouse_ndc_pos;  // Posição atual do mouse em snake_case 
	glm::vec2 current_mouse_world_pos;  // Posição atual do mouse em world coordinates 
	glm::vec2 current_mouse_fbo_pos;  // Posição atual do mouse em fbo pixels coordinates 

	char buffer[124] = "";
	char pass_buffer[124] = "";
	char port_buffer[6] = "7777";

	// Variável para armazenar o caminho do arquivo selecionado
	std::string map_image_path = "";

};

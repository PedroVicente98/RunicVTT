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

	void render(VertexArray& va, IndexBuffer& ib, Shader& shader, Shader& grid_shader, Renderer& renderer);


	void handleInputs(glm::vec2 current_mouse_fbo_pos);
	void handleCursorInputs();
	void handleMouseButtonInputs();
	void handleScrollInputs();

	void createGameTableFile(flecs::entity game_table);

	std::vector<std::string> listBoardFiles();
	std::vector<std::string> listGameTableFiles();
	void setCameraWindowSizePos(glm::vec2 window_size, glm::vec2 window_pos);


	std::string game_table_name; 
	Chat chat;
	std::shared_ptr<DirectoryWindow> map_directory;
	std::shared_ptr<NetworkManager> network_manager;
	BoardManager board_manager;
private:

	void handleMouseButtons(glm::vec2 current_mouse_fbo_pixels_bl_origin,int fbo_height);
	void handleCursorMovement(glm::vec2 current_mouse_fbo_pixels_bl_origin);
	void handleScroll(glm::vec2 current_mouse_fbo_pixels_bl_origin);

	flecs::entity active_game_table = flecs::entity();
	flecs::world ecs;

	glm::vec2 current_mouse_pos;  // Posição atual do mouse em snake_case 

	glm::vec2 current_mouse_ndc_pos;  // Posição atual do mouse em snake_case 
	glm::vec2 current_mouse_world_pos;  // Posição atual do mouse em world coordinates 
	glm::vec2 current_mouse_fbo_pos;  // Posição atual do mouse em fbo pixels coordinates 

	char buffer[124] = "";
	char pass_buffer[124] = "";
	char port_buffer[6] = "7777";
	char username_buffer[124] = "";

	std::string map_image_path = "";

	bool mouse_left_clicked, mouse_right_clicked, mouse_middle_clicked;
	bool mouse_left_released, mouse_right_released, mouse_middle_released;
	float mouse_wheel_delta;
	bool ignore_mouse_until_release = false;
	bool is_non_map_window_hovered = false;

public:
	
	void processMouseInput(bool is_mouse_within_image_bounds) {

		ImGuiIO& io = ImGui::GetIO();

		mouse_wheel_delta = io.MouseWheel;
		auto is_non_map_window_hovered = board_manager.getIsNonMapWindowHovered();

		if ((io.MouseClicked[0] || io.MouseClicked[1] || io.MouseClicked[2]) && !is_mouse_within_image_bounds && !is_non_map_window_hovered) {
			ignore_mouse_until_release = true;
		}
		if (io.MouseReleased[0] || io.MouseReleased[1] || io.MouseReleased[2]) {
			ignore_mouse_until_release = false;
		}

		mouse_left_clicked = mouse_right_clicked = mouse_middle_clicked = false;
		mouse_left_released = mouse_right_released = mouse_middle_released = false;

		if (!ignore_mouse_until_release) {
			if (is_mouse_within_image_bounds && !is_non_map_window_hovered) {
				if (io.MouseClicked[0]) { // LEFT mouse button just clicked
					mouse_left_clicked = true;
				}
				if (io.MouseClicked[1]) { // RIGHT mouse button just clicked
					mouse_right_clicked = true;
				}
				if (io.MouseClicked[2]) { // MIDDLE mouse button just clicked
					mouse_middle_clicked = true;
				}
			}
		}

		if (io.MouseReleased[0]) { // LEFT mouse button just released
			mouse_left_released = true;
		}
		if (io.MouseReleased[1]) { // RIGHT mouse button just released
			mouse_right_released = true;
		}
		if (io.MouseReleased[2]) { // MIDDLE mouse button just released
			mouse_middle_released = true;
		}
	}
};

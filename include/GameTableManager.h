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

	bool isConnected() const;
	
	//void openConnection(unsigned short port);
	//void closeConnection();

	//void processSentMessages();
	//void processReceivedMessages();

	//void createGameTablePopUp();
	//void loadGameTablePopUp();

	void hostGameTablePopUp();
	void networkCenterPopUp();
	void renderNetworkCenterPlayer();
	void renderNetworkCenterGM();
	void connectToGameTablePopUp();
	void closeGameTablePopUp();
	
	void createBoardPopUp();
	void closeBoardPopUp();
	void saveBoardPopUp();
	void loadBoardPopUp();

	void guidePopUp();
	void aboutPopUp();

	//void createNetworkPopUp();
	//void closeNetworkPopUp();
	//void openNetworkInfoPopUp();
	void renderNetworkToasts(std::shared_ptr<NetworkManager> nm);
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

	// Label + value on one line
	void UI_LabelValue(const char* label, const std::string& value) {
		ImGui::TextUnformatted(label);
		ImGui::SameLine();
		ImGui::TextUnformatted(value.c_str());
	}

	// Copy button with timed "Copied!" toast (per-id)
	bool UI_CopyButtonWithToast(const char* btnId, const std::string& toCopy,
		const char* toastId, float seconds = 1.5f) {
		static std::unordered_map<std::string, double> s_toasts; // toastId -> expire time
		bool clicked = ImGui::Button(btnId);
		if (clicked) {
			ImGui::SetClipboardText(toCopy.c_str());
			s_toasts[toastId] = ImGui::GetTime() + seconds;
		}
		if (auto it = s_toasts.find(toastId); it != s_toasts.end()) {
			if (ImGui::GetTime() < it->second) {
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.4f, 1.f, 0.4f, 1.f), "Copied!");
			}
			else {
				s_toasts.erase(it);
			}
		}
		return clicked;
	}

	// Transient colored line (e.g., "Failed to connect!") that auto hides
	void UI_TransientLine(const char* key, bool trigger, const ImVec4& color,
		const char* text, float seconds = 2.0f) {
		static std::unordered_map<std::string, double> s_until; // key -> expire time
		if (trigger) s_until[key] = ImGui::GetTime() + seconds;
		if (auto it = s_until.find(key); it != s_until.end()) {
			if (ImGui::GetTime() < it->second) {
				ImGui::TextColored(color, "%s", text);
			}
			else {
				s_until.erase(it);
			}
		}
	}

	// Shared chunk for entering password + port (returns true if drew fields)
	bool UI_RenderPortAndPassword(char* portBuf, size_t portBufSize,
		char* passBuf, size_t passBufSize) {
		ImGui::InputText("Password", passBuf, passBufSize, ImGuiInputTextFlags_Password);
		ImGui::InputText("Port", portBuf, portBufSize,
			ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_CharsNoBlank);
		return true;
	}

	// Reusable confirm modal
	inline bool UI_ConfirmModal(const char* popupId, const char* title, const char* text) {
	    bool confirmed = false;
	    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	    if (ImGui::BeginPopupModal(popupId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
	        ImGui::TextUnformatted(title);
	        ImGui::Separator();
	        ImGui::TextWrapped("%s", text);
	        ImGui::Separator();
	        if (ImGui::Button("Yes")) { confirmed = true; ImGui::CloseCurrentPopup(); }
	        ImGui::SameLine();
	        if (ImGui::Button("No")) { ImGui::CloseCurrentPopup(); }
	        ImGui::EndPopup();
	    }
	    return confirmed;
	}

};

#include "GameTableManager.h"


GameTableManager::GameTableManager(flecs::world ecs)
    : ecs(ecs), board_manager(ecs)
{
}


GameTableManager::~GameTableManager()
{

}

void GameTableManager::saveGameTable()
{
}

void GameTableManager::loadGameTable()
{
}



bool GameTableManager::isGameTableActive()
{
	return active_game_table.is_valid();
}

bool GameTableManager::isConnectionActive() {
    return network_manager.isConnectionOpen();
}

void GameTableManager::openConnection(unsigned short port) {
    network_manager.startServer(port);
    std::cout << "Connection opened: " << network_manager.getNetworkInfo() << std::endl;
}

void GameTableManager::closeConnection() {
    network_manager.stopServer();
    std::cout << "Connection closed." << std::endl;
}



// ----------------------------- GUI --------------------------------------------------------------------------------
void GameTableManager::createGameTablePopUp()
{   
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("CreateGameTable"))
    {
        ImGui::SetItemDefaultFocus();
        ImGui::InputText("GameTable Name", buffer, sizeof(buffer));
        game_table_name = buffer;

        ImGui::Separator();
        ImGui::InputText("Password", pass_buffer, sizeof(buffer), ImGuiInputTextFlags_Password);
        
        if (ImGui::Button("Save"))
        {
            auto game_table = ecs.entity("GameTable").set(GameTable{ game_table_name });
            active_game_table = game_table;
            ImGui::CloseCurrentPopup();

            memset(buffer, '\0', sizeof(buffer));
            memset(pass_buffer, '\0', sizeof(pass_buffer));
        }

        ImGui::SameLine();

        if (ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
            memset(buffer, '\0', sizeof(buffer));
            memset(pass_buffer, '\0', sizeof(pass_buffer));
        }
        ImGui::EndPopup();
    }
}

void GameTableManager::createBoardPopUp()
{   
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("CreateBoard"))
    {
        ImGui::SetItemDefaultFocus();
        ImGui::InputText("Board Name", buffer, sizeof(buffer));
        game_table_name = buffer;

        ImGui::Separator();
        
        if (ImGui::Button("Save"))
        {
            auto board = board_manager.createBoard();
            board.add(flecs::ChildOf, active_game_table);
            ImGui::CloseCurrentPopup();

            memset(buffer, '\0', sizeof(buffer));

        }

        ImGui::SameLine();

        if (ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
            memset(buffer, '\0', sizeof(buffer));
        }
        ImGui::EndPopup();
    }
}

void GameTableManager::closeGameTablePopUp()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("CloseGameTable"))
    {
        ImGui::Text("Close Current GameTable?? Any unsaved changes will be lost!!");
        if (ImGui::Button("Close"))
        {
            active_game_table = flecs::entity();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}


void GameTableManager::createNetworkPopUp() {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (ImGui::BeginPopupModal("CreateNetwork")) {
        ImGui::SetItemDefaultFocus();

        // Display local IP address (use NetworkManager to get IP)
        std::string localIp = network_manager.getLocalIPAddress();  // New method to get IP address
        ImGui::Text("Local IP Address: %s", localIp.c_str());

        // Input field for port
        ImGui::InputText("Port", port_buffer, sizeof(port_buffer));

        // Optional password field
        ImGui::InputText("Password (optional)", pass_buffer, sizeof(pass_buffer), ImGuiInputTextFlags_Password);

        ImGui::Separator();

        // Save button to start the network
        if (ImGui::Button("Start Network")) {
            unsigned short port = static_cast<unsigned short>(std::stoi(port_buffer));
            // Start the network with the given port and save the password
            network_manager.startServer(port);
            memcpy(network_password, pass_buffer, sizeof(pass_buffer));
            ImGui::CloseCurrentPopup();

            // Clear buffers after saving
            memset(port_buffer, '\0', sizeof(port_buffer));
            memset(pass_buffer, '\0', sizeof(pass_buffer));
        }

        ImGui::SameLine();

        // Close button to exit the pop-up
        if (ImGui::Button("Close")) {
            ImGui::CloseCurrentPopup();
            memset(port_buffer, '\0', sizeof(port_buffer));
            memset(pass_buffer, '\0', sizeof(pass_buffer));
        }
        ImGui::EndPopup();
    }
}


void GameTableManager::closeNetworkPopUp() {
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("CloseNetwork"))
    {
        ImGui::Text("Close Current Conection?? Any unsaved changes will be lost!!");
        if (ImGui::Button("Close Connection"))
        {
            network_manager.stopServer();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

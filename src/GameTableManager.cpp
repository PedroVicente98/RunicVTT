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

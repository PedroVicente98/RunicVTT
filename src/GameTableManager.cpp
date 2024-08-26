#include "GameTableManager.h"


GameTableManager::GameTableManager(flecs::world ecs)
    : ecs(ecs)
{
}


GameTableManager::~GameTableManager()
{

}

flecs::entity GameTableManager::createBoard()
{
	return flecs::entity();
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

        ImGui::Text("GameTable Name");
        ImGui::SameLine();
        ImGui::InputText("Name", buffer, sizeof(buffer));
        game_table_name = buffer;

        ImGui::Separator();
        ImGui::InputText("Password", pass_buffer, sizeof(buffer), ImGuiInputTextFlags_Password);
        
        if (ImGui::Button("Save"))
        {
            auto game_table = ecs.entity("GameTable").set(GameTable{ game_table_name });
            active_game_table = game_table;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Close"))
        {
            ImGui::CloseCurrentPopup();
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
        ImGui::Text("Close Current GameTable??");
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

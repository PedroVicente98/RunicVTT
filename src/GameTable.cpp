#include "GameTable.h"


GameTable::GameTable(unsigned char serialized_data, Renderer& renderer)
	: renderer(renderer), active_board(nullptr)
{
	// Deserialize the data and create a gametable
}

GameTable::GameTable(std::string& game_table_name, Renderer& renderer)
	:renderer(renderer)
{
	this->game_table_name = game_table_name;
	Board* active_board = new Board(renderer,"C:\\Dev\\RunicVTT\\RunicVTT\\res\\textures\\PhandalinBattlemap2.png"); 
	this->active_board = active_board;
	
}

GameTable::~GameTable()
{

}

void GameTable::renderMenu()
{

}

void GameTable::renderActiveBoard()
{
	active_board->renderMap();
	/*ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;
	ImVec2 display_size = ImGui::GetIO().DisplaySize;
	ImVec2 min_size = ImVec2(display_size.x * 0.5f, display_size.y * 0.5f);
	ImVec2 max_size = ImVec2(display_size.x * 3, display_size.y * 3);
	ImGui::SetNextWindowSizeConstraints(min_size , max_size);
	ImGui::SetNextWindowDockID(ImGui::GetID("Root"), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(display_size.x * 0.7f, display_size.y), ImGuiCond_Appearing);

	ImGui::Begin(active_board->board_name.c_str(),NULL, window_flags);
	active_board->handleInput();
	active_board->renderToolbar();
	active_board->renderFogOfWar();
	active_board->renderMap();
	ImGui::End();*/
}


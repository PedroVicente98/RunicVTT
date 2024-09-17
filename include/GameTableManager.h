#pragma once
#include "BoardManager.h"
#include "Chat.h"
#include <vector>

#include "flecs.h"
#include "Components.h"

class GameTableManager {
public:
	GameTableManager(flecs::world ecs);
	~GameTableManager();

	void saveGameTable();
	void loadGameTable();

	bool isBoardActive();
	bool isGameTableActive();

	void createGameTablePopUp();
	void closeGameTablePopUp();
	void createBoardPopUp();

	std::string game_table_name; 
	Chat chat;

private:
	BoardManager board_manager;
	flecs::entity active_game_table = flecs::entity();
	flecs::world ecs;
	char buffer[124] = "";
	char pass_buffer[124] = "";


};
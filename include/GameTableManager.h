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

	void createGameTablePopUp();
	void closeGameTablePopUp();
	void createBoardPopUp();
	void createNetworkPopUp();
	void closeNetworkPopUp();

	std::string game_table_name; 
	Chat chat;

private:
	NetworkManager network_manager;
	BoardManager board_manager;
	flecs::entity active_game_table = flecs::entity();
	flecs::world ecs;
	char buffer[124] = "";
	char pass_buffer[124] = "";
	char port_buffer[124] = "";
	char network_password[124] = "";



};
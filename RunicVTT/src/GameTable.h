#pragma once
#include "Board.h"
#include <unordered_map>

class GameTable {
public:
	void setup(); // when creating a new gametable

	GameTable();
	~GameTable();

	void draw();
	void handleInput();

	unsigned int game_table_id;
	std::string game_table_name;
private:
	//std::unordered_map<std::string, Board> board_list;
	Board board;
	//Connection
	//Board* active_board;

};
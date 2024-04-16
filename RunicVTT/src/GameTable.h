#pragma once
#include "Board.h"
#include <vector>

class GameTable {
public:
	GameTable();
	~GameTable();


	void renderMenu();
	unsigned char serializeData();

	void renderActiveBoard();
	void createBoard();
	void saveBoard();
	void loadBoard();
	void setActiveBoard();

	/*void openConnection();
	void closeConnection();
	void saveConnection();

	void createNote();
	void loadNote();
	void saveNote();
	void openNotesDirectory();*/

	std::string game_table_name;
private:

	Board active_board;
};
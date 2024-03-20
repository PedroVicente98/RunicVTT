#pragma once
#include "Board.h"



class Window {
public:
	enum WindowType {
		BoardType = 0,
		MarkerDirectoryType = 1,
		MapDirectoryType = 2,
		ChatType = 3,
		NetworkLogType = 4,
	};

	Window();
	Window(int window_type);
	~Window();

	void draw(int window_type);
	void clear();

private:
	void draw_OLD();
	Board board;

};
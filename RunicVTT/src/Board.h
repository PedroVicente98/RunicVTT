#pragma once
#include <vector>
#include "Marker.h"

class Board {
public:

	struct ToolbarItem {
		const char* tooltip; // Tooltip text
		const char* iconName; // Icon name (use font icons or texture IDs)
		const char* label; // Label text
		bool clicked; // Clicked state (can be used for button activation)
	};


	Board();
	~Board();

	void draw_ui();

	void draw_map();

	void draw_toolbar();

private:
	std::vector<Marker> board_markers;

	std::vector<ToolbarItem> toolbarItems = {
	   {"Draw", "ICON_FA_PENCIL", "Draw Tool", false},
	   {"Erase", "ICON_FA_ERASER", "Erase Tool", false},
	   // Add more toolbar items as needed
	};
};
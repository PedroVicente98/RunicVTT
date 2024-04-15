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

	void draw();


	Marker create_marker(std::string file_path);

	void setMap(std::string file_path);
	void handleInput();

private:
	void draw_map();
	void draw_toolbar();
	void render();
	void handleMarkers();
	unsigned int width;
	unsigned int height;

	Texture texture;

	std::string map_path;
	std::vector<Marker> board_markers;

	/*std::vector<ToolbarItem> toolbarItems = {
	   {"Draw", "ICON_FA_PENCIL", "Draw Tool", false},
	   {"Erase Marker", "ICON_FA_ERASER", "Erase Tool", false},
	   {"Add Marker", "ICON_FA_ERASER", "Marker Tool", false}
	};*/
};
#pragma once
#include <vector>
#include "Marker.h"

class Board {
public:
	Board();
	~Board();

	void renderToolbar();
	unsigned char serializeData();
	void renderMap();
	void handleInput();
	void setMapImage(std::string file_path);

	void renderFogOfWar();
	void toggleFogOfWar();
	void drawFogOfWar();
	void deleteFogOfWar();

	void createMarker();
	void handleMarkers();
	void deleteMarker();

private:

	struct ToolbarItem {
		const char* tooltip; // Tooltip text
		const char* iconName; // Icon name (use font icons or texture IDs)
		const char* label; // Label text
		bool clicked; // Clicked state (can be used for button activation)
	};
	struct FogOfWar {
		ImVec2 position;
		ImVec2 size;
		bool visible;
	};


	ImVec2 map_size;
	Texture texture;
	std::string map_path;
	std::vector<Marker> board_markers;
	std::vector<FogOfWar> fog_of_war_segments;
	std::vector<ToolbarItem> toolbarItems = {
	   {"Draw", "ICON_FA_PENCIL", "Draw Tool", false},
	   {"Erase Marker", "ICON_FA_ERASER", "Erase Tool", false},
	   {"Add Marker", "ICON_FA_ERASER", "Marker Tool", false}
	};
};
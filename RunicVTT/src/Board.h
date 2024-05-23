#pragma once
#include <vector>
#include "Marker.h"
#include "Renderer.h"
#include "Texture.h"
#include "Shader.h"
#include "VertexArray.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

class Board {
public:
	//Board(const std::string& board_name, const std::string& texturePath, ImVec2 cameraCenter, float initialZoom, int viewportWidth, int viewportHeight);
	Board(Renderer& rend, const std::string& texturePath);
	~Board();
	//unsigned char serializeData();

	void renderToolbar();
	void renderMap();
	void handleInput();
	//void setMapImage(std::string file_path);

	void renderFogOfWar();
	/*void toggleFogOfWar();
	void drawFogOfWar();
	void deleteFogOfWar();

	void createMarker();
	void handleMarkers();
	void deleteMarker();*/

	std::string board_name;
private:

	Texture mapTexture;
	std::vector<Marker> markers;
	glm::mat4 viewProjectionMatrix;
	Renderer& renderer;
	Shader shader;
	VertexArray va;
	VertexBuffer vb;
	IndexBuffer ib;

	void addMarkerOnClick(ImVec2 mousePosition);

	void updateCamera(float zoomLevel, const glm::vec2& panOffset);

	ImVec2 map_size;
	ImVec2 map_position;

	ImVec2 prevMousePos = {0 , 0};



	//Camera camera;
	struct FogOfWar {
		ImVec2 position;
		ImVec2 size;
		bool visible;
	};

	struct ToolbarItem {
		const char* tooltip; // Tooltip text
		const char* iconName; // Icon name (use font icons or texture IDs)
		const char* label; // Label text
		bool clicked; // Clicked state (can be used for button activation)
	};
	std::vector<FogOfWar> fog_of_war_segments;
	std::vector<ToolbarItem> toolbarItems = {
	   {"Draw", "ICON_FA_PENCIL", "Draw Tool", false},
	   {"Erase Marker", "ICON_FA_ERASER", "Erase Tool", false},
	   {"Add Marker", "ICON_FA_ERASER", "Marker Tool", false}
	};
};
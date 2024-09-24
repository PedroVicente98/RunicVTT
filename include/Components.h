#pragma once
// Components.h


#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <GL/glew.h>
#include "flecs.h"
#include <shared_mutex>

// Position Component
struct Position {
    int x;
    int y;
};

// Size Component
struct Size {
    float width;
    float height;
};

// Visibility Component
struct Visibility {
    bool isVisible;
};

// Moving Component
struct Moving {
    bool isDragging;
};

// Texture Component
struct TextureComponent {
    GLuint textureID;
    std::string image_path;
};

// Zoom Component
struct Zoom {
    float zoomLevel;
};

// Panning Component
struct Panning {
    bool isPanning;
};

// Grid Component
struct Grid {
    glm::vec2 offset;
    //float offsetX, offsetY;
    //float scaleX, offsetY;
    glm::vec2 scale;
};

// Board Component
struct Board {
    std::vector<flecs::entity> markers;
    std::vector<flecs::entity> fogOfWar;
};

// Marker Component
struct MarkerComponent {
    bool isWindowActive;
};

// FogOfWar Component
struct FogOfWar {
    bool isWindowActive;
};

// GameTable Component
struct GameTable {
    std::string gameTableName;
};

struct ActiveBoard
{
};


// Network Component
struct Network {
    std::string external_ip;
    std::string internal_ip;
    unsigned short port;
    std::string password;
    std::vector<std::shared_ptr<std::string>> active_peers;
    bool is_gamemaster;
};

// Notes Component
struct Notes {
    std::vector<flecs::entity> notes;
};

// Tool Component (for managing active tools)
struct Tool {
    std::string currentTool;
};

#pragma once
// Components.h

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <GL/glew.h>
#include "flecs.h"
#include <shared_mutex>

// Identifier Component
struct Identifier
{
    uint64_t id; // Unique identifier for each entity
};

// Position Component
struct Position
{
    int x;
    int y;
};

// Size Component
struct Size
{
    float width;
    float height;
};

// Visibility Component
struct Visibility
{
    bool isVisible;
};

// Moving Component
struct Moving
{
    bool isDragging;
};

// Texture Component
struct TextureComponent
{
    GLuint textureID;
    std::string image_path;
    glm::vec2 size;
};

// Zoom Component
//struct Zoom {
//    float zoomLevel;
//};

// Panning Component
struct Panning
{
    bool isPanning;
};

// Grid Component
struct Grid
{
    glm::vec2 offset;
    float cell_size;
    bool is_hex;
    bool snap_to_grid;
    bool visible;
    float opacity;
};

// Board Component
struct Board
{
    std::string board_name;
};

// Marker Component
struct MarkerComponent
{
    std::string ownerPeerId; // "" = no owner
    bool allowAllPlayersMove = false;
    bool locked = false; // hard lock (GM can still move)
};

// FogOfWar Component
struct FogOfWar
{
};

// GameTable Component
struct GameTable
{
    std::string gameTableName;
};

struct ActiveBoard
{
};

// Notes Component
struct Notes
{
    std::vector<flecs::entity> notes;
};

struct NoteComponent
{
    std::string uuid;
    std::string title;
    std::string markdown_text;
    std::string author;
    std::string creation_date;
    std::string last_update;

    bool open_editor = false;
    bool selected = false;
    bool shared = false;
    bool saved_locally = false;
    bool has_conflict = false;
    std::string shared_from;
};

// Network Component
//struct Network {
//    std::string external_ip;
//    std::string internal_ip;
//    unsigned short port;
//    std::string password;
//    std::vector<std::shared_ptr<std::string>> active_peers;
//    bool is_gamemaster;
//};
//
//struct PeerInfo {
//    std::string id;
//    std::string username;
//    bool isAuthenticated = false;
//    bool isGameMaster = false;
//};

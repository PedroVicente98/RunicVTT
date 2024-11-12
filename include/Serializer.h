#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <iostream>
#include <cstring>  // For memcpy
#include "Components.h"  // Include your components header

class Serializer {
public:
    // Serialize methods for basic types
    static void serializeInt(std::vector<unsigned char>& buffer, int value);
    static void serializeFloat(std::vector<unsigned char>& buffer, float value);
    static void serializeBool(std::vector<unsigned char>& buffer, bool value);
    static void serializeString(std::vector<unsigned char>& buffer, const std::string& str);
    static void serializeVec2(std::vector<unsigned char>& buffer, const glm::vec2& vec);
    static void Serializer::serializeUInt64(std::vector<unsigned char>& buffer, uint64_t value);

    // Deserialize methods for basic types
    static int deserializeInt(const std::vector<unsigned char>& buffer, size_t& offset);
    static float deserializeFloat(const std::vector<unsigned char>& buffer, size_t& offset);
    static bool deserializeBool(const std::vector<unsigned char>& buffer, size_t& offset);
    static std::string deserializeString(const std::vector<unsigned char>& buffer, size_t& offset);
    static glm::vec2 deserializeVec2(const std::vector<unsigned char>& buffer, size_t& offset);
    static uint64_t Serializer::deserializeUInt64(const std::vector<unsigned char>& buffer, size_t& offset);

    // Serialize methods for components
    static void serializePosition(std::vector<unsigned char>& buffer, const Position* position);
    static Position deserializePosition(const std::vector<unsigned char>& buffer, size_t& offset);

    static void serializeSize(std::vector<unsigned char>& buffer, const Size* size);
    static Size deserializeSize(const std::vector<unsigned char>& buffer, size_t& offset);

    static void serializeVisibility(std::vector<unsigned char>& buffer, const Visibility* visibility);
    static Visibility deserializeVisibility(const std::vector<unsigned char>& buffer, size_t& offset);

    static void serializeMoving(std::vector<unsigned char>& buffer, const Moving* moving);
    static Moving deserializeMoving(const std::vector<unsigned char>& buffer, size_t& offset);

    static void serializeTextureComponent(std::vector<unsigned char>& buffer, const TextureComponent* texture);
    static TextureComponent deserializeTextureComponent(const std::vector<unsigned char>& buffer, size_t& offset);

    static void serializePanning(std::vector<unsigned char>& buffer, const Panning* panning);
    static Panning deserializePanning(const std::vector<unsigned char>& buffer, size_t& offset);

    static void serializeGrid(std::vector<unsigned char>& buffer, const Grid* grid);
    static Grid deserializeGrid(const std::vector<unsigned char>& buffer, size_t& offset);

    static void serializeBoard(std::vector<unsigned char>& buffer, const Board* board);
    static Board deserializeBoard(const std::vector<unsigned char>& buffer, size_t& offset);

    static void serializeGameTable(std::vector<unsigned char>& buffer, const GameTable* gameTable);
    static GameTable deserializeGameTable(const std::vector<unsigned char>& buffer, size_t& offset);

    static void serializeMarkerEntity(std::vector<unsigned char>& buffer, const flecs::entity entity, flecs::world& ecs);
    static flecs::entity deserializeMarkerEntity(const std::vector<unsigned char>& buffer, size_t& offset, flecs::world& ecs);

    static void serializeFogEntity(std::vector<unsigned char>& buffer, const flecs::entity entity, flecs::world& ecs);
    static flecs::entity deserializeFogEntity(const std::vector<unsigned char>& buffer, size_t& offset, flecs::world& ecs);


    static void serializeGameTableEntity(std::vector<unsigned char>& buffer, const flecs::entity entity, flecs::world& ecs);
    static flecs::entity deserializeGameTableEntity(const std::vector<unsigned char>& buffer, size_t& offset, flecs::world& ecs);

    static void serializeBoardEntity(std::vector<unsigned char>& buffer, const flecs::entity entity, flecs::world& ecs);
    static flecs::entity deserializeBoardEntity(const std::vector<unsigned char>& buffer, size_t& offset, flecs::world& ecs);

};


// Serialize and Deserialize MarkerEntity
inline void Serializer::serializeMarkerEntity(std::vector<unsigned char>& buffer, const flecs::entity entity, flecs::world& ecs) {
    auto position = entity.get<Position>();
    auto size = entity.get<Size>();
    auto texture = entity.get<TextureComponent>();
    auto visibility = entity.get<Visibility>();
    auto moving = entity.get<Moving>();
    auto identifier = entity.get<Identifier>();

    serializeUInt64(buffer, identifier->id);  
    serializePosition(buffer, position);
    serializeSize(buffer, size);
    serializeMoving(buffer, moving);
    serializeVisibility(buffer, visibility);
    serializeTextureComponent(buffer, texture);

}

inline flecs::entity Serializer::deserializeMarkerEntity(const std::vector<unsigned char>& buffer, size_t& offset, flecs::world& ecs) {

    uint64_t marker_id = Serializer::deserializeUInt64(buffer, offset);
    auto position = deserializePosition(buffer, offset);
    auto size = deserializeSize(buffer, offset);
    auto moving = deserializeMoving(buffer, offset);
    auto visibility = deserializeVisibility(buffer, offset);
    auto texture = deserializeTextureComponent(buffer, offset);

    auto marker = ecs.entity()
        .set<Identifier>({ marker_id })
        .set<Position>(position)
        .set<Size>(size)
        .set<Moving>(moving)
        .set<Visibility>(visibility)
        .set<TextureComponent>({ 0 , texture.image_path  , texture.size });

    return marker;
}

// Serialize and Deserialize FogEntity
inline void Serializer::serializeFogEntity(std::vector<unsigned char>& buffer, const flecs::entity entity, flecs::world& ecs) {
    auto position = entity.get<Position>();
    auto size = entity.get<Size>();
    auto visibility = entity.get<Visibility>();
    auto identifier = entity.get<Identifier>();

    serializeUInt64(buffer, identifier->id);  
    serializePosition(buffer, position);
    serializeSize(buffer, size);
    serializeVisibility(buffer, visibility);
}

inline flecs::entity Serializer::deserializeFogEntity(const std::vector<unsigned char>& buffer, size_t& offset, flecs::world& ecs) {
    
    uint64_t fog_id = Serializer::deserializeUInt64(buffer, offset);
    auto position = deserializePosition(buffer, offset);
    auto size = deserializeSize(buffer, offset);
    auto visibility = deserializeVisibility(buffer, offset);

    auto fog = ecs.entity()
        .set<Identifier>({ fog_id })
        .set<Position>(position)
        .set<Size>(size)
        .set<Visibility>(visibility);

    return fog;
}


inline void Serializer::serializeGameTableEntity(std::vector<unsigned char>& buffer, const flecs::entity entity, flecs::world& ecs)
{
    auto game_table = entity.get<GameTable>();
    //auto game_table_id = entity.id();

    //serializeInt(buffer, game_table_id);
    serializeGameTable(buffer, game_table);

    int boardCount = 0;
    ecs.defer_begin();
    entity.children([&](flecs::entity child) {
        if (child.has<Board>()) {
            boardCount++;
        }
    });
    ecs.defer_end();
    Serializer::serializeInt(buffer, boardCount);
    ecs.defer_begin();
    entity.children([&](flecs::entity child) {
        serializeBoardEntity(buffer, child, ecs);
    });
    ecs.defer_end();

}

inline flecs::entity Serializer::deserializeGameTableEntity(const std::vector<unsigned char>& buffer, size_t& offset, flecs::world& ecs)
{
    //auto game_table_id = deserializeInt(buffer, offset);
    auto game_table = deserializeGameTable(buffer, offset);
    auto game_table_entity = ecs.entity().set<GameTable>(game_table);

    int board_count = Serializer::deserializeInt(buffer, offset);
    for (int i = 1; i <= board_count; i++) {
        auto board_entity = deserializeBoardEntity(buffer, offset, ecs);
        board_entity.add(flecs::ChildOf, game_table_entity);
    }

    return game_table_entity;
}

inline void Serializer::serializeBoardEntity(std::vector<unsigned char>& buffer, const flecs::entity entity, flecs::world& ecs)
{
    auto boardData = entity.get<Board>();
    auto panning = entity.get<Panning>();
    auto position = entity.get<Position>();
    auto grid = entity.get<Grid>();
    auto texture = entity.get<TextureComponent>();
    auto size = entity.get<Size>();
    auto identifier = entity.get<Identifier>();

    Serializer::serializeUInt64(buffer, identifier->id);  
    Serializer::serializeBoard(buffer, boardData);
    Serializer::serializePanning(buffer, panning);
    Serializer::serializePosition(buffer, position);
    Serializer::serializeGrid(buffer, grid);
    Serializer::serializeTextureComponent(buffer, texture);
    Serializer::serializeSize(buffer, size);

    // Serialize markers
    int markerCount = 0;
    ecs.defer_begin();
    entity.children([&](flecs::entity child) {
        if (child.has<MarkerComponent>()) {
            markerCount++;
        }
     });
    ecs.defer_end();
    Serializer::serializeInt(buffer, markerCount);
    // Serialize each marker's data
    ecs.defer_begin();
    entity.children([&](flecs::entity child) {
        if (child.has<MarkerComponent>()) {
            serializeMarkerEntity(buffer, child, ecs);
        }
    });
    ecs.defer_end();
    // Serialize fog entities
    int fogCount = 0;
    ecs.defer_begin();
    entity.children([&](flecs::entity child) {
        if (child.has<FogOfWar>()) {
            fogCount++;
        }
    });
    ecs.defer_end();
    Serializer::serializeInt(buffer, fogCount);
    // Serialize each fog's data
    ecs.defer_begin();
    entity.children([&](flecs::entity child) {
        if (child.has<FogOfWar>()) {
            serializeFogEntity(buffer, child, ecs);
        }
    });
    ecs.defer_end();
}

inline flecs::entity Serializer::deserializeBoardEntity(const std::vector<unsigned char>& buffer, size_t& offset, flecs::world& ecs)
{

    uint64_t board_id = Serializer::deserializeUInt64(buffer, offset);
    auto boardData = Serializer::deserializeBoard(buffer, offset);
    auto panning = Serializer::deserializePanning(buffer, offset);
    auto position = Serializer::deserializePosition(buffer, offset);
    auto grid = Serializer::deserializeGrid(buffer, offset);
    auto texture = Serializer::deserializeTextureComponent(buffer, offset);
    auto size = Serializer::deserializeSize(buffer, offset);

    auto newBoard = ecs.entity()
        .set<Identifier>({ board_id })
        .set<Board>(boardData)
        .set<Panning>(panning)
        .set<Position>(position)
        .set<Grid>(grid)
        .set<TextureComponent>({ 0 ,texture.image_path , texture.size })
        .set<Size>(size);

    // Deserialize markers
    int markerCount = Serializer::deserializeInt(buffer, offset);
    for (int i = 0; i < markerCount; ++i) {
        auto marker = deserializeMarkerEntity(buffer, offset, ecs);
        marker.add<MarkerComponent>();
        marker.add(flecs::ChildOf, newBoard);

    }

    // Deserialize fog entities
    int fogCount = Serializer::deserializeInt(buffer, offset);
    for (int i = 0; i < fogCount; ++i) {
        auto fog = deserializeFogEntity(buffer, offset, ecs);
        fog.add<FogOfWar>();
        fog.add(flecs::ChildOf, newBoard);
    }

    return newBoard;
}


// Implementation
inline void Serializer::serializeInt(std::vector<unsigned char>& buffer, int value) {
    buffer.insert(buffer.end(), reinterpret_cast<unsigned char*>(&value), reinterpret_cast<unsigned char*>(&value) + sizeof(int));
}

inline void Serializer::serializeFloat(std::vector<unsigned char>& buffer, float value) {
    buffer.insert(buffer.end(), reinterpret_cast<unsigned char*>(&value), reinterpret_cast<unsigned char*>(&value) + sizeof(float));
}

inline void Serializer::serializeBool(std::vector<unsigned char>& buffer, bool value) {
    unsigned char byteValue = value ? 1 : 0;
    buffer.push_back(byteValue);
}

inline void Serializer::serializeString(std::vector<unsigned char>& buffer, const std::string& str) {
    int length = static_cast<int>(str.size());
    serializeInt(buffer, length);
    buffer.insert(buffer.end(), str.begin(), str.end());
}

inline void Serializer::serializeVec2(std::vector<unsigned char>& buffer, const glm::vec2& vec) {
    serializeFloat(buffer, vec.x);
    serializeFloat(buffer, vec.y);
}

inline void Serializer::serializeUInt64(std::vector<unsigned char>& buffer, uint64_t value) {
    buffer.insert(buffer.end(), reinterpret_cast<unsigned char*>(&value), reinterpret_cast<unsigned char*>(&value) + sizeof(uint64_t));
}

inline uint64_t Serializer::deserializeUInt64(const std::vector<unsigned char>& buffer, size_t& offset) {
    uint64_t value = *reinterpret_cast<const uint64_t*>(&buffer[offset]);
    offset += sizeof(uint64_t);
    return value;
}

inline int Serializer::deserializeInt(const std::vector<unsigned char>& buffer, size_t& offset) {
    int value = *reinterpret_cast<const int*>(&buffer[offset]);
    offset += sizeof(int);
    return value;
}

inline float Serializer::deserializeFloat(const std::vector<unsigned char>& buffer, size_t& offset) {
    float value = *reinterpret_cast<const float*>(&buffer[offset]);
    offset += sizeof(float);
    return value;
}

inline bool Serializer::deserializeBool(const std::vector<unsigned char>& buffer, size_t& offset) {
    bool value = buffer[offset] != 0;
    offset += sizeof(unsigned char);
    return value;
}

inline std::string Serializer::deserializeString(const std::vector<unsigned char>& buffer, size_t& offset) {
    int length = deserializeInt(buffer, offset);
    std::string str(buffer.begin() + offset, buffer.begin() + offset + length);
    offset += length;
    return str;
}

inline glm::vec2 Serializer::deserializeVec2(const std::vector<unsigned char>& buffer, size_t& offset) {
    float x = deserializeFloat(buffer, offset);
    float y = deserializeFloat(buffer, offset);
    return glm::vec2(x, y);
}

inline void Serializer::serializePosition(std::vector<unsigned char>& buffer, const Position* position) {
    serializeInt(buffer, position->x);
    serializeInt(buffer, position->y);
}

inline Position Serializer::deserializePosition(const std::vector<unsigned char>& buffer, size_t& offset) {
    int x = deserializeInt(buffer, offset);
    int y = deserializeInt(buffer, offset);
    return Position{ x, y };
}

inline void Serializer::serializeSize(std::vector<unsigned char>& buffer, const Size* size) {
    serializeFloat(buffer, size->width);
    serializeFloat(buffer, size->height);
}

inline Size Serializer::deserializeSize(const std::vector<unsigned char>& buffer, size_t& offset) {
    float width = deserializeFloat(buffer, offset);
    float height = deserializeFloat(buffer, offset);
    return Size{ width, height };
}


// Example for Visibility component:
inline void Serializer::serializeVisibility(std::vector<unsigned char>& buffer, const Visibility* visibility) {
    serializeBool(buffer, visibility->isVisible);
}

inline Visibility Serializer::deserializeVisibility(const std::vector<unsigned char>& buffer, size_t& offset) {
    bool isVisible = deserializeBool(buffer, offset);
    return Visibility{ isVisible };
}

// Serialize and Deserialize Moving
inline void Serializer::serializeMoving(std::vector<unsigned char>& buffer, const Moving* moving) {
    serializeBool(buffer, moving->isDragging);
}

inline Moving Serializer::deserializeMoving(const std::vector<unsigned char>& buffer, size_t& offset) {
    bool isDragging = deserializeBool(buffer, offset);
    return Moving{ isDragging };
}

// Serialize and Deserialize TextureComponent
inline void Serializer::serializeTextureComponent(std::vector<unsigned char>& buffer, const TextureComponent* texture) {
    serializeInt(buffer, static_cast<int>(texture->textureID));
    serializeString(buffer, texture->image_path);
    serializeVec2(buffer, texture->size);
}

inline TextureComponent Serializer::deserializeTextureComponent(const std::vector<unsigned char>& buffer, size_t& offset) {
    GLuint textureID = static_cast<GLuint>(deserializeInt(buffer, offset));
    std::string image_path = deserializeString(buffer, offset);
    glm::vec2 size = deserializeVec2(buffer, offset);
    return TextureComponent{ 0, image_path, size };
}

// Serialize and Deserialize Panning
inline void Serializer::serializePanning(std::vector<unsigned char>& buffer, const Panning* panning) {
    serializeBool(buffer, panning->isPanning);
}

inline Panning Serializer::deserializePanning(const std::vector<unsigned char>& buffer, size_t& offset) {
    bool isPanning = deserializeBool(buffer, offset);
    return Panning{ isPanning };
}

// Serialize and Deserialize Grid
inline void Serializer::serializeGrid(std::vector<unsigned char>& buffer, const Grid* grid) {
    serializeVec2(buffer, grid->offset);
    serializeVec2(buffer, grid->scale);
    serializeBool(buffer, grid->is_hex);
    serializeBool(buffer, grid->snap_to_grid);
}

inline Grid Serializer::deserializeGrid(const std::vector<unsigned char>& buffer, size_t& offset) {
    glm::vec2 offset_val = deserializeVec2(buffer, offset);
    glm::vec2 scale = deserializeVec2(buffer, offset);
    bool is_hex = deserializeBool(buffer, offset);
    bool snap_to_grid = deserializeBool(buffer, offset);
    return Grid{ offset_val, scale, is_hex, snap_to_grid };
}

// Serialize and Deserialize Board
inline void Serializer::serializeBoard(std::vector<unsigned char>& buffer, const Board* board) {
    serializeString(buffer, board->board_name);
}

inline Board Serializer::deserializeBoard(const std::vector<unsigned char>& buffer, size_t& offset) {
    std::string board_name = deserializeString(buffer, offset);
    return Board{ board_name };
}

// Serialize and Deserialize GameTable
inline void Serializer::serializeGameTable(std::vector<unsigned char>& buffer, const GameTable* gameTable) {
    serializeString(buffer, gameTable->gameTableName);
}

inline GameTable Serializer::deserializeGameTable(const std::vector<unsigned char>& buffer, size_t& offset) {
    std::string gameTableName = deserializeString(buffer, offset);
    return GameTable{ gameTableName };
}




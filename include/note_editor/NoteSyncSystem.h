#pragma once
#include "flecs.h"
#include "Components.h"
#include <string>

class NoteSyncSystem {
public:
    NoteSyncSystem(flecs::world& world);

    void broadcastNote(const NoteComponent& note);
    void receiveNoteJson(const std::string& jsonStr, const std::string& sender);

    //static nlohmann::json noteToJson(const NoteComponent& note);
    //static NoteComponent jsonToNote(const nlohmann::json& j);

private:
    flecs::world& m_world;
};

#pragma once
#include "flecs.h"
#include <iostream>
#include "Components.h"

class NoteManager {
public:
    NoteManager(flecs::world& world, const std::string& noteDir = "notes/");

    void loadAllNotesFromDisk();
    void saveNoteToDisk(const NoteComponent& note);
    void saveNoteToDiskAs(const NoteComponent& note, const std::string& filename);
    NoteComponent parseNoteFromFile(const std::string& path);

    NoteComponent createNewNote(const std::string& author);
    std::string generateUUID();

    void discardUnsavedSharedNotes();

private:
    flecs::world& m_world;
    std::string m_noteDirectory;

};

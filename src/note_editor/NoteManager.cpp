#include "NoteManager.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>

namespace fs = std::filesystem;

NoteManager::NoteManager(flecs::world& world, const std::string& noteDir) :
    m_world(world), m_noteDirectory(noteDir)
{
}

void NoteManager::loadAllNotesFromDisk()
{
    if (!fs::exists(m_noteDirectory))
    {
        fs::create_directory(m_noteDirectory);
        return;
    }

    for (const auto& file : fs::directory_iterator(m_noteDirectory))
    {
        if (!file.is_regular_file() || file.path().extension() != ".md")
            continue;

        NoteComponent note = parseNoteFromFile(file.path().string());

        // Assign UUID if missing
        if (note.uuid.empty())
        {
            note.uuid = generateUUID();
        }

        note.saved_locally = true;
        note.shared = false;

        m_world.entity(note.uuid.c_str())
            .set<NoteComponent>(note);
    }
}

NoteComponent NoteManager::parseNoteFromFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
        return {};

    NoteComponent note;
    std::ostringstream body;
    std::string line;
    bool inMeta = false;
    bool metaDone = false;

    while (std::getline(file, line))
    {
        if (!metaDone)
        {
            if (line == "---")
            {
                inMeta = !inMeta;
                if (!inMeta)
                    metaDone = true;
                continue;
            }

            if (inMeta)
            {
                auto colon = line.find(':');
                if (colon != std::string::npos)
                {
                    std::string key = line.substr(0, colon);
                    std::string value = line.substr(colon + 1);
                    value.erase(0, value.find_first_not_of(" \t"));

                    if (key == "uuid")
                        note.uuid = value;
                    else if (key == "title")
                        note.title = value;
                    else if (key == "author")
                        note.author = value;
                    else if (key == "creation_date")
                        note.creation_date = value;
                    else if (key == "last_update")
                        note.last_update = value;
                }
                continue;
            }
        }
        else
        {
            body << line << '\n';
        }
    }

    note.markdown_text = body.str();
    return note;
}

std::string NoteManager::generateUUID()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    const char* hex_chars = "0123456789abcdef";
    std::string uuid = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    for (auto& c : uuid)
    {
        if (c == 'x')
            c = hex_chars[dis(gen)];
        else if (c == 'y')
            c = hex_chars[(dis(gen) & 0x3) | 0x8];
    }
    return uuid;
}

void NoteManager::discardUnsavedSharedNotes()
{
    std::vector<flecs::entity> to_delete;

    m_world.each<NoteComponent>([&](flecs::entity e, NoteComponent& note)
                                {
        if (note.shared && !note.saved_locally) {
            to_delete.push_back(e);
        } });

    for (auto& e : to_delete)
    {
        e.destruct();
    }
}

NoteComponent NoteManager::createNewNote(const std::string& author)
{
    std::string uuid = generateUUID();
    std::string title = "Note_" + uuid.substr(0, 8); // Shortened title

    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    std::string now_str = std::ctime(&now_c);
    now_str.pop_back(); // remove newline

    NoteComponent note;
    note.uuid = uuid;
    note.title = title;
    note.author = author;
    note.creation_date = now_str;
    note.last_update = now_str;
    note.markdown_text = "# " + title + "\n\n";
    note.open_editor = true;
    note.selected = true;
    note.saved_locally = true;

    m_world.entity(uuid.c_str())
        .set<NoteComponent>(note);

    return note;
}

void NoteManager::saveNoteToDisk(const NoteComponent& note)
{
}
void NoteManager::saveNoteToDiskAs(const NoteComponent& note, const std::string& filename)
{
}
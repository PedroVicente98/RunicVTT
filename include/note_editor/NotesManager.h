#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <optional>
#include <filesystem>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <memory>
#include "ImGuiToaster.h"

// Plain Note model (no ECS)
struct Note
{
    // Identity & placement
    std::string uuid;                      // stable identifier (globally unique)
    std::optional<std::string> table_name; // optional GameTable name
    std::filesystem::path file_path;       // where it's saved (empty if never saved)

    // Metadata
    std::string title;
    std::string author;
    std::chrono::system_clock::time_point creation_ts{};
    std::chrono::system_clock::time_point last_update_ts{};

    // Content
    std::string markdown_text;

    // Runtime flags
    bool saved_locally = false;
    bool dirty = false;
    bool shared = false;
    std::optional<std::string> shared_from;
    bool inbox = false;

    // UI hint
    bool open_editor = true;
};

// Where notes live on disk
struct NotesManagerConfig
{
    std::filesystem::path globalNotesDir;    // e.g., <root>/Notes
    std::filesystem::path gameTablesRootDir; // e.g., <root>/GameTables
};

class NotesManager
{
public:
    NotesManager(NotesManagerConfig cfg, std::shared_ptr<ImGuiToaster> toaster);

    // Loaders
    void loadAllFromDisk();
    void loadFromGlobal();
    void loadFromTable(const std::string& tableName);

    // CRUD
    std::string createNote(std::string title,
                           std::string author,
                           std::optional<std::string> tableName = std::nullopt);
    bool saveNote(const std::string& uuid);
    bool saveNoteAs(const std::string& uuid, const std::filesystem::path& absolutePath);
    bool deleteNote(const std::string& uuid, bool deleteFromDisk);

    // Access (shared_ptr only)
    std::shared_ptr<Note> getNote(const std::string& uuid);
    std::shared_ptr<const Note> getNote(const std::string& uuid) const;

    std::vector<std::shared_ptr<Note>> listAll();
    std::vector<std::shared_ptr<Note>> listMyNotes(); // !inbox
    std::vector<std::shared_ptr<Note>> listInbox();   // inbox only

    // Mutations
    void setTitle(const std::string& uuid, std::string title);
    void setContent(const std::string& uuid, std::string md);
    void setAuthor(const std::string& uuid, std::string author);

    // Shared Inbox (future network will call these)
    std::string upsertSharedIncoming(std::string uuid,
                                     std::string title,
                                     std::string author,
                                     std::string markdown,
                                     std::optional<std::string> fromPeerName);

    bool saveInboxToLocal(const std::string& uuid,
                          std::optional<std::string> tableName = std::nullopt);

    // Utilities
    static std::string generateUUID();
    static std::string slugify(const std::string& title);
    static int64_t toEpochMillis(std::chrono::system_clock::time_point tp);
    static std::chrono::system_clock::time_point fromEpochMillis(int64_t ms);

    static bool parseMarkdownFile(const std::filesystem::path& path, Note& outNote);
    static bool writeMarkdownFile(const std::filesystem::path& path, const Note& note);

    std::filesystem::path defaultSavePath(const Note& note) const;

    // lookups
    std::shared_ptr<Note> getByUuid(const std::string& uuid) const;

    // resolve "ref" (uuid, title, short-id) -> UUID string (or empty if not found)
    std::string resolveRef(const std::string& ref) const;

    // (Called when notes are loaded/created/renamed)
    void indexNote(const std::shared_ptr<Note>& n);
    void removeFromIndex(const std::string& uuid);

private:
    // canonical store
    std::unordered_map<std::string, std::shared_ptr<Note>> notesByUuid_;

    // title → uuid (case-insensitive; exact match only)
    std::unordered_map<std::string, std::string> titleToUuid_;

    // helpers
    static bool looksLikeUuid_(const std::string& s);
    static bool looksLikeShortHex_(const std::string& s);
    static std::string toLower_(const std::string& s);

    NotesManagerConfig cfg_;
    std::shared_ptr<ImGuiToaster> toaster_;

    mutable std::mutex mtx_;
    std::unordered_map<std::string, std::shared_ptr<Note>> notesById_;

    void scanFolderForMd_(const std::filesystem::path& dir,
                          std::optional<std::string> tableName);

    // Toaster wrappers
    void toastInfo(const std::string& msg) const;
    void toastGood(const std::string& msg) const;
    void toastWarn(const std::string& msg) const;
    void toastError(const std::string& msg) const;
};

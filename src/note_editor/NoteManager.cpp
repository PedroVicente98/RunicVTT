#include "NotesManager.h"
#include "ImGuiToaster.h"

#include <fstream>
#include <sstream>
#include <random>
#include <iomanip>
#include <algorithm>
#include <cctype>


using Clock = std::chrono::system_clock;

static bool isMarkdownExt(const std::filesystem::path& p) {
    auto ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return (ext == ".md" || ext == ".markdown");
}

NotesManager::NotesManager(NotesManagerConfig cfg, std::shared_ptr<ImGuiToaster> toaster)
    : cfg_(std::move(cfg)), toaster_(std::move(toaster)) {
    std::error_code ec;
    if (!cfg_.globalNotesDir.empty())
        std::filesystem::create_directories(cfg_.globalNotesDir, ec);
    if (!cfg_.gameTablesRootDir.empty())
        std::filesystem::create_directories(cfg_.gameTablesRootDir, ec);
}

void NotesManager::loadAllFromDisk() {
    std::lock_guard<std::mutex> lk(mtx_);
    notesById_.clear();

    scanFolderForMd_(cfg_.globalNotesDir, std::nullopt);

    if (!cfg_.gameTablesRootDir.empty() && std::filesystem::exists(cfg_.gameTablesRootDir)) {
        for (auto& dir : std::filesystem::directory_iterator(cfg_.gameTablesRootDir)) {
            if (!dir.is_directory()) continue;
            auto tableName = dir.path().filename().string();
            auto notesDir = dir.path() / "Notes";
            scanFolderForMd_(notesDir, tableName);
        }
    }
    toastInfo("Notes loaded from disk.");
}

void NotesManager::loadFromGlobal() {
    std::lock_guard<std::mutex> lk(mtx_);
    scanFolderForMd_(cfg_.globalNotesDir, std::nullopt);
    toastInfo("Global notes loaded.");
}

void NotesManager::loadFromTable(const std::string& tableName) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto notesDir = cfg_.gameTablesRootDir / tableName / "Notes";
    scanFolderForMd_(notesDir, tableName);
    toastInfo("Table notes loaded: " + tableName);
}

void NotesManager::scanFolderForMd_(const std::filesystem::path& dir,
                                    std::optional<std::string> tableName) {
    std::error_code ec;
    if (dir.empty() || !std::filesystem::exists(dir, ec)) return;

    for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        if (!isMarkdownExt(entry.path())) continue;

        Note temp;
        if (parseMarkdownFile(entry.path(), temp)) {
            auto n = std::make_shared<Note>(std::move(temp));
            n->file_path = entry.path();
            n->saved_locally = true;
            n->dirty = false;
            n->inbox = false;
            if (tableName) n->table_name = tableName;
            notesById_[n->uuid] = std::move(n);
        }
    }
}

std::string NotesManager::createNote(std::string title,
                                     std::string author,
                                     std::optional<std::string> tableName) {
    std::lock_guard<std::mutex> lk(mtx_);

    auto n = std::make_shared<Note>();
    n->uuid = generateUUID();
    n->title = std::move(title);
    n->author = std::move(author);
    n->table_name = std::move(tableName);
    n->creation_ts = Clock::now();
    n->last_update_ts = n->creation_ts;
    n->saved_locally = false;
    n->dirty = true;
    n->shared = false;
    n->inbox = false;
    n->open_editor = true;

    auto id = n->uuid;
    notesById_.emplace(id, std::move(n));
    toastGood("Note created: " + id);
    return id;
}

bool NotesManager::saveNote(const std::string& uuid) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = notesById_.find(uuid);
    if (it == notesById_.end()) return false;
    auto& n = *it->second;

    std::error_code ec;
    if (n.file_path.empty()) {
        n.file_path = defaultSavePath(n);
    }
    std::filesystem::create_directories(n.file_path.parent_path(), ec);

    n.last_update_ts = Clock::now();
    if (!writeMarkdownFile(n.file_path, n)) {
        toastError("Failed to save: " + n.title);
        return false;
    }
    n.saved_locally = true;
    n.dirty = false;
    n.inbox = false;
    toastGood("Saved: " + n.title);
    return true;
}

bool NotesManager::saveNoteAs(const std::string& uuid, const std::filesystem::path& absolutePath) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = notesById_.find(uuid);
    if (it == notesById_.end()) return false;
    auto& n = *it->second;

    std::error_code ec;
    std::filesystem::create_directories(absolutePath.parent_path(), ec);

    n.last_update_ts = Clock::now();
    if (!writeMarkdownFile(absolutePath, n)) {
        toastError("Failed to save as: " + absolutePath.string());
        return false;
    }
    n.file_path = absolutePath;
    n.saved_locally = true;
    n.dirty = false;
    n.inbox = false;
    toastGood("Saved as: " + absolutePath.filename().string());
    return true;
}

bool NotesManager::deleteNote(const std::string& uuid, bool deleteFromDisk) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = notesById_.find(uuid);
    if (it == notesById_.end()) return false;

    if (deleteFromDisk && !it->second->file_path.empty()) {
        std::error_code ec;
        std::filesystem::remove(it->second->file_path, ec);
    }
    notesById_.erase(it);
    toastWarn("Note deleted");
    return true;
}

std::shared_ptr<Note> NotesManager::getNote(const std::string& uuid) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = notesById_.find(uuid);
    return (it == notesById_.end() ? nullptr : it->second);
}

std::shared_ptr<const Note> NotesManager::getNote(const std::string& uuid) const {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = notesById_.find(uuid);
    return (it == notesById_.end() ? nullptr : it->second);
}

std::vector<std::shared_ptr<Note>> NotesManager::listAll() {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<std::shared_ptr<Note>> v; v.reserve(notesById_.size());
    for (auto& kv : notesById_) v.push_back(kv.second);
    return v;
}

std::vector<std::shared_ptr<Note>> NotesManager::listMyNotes() {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<std::shared_ptr<Note>> v;
    v.reserve(notesById_.size());
    for (auto& kv : notesById_) {
        if (!kv.second->inbox) v.push_back(kv.second);
    }
    return v;
}

std::vector<std::shared_ptr<Note>> NotesManager::listInbox() {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<std::shared_ptr<Note>> v;
    v.reserve(notesById_.size());
    for (auto& kv : notesById_) {
        if (kv.second->inbox) v.push_back(kv.second);
    }
    return v;
}

void NotesManager::setTitle(const std::string& uuid, std::string title) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = notesById_.find(uuid);
    if (it == notesById_.end()) return;
    it->second->title = std::move(title);
    it->second->dirty = true;
    it->second->last_update_ts = Clock::now();
}

void NotesManager::setContent(const std::string& uuid, std::string md) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = notesById_.find(uuid);
    if (it == notesById_.end()) return;
    it->second->markdown_text = std::move(md);
    it->second->dirty = true;
    it->second->last_update_ts = Clock::now();
}

void NotesManager::setAuthor(const std::string& uuid, std::string author) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = notesById_.find(uuid);
    if (it == notesById_.end()) return;
    it->second->author = std::move(author);
    it->second->dirty = true;
    it->second->last_update_ts = Clock::now();
}

std::string NotesManager::upsertSharedIncoming(std::string uuid,
                                               std::string title,
                                               std::string author,
                                               std::string markdown,
                                               std::optional<std::string> fromPeerName) {
    std::lock_guard<std::mutex> lk(mtx_);
    if (uuid.empty()) uuid = generateUUID();

    auto it = notesById_.find(uuid);
    if (it == notesById_.end()) {
        auto n = std::make_shared<Note>();
        n->uuid = uuid;
        n->title = std::move(title);
        n->author = std::move(author);
        n->markdown_text = std::move(markdown);
        n->creation_ts = Clock::now();
        n->last_update_ts = n->creation_ts;
        n->saved_locally = false;
        n->dirty = false;
        n->shared = true;
        n->inbox = true;
        n->shared_from = std::move(fromPeerName);
        n->open_editor = false;
        notesById_.emplace(uuid, std::move(n));
    } else {
        auto& n = *it->second;
        n.title = std::move(title);
        n.author = std::move(author);
        n.markdown_text = std::move(markdown);
        n.last_update_ts = Clock::now();
        n.shared = true;
        n.inbox = true;
        n.shared_from = std::move(fromPeerName);
        n.dirty = false;
    }
    return uuid;
}

bool NotesManager::saveInboxToLocal(const std::string& uuid,
                                    std::optional<std::string> tableName) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = notesById_.find(uuid);
    if (it == notesById_.end()) return false;
    auto& n = *it->second;

    if (tableName) n.table_name = tableName;

    std::error_code ec;
    if (n.file_path.empty()) {
        n.file_path = defaultSavePath(n);
    }
    std::filesystem::create_directories(n.file_path.parent_path(), ec);

    n.last_update_ts = Clock::now();
    if (!writeMarkdownFile(n.file_path, n)) {
        toastError("Failed to save inbox note.");
        return false;
    }
    n.saved_locally = true;
    n.inbox = false;
    toastGood("Saved to disk: " + n.title);
    return true;
}

std::filesystem::path NotesManager::defaultSavePath(const Note& n) const {
    auto slug = slugify(n.title.empty() ? "note" : n.title);
    auto fname = slug + "__" + n.uuid + ".md";
    if (n.table_name) return cfg_.gameTablesRootDir / *n.table_name / "Notes" / fname;
    return cfg_.globalNotesDir / fname;
}

// ------------- utils (same as before) -------------
std::string NotesManager::generateUUID() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    uint64_t a = gen(), b = gen();
    std::ostringstream oss;
    oss << std::hex << std::nouppercase << std::setfill('0')
        << std::setw(16) << a << std::setw(16) << b;
    return oss.str();
}

std::string NotesManager::slugify(const std::string& s) {
    std::string out; out.reserve(s.size());
    for (unsigned char c : s) {
        if (std::isalnum(c)) out.push_back((char)std::tolower(c));
        else if (c == ' ' || c == '-' || c == '_') out.push_back('-');
    }
    while (!out.empty() && out.back() == '-') out.pop_back();
    if (out.empty()) out = "note";
    return out;
}

int64_t NotesManager::toEpochMillis(std::chrono::system_clock::time_point tp) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}
std::chrono::system_clock::time_point NotesManager::fromEpochMillis(int64_t ms) {
    return std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
}

// front-matter parse/write (same as previous version)
bool NotesManager::parseMarkdownFile(const std::filesystem::path& path, Note& outNote) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    size_t pos = 0;
    auto startsWith = [&](const char* s) { return text.compare(pos, std::strlen(s), s) == 0; };

    std::unordered_map<std::string, std::string> meta;
    std::string body;

    if (startsWith("---")) {
        pos += 3;
        while (pos < text.size() && (text[pos] == '\r' || text[pos] == '\n')) ++pos;
        while (pos < text.size()) {
            if (text.compare(pos, 3, "---") == 0) {
                pos += 3;
                while (pos < text.size() && (text[pos] == '\r' || text[pos] == '\n')) ++pos;
                break;
            }
            size_t lineEnd = text.find_first_of("\r\n", pos);
            std::string line = (lineEnd == std::string::npos) ? text.substr(pos)
                                                              : text.substr(pos, lineEnd - pos);
            if (lineEnd == std::string::npos) pos = text.size();
            else {
                pos = lineEnd;
                while (pos < text.size() && (text[pos] == '\r' || text[pos] == '\n')) ++pos;
            }
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                auto k = line.substr(0, colon);
                auto v = line.substr(colon + 1);
                auto ltrim = [](std::string& s) {
                    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch){return !std::isspace(ch);})); };
                auto rtrim = [](std::string& s) {
                    s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch){return !std::isspace(ch);}).base(), s.end()); };
                auto trim = [&](std::string& s){ ltrim(s); rtrim(s); };
                trim(k); trim(v);
                meta[k] = v;
            }
        }
        body = (pos < text.size()) ? text.substr(pos) : std::string{};
    } else {
        body = text;
    }

    Note n;
    n.uuid = meta.count("uuid") ? meta["uuid"] : generateUUID();
    n.title = meta.count("title") ? meta["title"] : path.stem().string();
    n.author = meta.count("author") ? meta["author"] : "unknown";
    if (meta.count("creation_ts")) {
        try { n.creation_ts = fromEpochMillis(std::stoll(meta["creation_ts"])); }
        catch(...) { n.creation_ts = Clock::now(); }
    } else n.creation_ts = Clock::now();
    if (meta.count("last_update_ts")) {
        try { n.last_update_ts = fromEpochMillis(std::stoll(meta["last_update_ts"])); }
        catch(...) { n.last_update_ts = n.creation_ts; }
    } else n.last_update_ts = n.creation_ts;

    if (meta.count("table") && !meta["table"].empty()) n.table_name = meta["table"];
    if (meta.count("shared")) n.shared = (meta["shared"] == "1");
    if (meta.count("shared_from") && !meta["shared_from"].empty()) n.shared_from = meta["shared_from"];

    n.markdown_text = std::move(body);
    outNote = std::move(n);
    return true;
}

bool NotesManager::writeMarkdownFile(const std::filesystem::path& path, const Note& note) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    auto c_ms = toEpochMillis(note.creation_ts);
    auto u_ms = toEpochMillis(note.last_update_ts);

    out << "---\n";
    out << "uuid: " << note.uuid << "\n";
    out << "title: " << note.title << "\n";
    out << "author: " << note.author << "\n";
    out << "creation_ts: " << c_ms << "\n";
    out << "last_update_ts: " << u_ms << "\n";
    out << "table: " << (note.table_name ? *note.table_name : "") << "\n";
    out << "shared: " << (note.shared ? "1" : "0") << "\n";
    out << "shared_from: " << (note.shared_from ? *note.shared_from : "") << "\n";
    out << "---\n";
    out << note.markdown_text;
    return true;
}

// Toaster helpers
void NotesManager::toastInfo(const std::string& msg) const {
    if (toaster_) toaster_->Push(ImGuiToaster::Level::Info, msg);
}
void NotesManager::toastGood(const std::string& msg) const {
    if (toaster_) toaster_->Push(ImGuiToaster::Level::Good, msg);
}
void NotesManager::toastWarn(const std::string& msg) const {
    if (toaster_) toaster_->Push(ImGuiToaster::Level::Warning, msg);
}
void NotesManager::toastError(const std::string& msg) const {
    if (toaster_) toaster_->Push(ImGuiToaster::Level::Error, msg);
}


std::shared_ptr<Note> NotesManager::getByUuid(const std::string& uuid) const {
    auto it = notesByUuid_.find(uuid);
    return (it != notesByUuid_.end()) ? it->second : nullptr;
}

void NotesManager::indexNote(const std::shared_ptr<Note>& n) {
    if (!n) return;
    notesByUuid_[n->uuid] = n;

    // exact, case-insensitive index for title
    if (!n->title.empty()) {
        titleToUuid_[toLower_(n->title)] = n->uuid;
    }
}

void NotesManager::removeFromIndex(const std::string& uuid) {
    auto it = notesByUuid_.find(uuid);
    if (it == notesByUuid_.end()) return;

    // erase title index if it points to this uuid
    if (!it->second->title.empty()) {
        auto key = toLower_(it->second->title);
        auto jt = titleToUuid_.find(key);
        if (jt != titleToUuid_.end() && jt->second == uuid) {
            titleToUuid_.erase(jt);
        }
    }
    notesByUuid_.erase(it);
}

std::string NotesManager::resolveRef(const std::string& ref) const {
    if (ref.empty()) return {};

    // 1) full UUID? (very loose validation) and it exists
    if (looksLikeUuid_(ref)) {
        if (notesByUuid_.find(ref) != notesByUuid_.end())
            return ref;
        // if it "looks like uuid" but doesn't exist, fall through to title (user may have typed a title with dashes)
    }

    // 2) title (case-insensitive exact match)
    {
        auto it = titleToUuid_.find(toLower_(ref));
        if (it != titleToUuid_.end())
            return it->second;
    }

    // 3) short id (>=8 hex chars, no dashes) — optional bonus
    if (looksLikeShortHex_(ref)) {
        std::string matchUuid;
        bool ambig = false;
        for (const auto& kv : notesByUuid_) {
            const std::string& uuid = kv.first; // canonical 36-char UUID
            // compare only hex chars (ignore dashes) at the front
            std::string compact;
            compact.reserve(32);
            for (char c : uuid) if (c != '-') compact.push_back((char)std::tolower((unsigned char)c));

            std::string needle = toLower_(ref);
            if (compact.rfind(needle, 0) == 0) { // prefix match
                if (matchUuid.empty()) matchUuid = uuid;
                else { ambig = true; break; }
            }
        }
        if (!ambig && !matchUuid.empty())
            return matchUuid;
        // ambiguous or not found → fall through
    }

    return {};
}

// ---------------- helpers ----------------
bool NotesManager::looksLikeUuid_(const std::string& s) {
    // Very loose: 8-4-4-4-12 hex + dashes
    if (s.size() != 36) return false;
    const int dashPos[4] = {8, 13, 18, 23};
    for (int i = 0; i < 36; ++i) {
        if (i == dashPos[0] || i == dashPos[1] || i == dashPos[2] || i == dashPos[3]) {
            if (s[i] != '-') return false;
        } else if (!std::isxdigit((unsigned char)s[i])) {
            return false;
        }
    }
    return true;
}

bool NotesManager::looksLikeShortHex_(const std::string& s) {
    if (s.size() < 8) return false; // minimum 8 for usefulness
    for (char c : s) {
        if (c == '-') return false;
        if (!std::isxdigit((unsigned char)c)) return false;
    }
    return true;
}

std::string NotesManager::toLower_(const std::string& s) {
    std::string out;
    out.resize(s.size());
    std::transform(s.begin(), s.end(), out.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    return out;
}

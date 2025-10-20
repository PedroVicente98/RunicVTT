// IdentityManager.h  (kept as your IdentityManager; only username-per-table duties)

#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

class IdentityManager
{
public:
    // File I/O
    bool loadUsernamesFromFile();     // load (tableId -> username) map
    bool saveUsernamesToFile() const; // optional: if you want explicit save

    // CRUD
    bool addUsername(uint64_t tableId, const std::string& username);    // insert (or overwrite) entry
    bool removeUsername(uint64_t tableId);                              // (unused for now)
    bool setUsername(uint64_t tableId, const std::string& newUsername); // change username for tableId

    // Queries
    std::string findUsername(uint64_t tableId,
                             const std::string& fallback = "Player") const; // returns saved or fallback

    // Access (optional)
    const std::unordered_map<uint64_t, std::string>& all() const
    {
        return byTable_;
    }

private:
    // backing map: tableId -> username
    std::unordered_map<uint64_t, std::string> byTable_;

    // internal helpers (optional; you may keep them private)
    bool writeBinaryFile() const;
    bool readBinaryFile();
};

// IdentityManager.h
#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

struct PeerIdentity
{
    std::string uniqueId;                      // stable, persisted
    std::string username;                      // last known display name
    std::string peerId;                        // last known transport id (ephemeral, can be empty)
    std::vector<std::string> usernamesHistory; // last few names (optional UX)
};

class IdentityManager
{
public:
    // ---- my identity (persisted) ----
    bool loadMyIdentityFromFile();
    bool saveMyIdentityToFile() const;

    // Set or update my identity (call on first run or rename)
    void setMyIdentity(const std::string& uniqueId, const std::string& username);
    const std::string& myUniqueId() const
    {
        return myUniqueId_;
    }
    const std::string& myUsername() const
    {
        return myUsername_;
    }

    // ---- address book (persisted) ----
    bool loadAddressBookFromFile();
    bool saveAddressBookToFile() const;

    // Bind a live peerId to a uniqueId + username (on DC open)
    void bindPeer(const std::string& peerId,
                  const std::string& uniqueId,
                  const std::string& username);

    // Lookups
    std::string usernameForUnique(const std::string& uniqueId) const;
    std::optional<std::string> uniqueForPeer(const std::string& peerId) const;
    std::optional<std::string> peerForUnique(const std::string& uniqueId) const;
    std::optional<std::string> usernameForPeer(const std::string& peerId) const;

    // Update username for a known uniqueId (no uniqueness enforcement)
    void setUsernameForUnique(const std::string& uniqueId, const std::string& username);

    // Optional: expose book
    const std::unordered_map<std::string, PeerIdentity>& all() const
    {
        return byUnique_;
    }

private:
    // persisted “me”
    std::string myUniqueId_;
    std::string myUsername_;

    // address book keyed by uniqueId
    std::unordered_map<std::string, PeerIdentity> byUnique_;

    // session map: peerId -> uniqueId (only for connected peers)
    std::unordered_map<std::string, std::string> peerToUnique_;

    // file helpers
    bool writeMeFile() const;
    bool readMeFile();
    bool writeBookFile() const;
    bool readBookFile();

    // paths
    static const char* kMeFile();
    static const char* kBookFile();
};

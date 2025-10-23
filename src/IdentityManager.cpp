// IdentityManager.cpp
#include "IdentityManager.h"
#include "Serializer.h"
#include <filesystem>
#include <fstream>
#include <algorithm>
#include "PathManager.h"

using std::string;

static constexpr const char* kME_MAGIC = "RUNIC-ME";
static constexpr const int kME_VER = 1;

static constexpr const char* kBOOK_MAGIC = "RUNIC-BOOK";
static constexpr const int kBOOK_VER = 1;

const char* IdentityManager::kMeFile()
{
    static const std::string s_me = (PathManager::getConfigPath() / "identity_me.runic").string();
    return s_me.c_str();
}
const char* IdentityManager::kBookFile()
{
    static const std::string s_book = (PathManager::getConfigPath() / "identity_book.runic").string();
    return s_book.c_str();
}
// ------------------ public: my identity ------------------

bool IdentityManager::loadMyIdentityFromFile()
{
    myUniqueId_.clear();
    myUsername_.clear();
    return readMeFile();
}

bool IdentityManager::saveMyIdentityToFile() const
{
    return writeMeFile();
}

void IdentityManager::setMyIdentity(const std::string& uniqueId, const std::string& username)
{
    myUniqueId_ = uniqueId;
    myUsername_ = username;
    (void)saveMyIdentityToFile();

    // also mirror into address book
    auto& pi = byUnique_[uniqueId];
    pi.uniqueId = uniqueId;
    if (!pi.username.empty() && pi.username != username)
    {
        pi.usernamesHistory.emplace_back(pi.username);
        if (pi.usernamesHistory.size() > 10) // cap
            pi.usernamesHistory.erase(pi.usernamesHistory.begin());
    }
    pi.username = username;
    (void)saveAddressBookToFile();
}

// ------------------ public: address book ------------------

bool IdentityManager::loadAddressBookFromFile()
{
    byUnique_.clear();
    peerToUnique_.clear();
    return readBookFile();
}

bool IdentityManager::saveAddressBookToFile() const
{
    return writeBookFile();
}
//
//void IdentityManager::bindPeer(const std::string& peerId,
//                               const std::string& uniqueId,
//                               const std::string& username)
//{
//    // session binding
//    peerToUnique_[peerId] = uniqueId;
//
//    auto& pi = byUnique_[uniqueId];
//    pi.uniqueId = uniqueId;
//    pi.peerId = peerId;
//
//    if (!pi.username.empty() && pi.username != username)
//    {
//        pi.usernamesHistory.emplace_back(pi.username);
//        if (pi.usernamesHistory.size() > 10)
//            pi.usernamesHistory.erase(pi.usernamesHistory.begin());
//    }
//    pi.username = username;
//
//    (void)saveAddressBookToFile(); // optional: you can defer saving if you prefer
//}

void IdentityManager::bindPeer(const std::string& peerId,
                               const std::string& uniqueId,
                               const std::string& username)
{
    // 1) If this peerId was previously mapped to some unique, clear that first.
    if (auto it = peerToUnique_.find(peerId); it != peerToUnique_.end())
    {
        const std::string& oldUid = it->second;
        if (oldUid != uniqueId)
        {
            // Unlink old mapping (the old unique keeps its record, but without this peerId)
            auto bit = byUnique_.find(oldUid);
            if (bit != byUnique_.end() && bit->second.peerId == peerId)
                bit->second.peerId.clear();
        }
        peerToUnique_.erase(it);
    }

    // 2) If this uniqueId already had a different peerId, remove that stale reverse mapping.
    auto& pi = byUnique_[uniqueId];
    if (!pi.peerId.empty() && pi.peerId != peerId)
    {
        peerToUnique_.erase(pi.peerId); // remove old peerId -> uniqueId
    }

    // 3) Set the fresh links (one-to-one at a time)
    peerToUnique_[peerId] = uniqueId;

    pi.uniqueId = uniqueId;
    pi.peerId = peerId;

    if (!pi.username.empty() && pi.username != username)
    {
        pi.usernamesHistory.emplace_back(pi.username);
        if (pi.usernamesHistory.size() > 10)
            pi.usernamesHistory.erase(pi.usernamesHistory.begin());
    }
    pi.username = username;

    (void)saveAddressBookToFile();
}

void IdentityManager::erasePeer(const std::string& peerId)
{
    if (auto it = peerToUnique_.find(peerId); it != peerToUnique_.end())
    {
        const std::string uid = it->second;
        peerToUnique_.erase(it);
        auto bit = byUnique_.find(uid);
        if (bit != byUnique_.end() && bit->second.peerId == peerId)
            bit->second.peerId.clear();
    }
}

// ------------------ public: lookups ------------------
void IdentityManager::setUsernameForUnique(const std::string& uniqueId, const std::string& username)
{
    if (uniqueId == myUniqueId_)
    {
        if (myUsername_ != username)
        {
            myUsername_ = username;
            (void)saveMyIdentityToFile();
        }
    }

    auto& pi = byUnique_[uniqueId];
    if (!pi.username.empty() && pi.username != username)
    {
        pi.usernamesHistory.emplace_back(pi.username);
        if (pi.usernamesHistory.size() > 10)
            pi.usernamesHistory.erase(pi.usernamesHistory.begin());
    }
    pi.uniqueId = uniqueId;
    pi.username = username;
    (void)saveAddressBookToFile();
}

std::string IdentityManager::usernameForUnique(const std::string& uniqueId) const
{
    auto it = byUnique_.find(uniqueId);
    if (it != byUnique_.end() && !it->second.username.empty())
        return it->second.username;

    if (uniqueId == myUniqueId_ && !myUsername_.empty())
        return myUsername_;

    // fallback: show truncated uniqueId if unknown
    if (uniqueId.size() > 8)
        return uniqueId.substr(0, 8);
    return uniqueId;
}

std::optional<std::string> IdentityManager::uniqueForPeer(const std::string& peerId) const
{
    if (auto it = peerToUnique_.find(peerId); it != peerToUnique_.end())
        return it->second;
    return std::nullopt;
}

std::optional<std::string> IdentityManager::peerForUnique(const std::string& uniqueId) const
{
    if (auto it = byUnique_.find(uniqueId); it != byUnique_.end() && !it->second.peerId.empty())
        return it->second.peerId;
    return std::nullopt;
}

//std::optional<std::string> IdentityManager::uniqueForPeer(const std::string& peerId) const
//{
//    auto it = peerToUnique_.find(peerId);
//    if (it == peerToUnique_.end())
//        return std::nullopt;
//    return it->second;
//}
//
//std::optional<std::string> IdentityManager::peerForUnique(const std::string& uniqueId) const
//{
//    for (const auto& kv : peerToUnique_)
//        if (kv.second == uniqueId)
//            return kv.first;
//    return std::nullopt;
//}

std::optional<std::string> IdentityManager::usernameForPeer(const std::string& peerId) const
{
    for (const auto& kv : peerToUnique_)
        if (kv.first == peerId)
            return usernameForUnique(kv.second);
    return std::nullopt;
}

// ------------------ private: files ------------------

bool IdentityManager::writeMeFile() const
{
    try
    {
        std::vector<uint8_t> buf;
        Serializer::serializeString(buf, kME_MAGIC);
        Serializer::serializeInt(buf, kME_VER);
        Serializer::serializeString(buf, myUniqueId_);
        Serializer::serializeString(buf, myUsername_);

        std::ofstream os(kMeFile(), std::ios::binary | std::ios::trunc);
        os.write(reinterpret_cast<const char*>(buf.data()),
                 static_cast<std::streamsize>(buf.size()));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool IdentityManager::readMeFile()
{
    namespace fs = std::filesystem;
    if (!fs::exists(kMeFile()))
        return true; // first run is fine

    try
    {
        std::ifstream is(kMeFile(), std::ios::binary);
        is.seekg(0, std::ios::end);
        auto sz = is.tellg();
        is.seekg(0, std::ios::beg);
        std::vector<uint8_t> buf(static_cast<size_t>(sz));
        if (sz > 0)
            is.read(reinterpret_cast<char*>(buf.data()), sz);

        size_t off = 0;
        if (Serializer::deserializeString(buf, off) != kME_MAGIC)
            return false;
        (void)Serializer::deserializeInt(buf, off); // ver
        myUniqueId_ = Serializer::deserializeString(buf, off);
        myUsername_ = Serializer::deserializeString(buf, off);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool IdentityManager::writeBookFile() const
{
    try
    {
        std::vector<uint8_t> buf;
        Serializer::serializeString(buf, kBOOK_MAGIC);
        Serializer::serializeInt(buf, kBOOK_VER);

        // count
        Serializer::serializeInt(buf, static_cast<int>(byUnique_.size()));
        for (const auto& [uid, pi] : byUnique_)
        {
            Serializer::serializeString(buf, pi.uniqueId);
            Serializer::serializeString(buf, pi.username);
            Serializer::serializeString(buf, pi.peerId);

            // history
            Serializer::serializeInt(buf, static_cast<int>(pi.usernamesHistory.size()));
            for (const auto& old : pi.usernamesHistory)
                Serializer::serializeString(buf, old);
        }

        std::ofstream os(kBookFile(), std::ios::binary | std::ios::trunc);
        os.write(reinterpret_cast<const char*>(buf.data()),
                 static_cast<std::streamsize>(buf.size()));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool IdentityManager::readBookFile()
{
    namespace fs = std::filesystem;
    if (!fs::exists(kBookFile()))
        return true;

    try
    {
        std::ifstream is(kBookFile(), std::ios::binary);
        is.seekg(0, std::ios::end);
        auto sz = is.tellg();
        is.seekg(0, std::ios::beg);
        std::vector<uint8_t> buf(static_cast<size_t>(sz));
        if (sz > 0)
            is.read(reinterpret_cast<char*>(buf.data()), sz);

        size_t off = 0;
        if (Serializer::deserializeString(buf, off) != kBOOK_MAGIC)
            return false;
        (void)Serializer::deserializeInt(buf, off); // ver

        int n = Serializer::deserializeInt(buf, off);
        // NOTE: const method; we need to cast away const to fill the map or
        // make this non-const. Easiest: make readBookFile() non-const (done above).
        // So keep this implementation but ensure header declares it non-const.
        return false; // (safety: this should never compile with const)
    }
    catch (...)
    {
        return false;
    }
}

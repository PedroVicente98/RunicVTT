// IdentityManager.cpp
#include "IdentityManager.h"
#include "Serializer.h"
#include <filesystem>
#include <fstream>
#include <vector>

static constexpr const char* kMagic = "RUNIC-IDENT";
static constexpr int kVersion = 1;

static std::filesystem::path identityFilePath()
{
    // Put the file next to your other data, or adjust as needed:
    return std::filesystem::path("identity.runic");
}

bool IdentityManager::loadUsernamesFromFile()
{
    byTable_.clear();

    auto p = identityFilePath();
    if (!std::filesystem::exists(p))
        return true; // no file yet is fine

    try
    {
        std::ifstream is(p, std::ios::binary);
        is.seekg(0, std::ios::end);
        auto sz = is.tellg();
        is.seekg(0, std::ios::beg);
        std::vector<uint8_t> buf((size_t)sz);
        if (sz > 0)
            is.read((char*)buf.data(), sz);

        size_t off = 0;
        auto magic = Serializer::deserializeString(buf, off);
        if (magic != kMagic)
            return false;
        (void)Serializer::deserializeInt(buf, off); // version

        const int n = Serializer::deserializeInt(buf, off);
        for (int i = 0; i < n; ++i)
        {
            uint64_t tid = Serializer::deserializeUInt64(buf, off);
            std::string uname = Serializer::deserializeString(buf, off);
            byTable_[tid] = std::move(uname);
        }
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool IdentityManager::saveUsernamesToFile() const
{
    try
    {
        std::vector<uint8_t> buf;
        Serializer::serializeString(buf, kMagic);
        Serializer::serializeInt(buf, kVersion);
        Serializer::serializeInt(buf, (int)byTable_.size());
        for (auto& [tid, uname] : byTable_)
        {
            Serializer::serializeUInt64(buf, tid);
            Serializer::serializeString(buf, uname);
        }

        auto p = identityFilePath();
        std::ofstream os(p, std::ios::binary | std::ios::trunc);
        os.write((const char*)buf.data(), (std::streamsize)buf.size());
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool IdentityManager::addUsername(uint64_t tableId, const std::string& username)
{
    byTable_[tableId] = username;
    return saveUsernamesToFile();
}

bool IdentityManager::removeUsername(uint64_t tableId)
{
    auto it = byTable_.find(tableId);
    if (it == byTable_.end())
        return false;
    byTable_.erase(it);
    return saveUsernamesToFile();
}

std::string IdentityManager::findUsername(uint64_t tableId, const std::string& fallback) const
{
    if (auto it = byTable_.find(tableId); it != byTable_.end() && !it->second.empty())
        return it->second;
    return fallback;
}

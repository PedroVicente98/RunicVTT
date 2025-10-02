#include "ChatManager.h"
#include "NetworkManager.h"
#include "PeerLink.h"
#include "Serializer.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <optional>

// --------- small local helpers ---------
static double nowSec() { return (double)std::time(nullptr); }

static bool ieq(const std::string &a, const char *b) {
    return std::equal(a.begin(), a.end(), b, b + std::strlen(b),
                      [](char x, char y) { return (char)std::tolower((unsigned char)x) == (char)std::tolower((unsigned char)y); });
}

static ChatMessageModel::Kind classifyMessage(const std::string &s) {
    auto ends_with = [](const std::string &a, const char *suf) {
        const size_t n = std::strlen(suf);
        if (a.size() < n) return false;
        return std::equal(a.end() - n, a.end(), suf, suf + n,
                          [](char c1, char c2) {
                              return (char)std::tolower((unsigned char)c1) == (char)std::tolower((unsigned char)c2);
                          });
    };
    if (ends_with(s, ".png") || ends_with(s, ".jpg") || ends_with(s, ".jpeg")) return ChatMessageModel::Kind::IMAGE;
    if (s.rfind("http://", 0) == 0 || s.rfind("https://", 0) == 0) return ChatMessageModel::Kind::LINK;
    return ChatMessageModel::Kind::TEXT;
}

// --------- ctor / state ---------
ChatManager::ChatManager(std::weak_ptr<NetworkManager> nm)
    : network_(std::move(nm)) {
}

void ChatManager::setNetwork(std::weak_ptr<NetworkManager> nm) {
    network_ = std::move(nm);
}

void ChatManager::setActiveGameTable(uint64_t tableId, const std::string &gameTableName) {
    // persist old
    if (currentTableId_ != 0) saveCurrent();

    currentTableId_ = tableId;
    currentTableName_ = gameTableName;
    // load or init
    loadCurrent();
    ensureGeneral();
}

bool ChatManager::hasCurrent() const {
    return currentTableId_ != 0;
}

// --------- persistence ---------
std::filesystem::path ChatManager::chatFilePathFor(uint64_t tableId, const std::string &name) const {
    // Keep it simple: <name>_<id>_chat.runic
    // If you prefer only name, replace accordingly (beware collisions).
    return std::filesystem::path(name + "_" + std::to_string(tableId) + "_chat.runic");
}

bool ChatManager::saveCurrent() {
    if (!hasCurrent()) return false;

    std::vector<unsigned char> buf;
    Serializer::serializeString(buf, "RUNIC-CHAT");
    Serializer::serializeInt(buf, 3); // version 3: threads keyed by u64 id

    // write threads count
    Serializer::serializeInt(buf, (int)threads_.size());
    for (auto &kv : threads_) {
        const auto &th = kv.second;
        Serializer::serializeUInt64(buf, th.id);
        Serializer::serializeString(buf, th.displayName);

        // participants
        Serializer::serializeInt(buf, (int)th.participants.size());
        for (auto &pid : th.participants) Serializer::serializeString(buf, pid);

        // messages
        Serializer::serializeInt(buf, (int)th.messages.size());
        for (auto &m : th.messages) {
            Serializer::serializeInt(buf, (int)m.kind);
            Serializer::serializeString(buf, m.senderId);
            Serializer::serializeString(buf, m.username);
            Serializer::serializeString(buf, m.content);
            Serializer::serializeUInt64(buf, (uint64_t)m.ts);
        }
    }

    try {
        auto p = chatFilePathFor(currentTableId_, currentTableName_);
        std::ofstream os(p, std::ios::binary);
        os.write((const char *)buf.data(), (std::streamsize)buf.size());
        return true;
    } catch (...) {
        return false;
    }
}

bool ChatManager::loadCurrent() {
    threads_.clear();
    if (!hasCurrent()) {
        ensureGeneral();
        return false;
    }

    auto p = chatFilePathFor(currentTableId_, currentTableName_);
    if (!std::filesystem::exists(p)) {
        ensureGeneral();
        return false;
    }

    std::vector<unsigned char> buf;
    try {
        std::ifstream is(p, std::ios::binary);
        is.seekg(0, std::ios::end);
        auto sz = is.tellg();
        is.seekg(0, std::ios::beg);
        buf.resize((size_t)sz);
        is.read((char *)buf.data(), sz);
    } catch (...) {
        ensureGeneral();
        return false;
    }

    size_t off = 0;
    auto magic = Serializer::deserializeString(buf, off);
    if (magic != "RUNIC-CHAT") { ensureGeneral(); return false; }
    const int version = Serializer::deserializeInt(buf, off);
    (void)version;

    const int tcount = Serializer::deserializeInt(buf, off);
    for (int i = 0; i < tcount; ++i) {
        ChatThreadModel th;
        th.id = Serializer::deserializeUInt64(buf, off);
        th.displayName = Serializer::deserializeString(buf, off);

        const int pcount = Serializer::deserializeInt(buf, off);
        for (int k = 0; k < pcount; ++k) {
            th.participants.insert(Serializer::deserializeString(buf, off));
        }

        const int mcount = Serializer::deserializeInt(buf, off);
        for (int k = 0; k < mcount; ++k) {
            ChatMessageModel m;
            m.kind     = (ChatMessageModel::Kind)Serializer::deserializeInt(buf, off);
            m.senderId = Serializer::deserializeString(buf, off);
            m.username = Serializer::deserializeString(buf, off);
            m.content  = Serializer::deserializeString(buf, off);
            m.ts       = (double)Serializer::deserializeUInt64(buf, off);
            th.messages.push_back(std::move(m));
        }
        threads_.emplace(th.id, std::move(th));
    }

    ensureGeneral();
    if (threads_.find(activeThreadId_) == threads_.end()) activeThreadId_ = generalThreadId_;
    return true;
}

// --------- general thread helpers ---------
void ChatManager::ensureGeneral() {
    // general thread id is a fixed synthetic value (e.g., 1)
    if (threads_.find(generalThreadId_) == threads_.end()) {
        ChatThreadModel th;
        th.id = generalThreadId_;
        th.displayName = "General";
        threads_.emplace(th.id, std::move(th));
    }
    activeThreadId_ = generalThreadId_;
}

void ChatManager::refreshGeneralParticipants(const std::unordered_map<std::string,std::shared_ptr<PeerLink>> &peers) {
    auto it = threads_.find(generalThreadId_);
    if (it == threads_.end()) return;
    auto &th = it->second;

    th.participants.clear();
    for (auto &kv : peers) {
        th.participants.insert(kv.first);
    }
    // displayName as "General (N)"
    th.displayName = "General (" + std::to_string(th.participants.size()) + ")";
}

ChatThreadModel *ChatManager::getThread(uint64_t id) {
    auto it = threads_.find(id);
    if (it == threads_.end()) return nullptr;
    return &it->second;
}

uint64_t ChatManager::findThreadByParticipants(const std::set<std::string> &parts) const {
    // never match "General" here (it has empty participants)
    for (auto &kv : threads_) {
        const auto &th = kv.second;
        if (th.id == generalThreadId_) continue;
        if (th.participants == parts) return th.id;
    }
    return 0;
}

uint64_t ChatManager::makeDeterministicThreadId(const std::set<std::string> &parts) {
    // simple hash on a sorted joined string "a|b|c"
    std::string joined;
    bool first = true;
    for (auto &p : parts) { if (!first) joined.push_back('|'); joined += p; first = false; }
    std::hash<std::string> H;
    uint64_t id = H(joined);
    if (id == 0 || id == generalThreadId_) id ^= 0x9E3779B97F4A7C15ull; // avoid special collisions
    return id;
}

// --------- snapshot (GameTable) meta only ---------
void ChatManager::writeThreadsToSnapshotGT(std::vector<unsigned char> &buf) const {
    // write only metadata (no messages): [u32 count] then for each:
    // [u64 threadId][string displayName][u32 partCount]{string peerId...}
    Serializer::serializeInt(buf, (int)threads_.size());
    for (auto &kv : threads_) {
        const auto &th = kv.second;
        Serializer::serializeUInt64(buf, th.id);
        Serializer::serializeString(buf, th.displayName);
        Serializer::serializeInt(buf, (int)th.participants.size());
        for (auto &p : th.participants) Serializer::serializeString(buf, p);
    }
}

void ChatManager::readThreadsFromSnapshotGT(const std::vector<unsigned char> &buf, size_t &off) {
    if (!hasCurrent()) return;
    const int count = Serializer::deserializeInt(buf, off);
    for (int i = 0; i < count; ++i) {
        ChatThreadModel th;
        th.id = Serializer::deserializeUInt64(buf, off);
        th.displayName = Serializer::deserializeString(buf, off);
        const int pc = Serializer::deserializeInt(buf, off);
        for (int k = 0; k < pc; ++k) th.participants.insert(Serializer::deserializeString(buf, off));

        auto it = threads_.find(th.id);
        if (it == threads_.end()) {
            threads_.emplace(th.id, std::move(th));
        } else {
            it->second.displayName  = th.displayName;
            it->second.participants = std::move(th.participants);
        }
    }
    ensureGeneral();
}

// --------- emits (binary frames) ---------
void ChatManager::emitThreadCreate(const ChatThreadModel &th) {
    auto nm = network_.lock(); if (!nm) return;
    std::vector<uint8_t> buf;
    Serializer::serializeUInt64(buf, currentTableId_);
    Serializer::serializeUInt64(buf, th.id);
    Serializer::serializeString(buf, th.displayName);
    Serializer::serializeInt(buf, (int)th.participants.size());
    for (auto &p : th.participants) Serializer::serializeString(buf, p);
    nm->broadcastChatThreadFrame(msg::DCType::ChatThreadCreate, buf);
}

void ChatManager::emitThreadUpdate(const ChatThreadModel &th) {
    auto nm = network_.lock(); if (!nm) return;
    std::vector<uint8_t> buf;
    Serializer::serializeUInt64(buf, currentTableId_);
    Serializer::serializeUInt64(buf, th.id);
    Serializer::serializeString(buf, th.displayName);
    Serializer::serializeInt(buf, (int)th.participants.size());
    for (auto &p : th.participants) Serializer::serializeString(buf, p);
    nm->broadcastChatThreadFrame(msg::DCType::ChatThreadUpdate, buf);
}

void ChatManager::emitThreadDelete(uint64_t threadId) {
    auto nm = network_.lock(); if (!nm) return;
    std::vector<uint8_t> buf;
    Serializer::serializeUInt64(buf, currentTableId_);
    Serializer::serializeUInt64(buf, threadId);
    nm->broadcastChatThreadFrame(msg::DCType::ChatThreadDelete, buf);
}

void ChatManager::emitChatMessageFrame(uint64_t threadId, const std::string &username,
                                       const std::string &text, uint64_t ts) {
    auto nm = network_.lock(); if (!nm) return;

    std::vector<uint8_t> buf;
    Serializer::serializeUInt64(buf, currentTableId_);
    Serializer::serializeUInt64(buf, threadId);
    Serializer::serializeUInt64(buf, ts);
    Serializer::serializeString(buf, username);
    Serializer::serializeString(buf, text);

    auto *th = getThread(threadId);
    if (!th) return;

    // general -> broadcast; group -> send to participants
    if (th->id == generalThreadId_ || th->participants.empty()) {
        nm->broadcastChatThreadFrame(msg::DCType::ChatMessage, buf);
    } else {
        nm->sendChatThreadFrameTo(th->participants, msg::DCType::ChatMessage, buf);
    }
}

// --------- inbound frames (binary) ---------
void ChatManager::onChatThreadCreateFrame(const std::vector<uint8_t> &b, size_t &off) {
    if (!hasCurrent()) return;
    const uint64_t tableId = Serializer::deserializeUInt64(b, off);
    if (tableId != currentTableId_) return;

    ChatThreadModel th;
    th.id = Serializer::deserializeUInt64(b, off);
    th.displayName = Serializer::deserializeString(b, off);
    const int pc = Serializer::deserializeInt(b, off);
    for (int i = 0; i < pc; ++i) th.participants.insert(Serializer::deserializeString(b, off));

    // dedupe by participants (unless it's General)
    if (!th.participants.empty()) {
        if (uint64_t dup = findThreadByParticipants(th.participants)) {
            auto *cur = getThread(dup);
            if (cur) cur->displayName = th.displayName;
            return;
        }
    }
    threads_.emplace(th.id, std::move(th));
}

void ChatManager::onChatThreadUpdateFrame(const std::vector<uint8_t> &b, size_t &off) {
    if (!hasCurrent()) return;
    const uint64_t tableId = Serializer::deserializeUInt64(b, off);
    if (tableId != currentTableId_) return;

    const uint64_t threadId = Serializer::deserializeUInt64(b, off);
    std::string displayName = Serializer::deserializeString(b, off);
    const int pc = Serializer::deserializeInt(b, off);
    std::set<std::string> parts;
    for (int i = 0; i < pc; ++i) parts.insert(Serializer::deserializeString(b, off));

    auto *th = getThread(threadId);
    if (!th) {
        ChatThreadModel tmp;
        tmp.id = threadId;
        tmp.displayName = std::move(displayName);
        tmp.participants = std::move(parts);
        threads_.emplace(threadId, std::move(tmp));
    } else {
        th->displayName = std::move(displayName);
        th->participants = std::move(parts);
    }
}

void ChatManager::onChatThreadDeleteFrame(const std::vector<uint8_t> &b, size_t &off) {
    if (!hasCurrent()) return;
    const uint64_t tableId  = Serializer::deserializeUInt64(b, off);
    if (tableId != currentTableId_) return;
    const uint64_t threadId = Serializer::deserializeUInt64(b, off);

    if (threadId == generalThreadId_) return; // never delete General
    threads_.erase(threadId);
    if (activeThreadId_ == threadId) activeThreadId_ = generalThreadId_;
}

void ChatManager::onIncomingChatFrame(const std::vector<uint8_t> &b, size_t &off) {
    if (!hasCurrent()) return;

    const uint64_t tableId  = Serializer::deserializeUInt64(b, off);
    const uint64_t threadId = Serializer::deserializeUInt64(b, off);
    const uint64_t ts       = Serializer::deserializeUInt64(b, off);
    const std::string username = Serializer::deserializeString(b, off);
    const std::string text     = Serializer::deserializeString(b, off);

    if (tableId != currentTableId_) return;

    auto *th = getThread(threadId);
    if (!th) {
        ChatThreadModel tmp;
        tmp.id = threadId;
        tmp.displayName = "Thread " + std::to_string(threadId);
        threads_.emplace(threadId, std::move(tmp));
        th = getThread(threadId);
    }

    ChatMessageModel m;
    m.kind     = classifyMessage(text);
    m.senderId = ""; // optional: fill if you carry peerId out of band
    m.username = username.empty() ? "guest" : username;
    m.content  = text;
    m.ts       = (double)ts;
    th->messages.push_back(std::move(m));
}

// --------- thread creation API (local UI calls) ---------
uint64_t ChatManager::ensureThreadByParticipants(const std::set<std::string> &participants,
                                                 const std::string &displayName) {
    if (!hasCurrent()) return 0;

    // General: empty participants maps to general
    if (participants.empty()) {
        ensureGeneral();
        return generalThreadId_;
    }

    // dedupe
    if (uint64_t dup = findThreadByParticipants(participants)) {
        auto *th = getThread(dup);
        if (th && !displayName.empty()) {
            th->displayName = displayName;
            emitThreadUpdate(*th);
        }
        return dup;
    }

    // create new
    ChatThreadModel th;
    th.id = makeDeterministicThreadId(participants);
    th.displayName = displayName.empty() ? "Group" : displayName;
    th.participants = participants;
    threads_.emplace(th.id, th);
    emitThreadCreate(th);
    return th.id;
}

void ChatManager::sendTextToThread(uint64_t threadId, const std::string &text) {
    if (!hasCurrent()) return;

    auto nm = network_.lock(); if (!nm) return;
    const std::string uname = nm->getMyUsername().empty() ? "me" : nm->getMyUsername();
    const uint64_t ts = (uint64_t)nowSec();

    emitChatMessageFrame(threadId, uname, text, ts);

    // local append
    auto *th = getThread(threadId);
    if (!th) return;
    ChatMessageModel m;
    m.kind     = classifyMessage(text);
    m.senderId = "me";
    m.username = uname;
    m.content  = text;
    m.ts       = (double)ts;
    th->messages.push_back(std::move(m));
}

#include "ChatManager.h"
#include "NetworkManager.h"   // must provide: getPeers(), displayNameFor(), getMyUsername(), broadcastChatThreadFrame(), sendChatThreadFrameTo()
#include "PeerLink.h"
#include "Serializer.h"       // serialize/deserialize binary helpers
#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <random>

// ---- local helpers ----
double ChatManager::nowSec() {
    return (double)std::time(nullptr);
}

static bool ends_with_icase(const std::string& a, const char* suf) {
    const size_t n = std::strlen(suf);
    if (a.size() < n) return false;
    return std::equal(a.end()-n, a.end(), suf, suf+n, [](char c1, char c2){
        return (char)std::tolower((unsigned char)c1) == (char)std::tolower((unsigned char)c2);
    });
}

ChatMessageModel::Kind ChatManager::classifyMessage(const std::string& s) {
    if (ends_with_icase(s, ".png") || ends_with_icase(s, ".jpg") || ends_with_icase(s, ".jpeg")) return ChatMessageModel::Kind::IMAGE;
    if (s.rfind("http://",0)==0 || s.rfind("https://",0)==0) return ChatMessageModel::Kind::LINK;
    return ChatMessageModel::Kind::TEXT;
}

// ---- ctor / binding ----
ChatManager::ChatManager(std::weak_ptr<NetworkManager> nm)
    : network_(std::move(nm)) {}

void ChatManager::setNetwork(std::weak_ptr<NetworkManager> nm) {
    network_ = std::move(nm);
}

void ChatManager::setActiveGameTable(uint64_t tableId, const std::string& gameTableName) {
    if (currentTableId_ != 0) saveCurrent();
    currentTableId_ = tableId;
    currentTableName_ = gameTableName;
    loadCurrent();
    ensureGeneral();
}

bool ChatManager::hasCurrent() const { return currentTableId_ != 0; }

// ---- persistence ----
std::filesystem::path ChatManager::chatFilePathFor(uint64_t tableId, const std::string& name) const {
    return std::filesystem::path(name + "_" + std::to_string(tableId) + "_chat.runic");
}

bool ChatManager::saveCurrent() {
    if (!hasCurrent()) return false;

    std::vector<uint8_t> buf;
    Serializer::serializeString(buf, "RUNIC-CHAT");
    Serializer::serializeInt(buf, 3); // version

    Serializer::serializeInt(buf, (int)threads_.size());
    for (auto& [id, th] : threads_) {
        Serializer::serializeUInt64(buf, th.id);
        Serializer::serializeString(buf, th.displayName);
        Serializer::serializeInt(buf, (int)th.participants.size());
        for (auto& p : th.participants) Serializer::serializeString(buf, p);

        Serializer::serializeInt(buf, (int)th.messages.size());
        for (auto& m : th.messages) {
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
        os.write((const char*)buf.data(), (std::streamsize)buf.size());
        return true;
    } catch (...) { return false; }
}

bool ChatManager::loadCurrent() {
    threads_.clear();
    if (!hasCurrent()) { ensureGeneral(); return false; }

    auto p = chatFilePathFor(currentTableId_, currentTableName_);
    if (!std::filesystem::exists(p)) { ensureGeneral(); return false; }

    std::vector<uint8_t> buf;
    try {
        std::ifstream is(p, std::ios::binary);
        is.seekg(0, std::ios::end);
        auto sz = is.tellg();
        is.seekg(0, std::ios::beg);
        buf.resize((size_t)sz);
        is.read((char*)buf.data(), sz);
    } catch (...) { ensureGeneral(); return false; }

    size_t off = 0;
    auto magic = Serializer::deserializeString(buf, off);
    if (magic != "RUNIC-CHAT") { ensureGeneral(); return false; }
    (void)Serializer::deserializeInt(buf, off); // version

    const int tcount = Serializer::deserializeInt(buf, off);
    for (int i=0;i<tcount;++i) {
        ChatThreadModel th;
        th.id          = Serializer::deserializeUInt64(buf, off);
        th.displayName = Serializer::deserializeString(buf, off);
        const int pc   = Serializer::deserializeInt(buf, off);
        for (int k=0;k<pc;++k) th.participants.insert(Serializer::deserializeString(buf, off));

        const int mc = Serializer::deserializeInt(buf, off);
        for (int m=0;m<mc;++m) {
            ChatMessageModel msg;
            msg.kind     = (ChatMessageModel::Kind)Serializer::deserializeInt(buf, off);
            msg.senderId = Serializer::deserializeString(buf, off);
            msg.username = Serializer::deserializeString(buf, off);
            msg.content  = Serializer::deserializeString(buf, off);
            msg.ts       = (double)Serializer::deserializeUInt64(buf, off);
            th.messages.push_back(std::move(msg));
        }
        th.unread = 0; // UX-only
        threads_.emplace(th.id, std::move(th));
    }

    ensureGeneral();
    if (threads_.find(activeThreadId_) == threads_.end()) activeThreadId_ = generalThreadId_;
    return true;
}

// ---- general thread ----
void ChatManager::ensureGeneral() {
    if (threads_.find(generalThreadId_) == threads_.end()) {
        ChatThreadModel th;
        th.id = generalThreadId_;
        th.displayName = "General";
        threads_.emplace(th.id, std::move(th));
    }
    if (activeThreadId_ == 0) activeThreadId_ = generalThreadId_;
}

void ChatManager::refreshGeneralParticipants(const std::unordered_map<std::string,std::shared_ptr<PeerLink>>& peers) {
    auto it = threads_.find(generalThreadId_);
    if (it == threads_.end()) return;
    auto& th = it->second;
    th.participants.clear();
    for (auto& kv : peers) th.participants.insert(kv.first);
    th.displayName = "General (" + std::to_string(th.participants.size()) + ")";
}

ChatThreadModel* ChatManager::getThread(uint64_t id) {
    auto it = threads_.find(id);
    return it == threads_.end() ? nullptr : &it->second;
}

uint64_t ChatManager::findThreadByParticipants(const std::set<std::string>& parts) const {
    for (auto& kv : threads_) {
        const auto& th = kv.second;
        if (th.id == generalThreadId_) continue;
        if (th.participants == parts) return th.id;
    }
    return 0;
}

uint64_t ChatManager::makeDeterministicThreadId(const std::set<std::string>& parts) {
    std::string joined;
    bool first = true;
    for (auto& p : parts) { if (!first) joined.push_back('|'); joined += p; first = false; }
    std::hash<std::string> H;
    uint64_t id = H(joined);
    if (id == 0 || id == generalThreadId_) id ^= 0x9E3779B97F4A7C15ull;
    return id;
}

// ---- snapshot (meta only) ----
void ChatManager::writeThreadsToSnapshotGT(std::vector<unsigned char>& buf) const {
    Serializer::serializeInt(buf, (int)threads_.size());
    for (auto& [id, th] : threads_) {
        Serializer::serializeUInt64(buf, th.id);
        Serializer::serializeString(buf, th.displayName);
        Serializer::serializeInt(buf, (int)th.participants.size());
        for (auto& p : th.participants) Serializer::serializeString(buf, p);
    }
}

void ChatManager::readThreadsFromSnapshotGT(const std::vector<unsigned char>& buf, size_t& off) {
    if (!hasCurrent()) return;
    const int count = Serializer::deserializeInt(buf, off);
    for (int i=0;i<count;++i) {
        ChatThreadModel th;
        th.id = Serializer::deserializeUInt64(buf, off);
        th.displayName = Serializer::deserializeString(buf, off);
        const int pc = Serializer::deserializeInt(buf, off);
        for (int k=0;k<pc;++k) th.participants.insert(Serializer::deserializeString(buf, off));

        auto it = threads_.find(th.id);
        if (it == threads_.end()) threads_.emplace(th.id, std::move(th));
        else {
            it->second.displayName  = th.displayName;
            it->second.participants = std::move(th.participants);
        }
    }
    ensureGeneral();
}

// ---- emits ----
void ChatManager::emitThreadCreate(const ChatThreadModel& th) {
    auto nm = network_.lock(); if (!nm) return;
    std::vector<uint8_t> buf;
    Serializer::serializeUInt64(buf, currentTableId_);
    Serializer::serializeUInt64(buf, th.id);
    Serializer::serializeString(buf, th.displayName);
    Serializer::serializeInt(buf, (int)th.participants.size());
    for (auto& p : th.participants) Serializer::serializeString(buf, p);
    nm->broadcastChatThreadFrame(msg::DCType::ChatThreadCreate, buf);
}

void ChatManager::emitThreadUpdate(const ChatThreadModel& th) {
    auto nm = network_.lock(); if (!nm) return;
    std::vector<uint8_t> buf;
    Serializer::serializeUInt64(buf, currentTableId_);
    Serializer::serializeUInt64(buf, th.id);
    Serializer::serializeString(buf, th.displayName);
    Serializer::serializeInt(buf, (int)th.participants.size());
    for (auto& p : th.participants) Serializer::serializeString(buf, p);
    nm->broadcastChatThreadFrame(msg::DCType::ChatThreadUpdate, buf);
}

void ChatManager::emitThreadDelete(uint64_t threadId) {
    auto nm = network_.lock(); if (!nm) return;
    std::vector<uint8_t> buf;
    Serializer::serializeUInt64(buf, currentTableId_);
    Serializer::serializeUInt64(buf, threadId);
    nm->broadcastChatThreadFrame(msg::DCType::ChatThreadDelete, buf);
}

void ChatManager::emitChatMessageFrame(uint64_t threadId, const std::string& username,
                                       const std::string& text, uint64_t ts) {
    auto nm = network_.lock(); if (!nm) return;
    std::vector<uint8_t> buf;
    Serializer::serializeUInt64(buf, currentTableId_);
    Serializer::serializeUInt64(buf, threadId);
    Serializer::serializeUInt64(buf, ts);
    Serializer::serializeString(buf, username);
    Serializer::serializeString(buf, text);

    const ChatThreadModel* th = nullptr;
    if (auto it = threads_.find(threadId); it != threads_.end()) th = &it->second;
    if (!th) return;

    if (th->id == generalThreadId_ || th->participants.empty()) {
        nm->broadcastChatThreadFrame(msg::DCType::ChatMessage, buf);
    } else {
        nm->sendChatThreadFrameTo(th->participants, msg::DCType::ChatMessage, buf);
    }
}

// ---- inbound ----
void ChatManager::onChatThreadCreateFrame(const std::vector<uint8_t>& b, size_t& off) {
    if (!hasCurrent()) return;
    const uint64_t tableId = Serializer::deserializeUInt64(b, off);
    if (tableId != currentTableId_) return;

    ChatThreadModel th;
    th.id          = Serializer::deserializeUInt64(b, off);
    th.displayName = Serializer::deserializeString(b, off);
    const int pc   = Serializer::deserializeInt(b, off);
    for (int i=0;i<pc;++i) th.participants.insert(Serializer::deserializeString(b, off));

    if (!th.participants.empty()) {
        if (uint64_t dup = findThreadByParticipants(th.participants)) {
            if (auto* cur = getThread(dup)) cur->displayName = th.displayName;
            return;
        }
    }
    th.unread = 0;
    threads_.emplace(th.id, std::move(th));
}

void ChatManager::onChatThreadUpdateFrame(const std::vector<uint8_t>& b, size_t& off) {
    if (!hasCurrent()) return;
    const uint64_t tableId = Serializer::deserializeUInt64(b, off);
    if (tableId != currentTableId_) return;

    const uint64_t threadId = Serializer::deserializeUInt64(b, off);
    std::string displayName = Serializer::deserializeString(b, off);
    const int pc = Serializer::deserializeInt(b, off);
    std::set<std::string> parts;
    for (int i=0;i<pc;++i) parts.insert(Serializer::deserializeString(b, off));

    auto* th = getThread(threadId);
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

void ChatManager::onChatThreadDeleteFrame(const std::vector<uint8_t>& b, size_t& off) {
    if (!hasCurrent()) return;
    const uint64_t tableId  = Serializer::deserializeUInt64(b, off);
    if (tableId != currentTableId_) return;
    const uint64_t threadId = Serializer::deserializeUInt64(b, off);
    if (threadId == generalThreadId_) return;
    threads_.erase(threadId);
    if (activeThreadId_ == threadId) activeThreadId_ = generalThreadId_;
}

void ChatManager::onIncomingChatFrame(const std::vector<uint8_t>& b, size_t& off) {
    if (!hasCurrent()) return;

    const uint64_t tableId  = Serializer::deserializeUInt64(b, off);
    const uint64_t threadId = Serializer::deserializeUInt64(b, off);
    const uint64_t ts       = Serializer::deserializeUInt64(b, off);
    const std::string username = Serializer::deserializeString(b, off);
    const std::string text     = Serializer::deserializeString(b, off);
    if (tableId != currentTableId_) return;

    // Append & unread
    pushMessageLocal(threadId, /*fromId*/"", username, text, (double)ts, /*incoming*/true);
}

// ---- UI helpers ----
void ChatManager::markThreadRead(uint64_t threadId) {
    if (auto* th = getThread(threadId)) th->unread = 0;
}

void ChatManager::pushMessageLocal(uint64_t threadId, const std::string& fromId,
                                   const std::string& username, const std::string& text,
                                   double ts, bool incoming) {
    auto* th = getThread(threadId);
    if (!th) {
        ChatThreadModel tmp; tmp.id = threadId; tmp.displayName = "Thread " + std::to_string(threadId);
        threads_.emplace(threadId, std::move(tmp));
        th = getThread(threadId);
    }
    ChatMessageModel m;
    m.kind     = classifyMessage(text);
    m.senderId = fromId;
    m.username = username;
    m.content  = text;
    m.ts       = ts;
    th->messages.push_back(std::move(m));

    // unread increment (UX only)
    const bool isActive = (activeThreadId_ == threadId);
    if (!isActive || !chatWindowFocused_ || !followScroll_) {
        th->unread = std::min<uint32_t>(th->unread + 1, 999u);
    }
}

void ChatManager::tryHandleSlashCommand(uint64_t threadId, const std::string& input) {
    // /roll NdM(+K)  e.g., /roll 4d6+2  or /roll 1d20-5
    if (input.rfind("/roll", 0) != 0) return;

    auto nm = network_.lock(); if (!nm) return;
    auto uname = nm->getMyUsername(); if (uname.empty()) uname = "me";

    auto parseInt = [](const std::string& s, size_t& i)->int{
        int v=0; bool any=false;
        while (i<s.size() && std::isdigit((unsigned char)s[i])) { any=true; v=v*10+(s[i]-'0'); ++i; }
        return any? v : -1;
    };

    size_t i = 5;
    while (i<input.size() && std::isspace((unsigned char)input[i])) ++i;

    int N = parseInt(input, i);
    if (i>=input.size() || input[i] != 'd' || N<=0) { /*invalid*/ return; }
    ++i;
    int M = parseInt(input, i);
    if (M <= 0) return;

    int K = 0;
    if (i<input.size() && (input[i]=='+' || input[i]=='-')) {
        bool neg = (input[i]=='-'); ++i;
        int Z = parseInt(input, i);
        if (Z>0) K = neg ? -Z : Z;
    }

    std::mt19937 rng((uint32_t)std::random_device{}());
    std::uniform_int_distribution<int> dist(1, std::max(1,M));

    int total = 0;
    std::string rolls;
    for (int r=0;r<N;++r) {
        int die = dist(rng);
        total += die;
        if (!rolls.empty()) rolls += ", ";
        rolls += std::to_string(die);
    }

    // This command outputs as a chat message from "me"
    const int modApplied = K;
    const int finalTotal = total + modApplied;

    const std::string text =
        "Roll: [" + rolls + "], mod " + std::to_string(modApplied) + " => Total: " + std::to_string(finalTotal);

    const uint64_t ts = (uint64_t)nowSec();
    emitChatMessageFrame(threadId, uname, text, ts);
    pushMessageLocal(threadId, "me", uname, text, (double)ts, /*incoming*/false);
}

// ---- public helpers ----

// ----- Rename (local change + network UPDATE) -----
void ChatManager::renameThread(uint64_t threadId, const std::string& newName)
{
    if (!hasCurrent()) return;
    auto it = threads_.find(threadId);
    if (it == threads_.end()) return;

    it->second.displayName = newName;
    emitThreadUpdate(it->second);
}

// ----- Set participants (local change + network UPDATE) -----
// NOTE: we keep the same threadId (stable identity). Deterministic IDs are for creation only.
void ChatManager::setThreadParticipants(uint64_t threadId, std::set<std::string> parts)
{
    if (!hasCurrent()) return;
    auto it = threads_.find(threadId);
    if (it == threads_.end()) return;
    if (threadId == generalThreadId_) return; // General is implicit, no participant list

    it->second.participants = std::move(parts);
    emitThreadUpdate(it->second);
}
// ----- Delete (local + network DELETE) -----
void ChatManager::deleteThread(uint64_t threadId)
{
    if (!hasCurrent()) return;
    if (threadId == generalThreadId_) return;

    // local first
    threads_.erase(threadId);
    if (activeThreadId_ == threadId) activeThreadId_ = generalThreadId_;

    emitThreadDelete(threadId);
}

uint64_t ChatManager::ensureThreadByParticipants(const std::set<std::string>& participants,
                                                 const std::string& displayName) {
    if (!hasCurrent()) return 0;

    if (participants.empty()) {
        ensureGeneral();
        return generalThreadId_;
    }

    if (uint64_t dup = findThreadByParticipants(participants)) {
        if (auto* th = getThread(dup)) {
            if (!displayName.empty()) {
                th->displayName = displayName;
                emitThreadUpdate(*th);
            }
        }
        return dup;
    }

    ChatThreadModel th;
    th.id = makeDeterministicThreadId(participants);
    th.displayName = displayName.empty() ? "Group" : displayName;
    th.participants = participants;
    th.unread = 0;
    threads_.emplace(th.id, th);
    emitThreadCreate(th);
    return th.id;
}

void ChatManager::sendTextToThread(uint64_t threadId, const std::string& text) {
    if (!hasCurrent()) return;
    auto nm = network_.lock(); if (!nm) return;
    auto uname = nm->getMyUsername(); if (uname.empty()) uname = "me";

    if (!text.empty() && text[0] == '/') {
        tryHandleSlashCommand(threadId, text);
        return;
    }

    const uint64_t ts = (uint64_t)nowSec();
    emitChatMessageFrame(threadId, uname, text, ts);
    pushMessageLocal(threadId, "me", uname, text, (double)ts, /*incoming*/false);
}


// ---- Network ----
// ----- High-level apply (used by GameTableManager -> processReceivedMessages) -----
void ChatManager::applyReady(const msg::ReadyMessage& m)
{
    if (!m.bytes || m.bytes->empty()) return; // expecting payload in bytes
    const auto& b = *m.bytes;
    size_t off = 0;

    switch (m.kind)
    {
        case msg::DCType::ChatThreadCreate:
        {
            if (!hasCurrent()) return;
            const uint64_t tableId = Serializer::deserializeUInt64(b, off);
            if (tableId != currentTableId_) return;

            ChatThreadModel th;
            th.id          = Serializer::deserializeUInt64(b, off);
            th.displayName = Serializer::deserializeString(b, off);

            const int pc = Serializer::deserializeInt(b, off);
            for (int i = 0; i < pc; ++i)
                th.participants.insert(Serializer::deserializeString(b, off));

            // General (empty participants) or group (non-empty):
            auto it = threads_.find(th.id);
            if (it == threads_.end())
            {
                threads_.emplace(th.id, std::move(th));
            }
            else
            {
                it->second.displayName  = th.displayName;
                it->second.participants = std::move(th.participants);
            }
            ensureGeneral();
            break;
        }

        case msg::DCType::ChatThreadUpdate:
        {
            if (!hasCurrent()) return;
            const uint64_t tableId = Serializer::deserializeUInt64(b, off);
            if (tableId != currentTableId_) return;

            const uint64_t threadId = Serializer::deserializeUInt64(b, off);
            std::string displayName = Serializer::deserializeString(b, off);
            const int pc = Serializer::deserializeInt(b, off);
            std::set<std::string> parts;
            for (int i = 0; i < pc; ++i)
                parts.insert(Serializer::deserializeString(b, off));

            auto it = threads_.find(threadId);
            if (it == threads_.end())
            {
                ChatThreadModel tmp;
                tmp.id = threadId;
                tmp.displayName = std::move(displayName);
                tmp.participants = std::move(parts);
                threads_.emplace(threadId, std::move(tmp));
            }
            else
            {
                it->second.displayName  = std::move(displayName);
                it->second.participants = std::move(parts);
            }
            ensureGeneral();
            break;
        }

        case msg::DCType::ChatThreadDelete:
        {
            if (!hasCurrent()) return;
            const uint64_t tableId  = Serializer::deserializeUInt64(b, off);
            if (tableId != currentTableId_) return;

            const uint64_t threadId = Serializer::deserializeUInt64(b, off);
            if (threadId == generalThreadId_) return; // never delete General
            threads_.erase(threadId);
            if (activeThreadId_ == threadId) activeThreadId_ = generalThreadId_;
            break;
        }

        case msg::DCType::ChatMessage:
        {
            if (!hasCurrent()) return;

            const uint64_t tableId  = Serializer::deserializeUInt64(b, off);
            const uint64_t threadId = Serializer::deserializeUInt64(b, off);
            const uint64_t ts       = Serializer::deserializeUInt64(b, off);
            const std::string username = Serializer::deserializeString(b, off);
            const std::string text     = Serializer::deserializeString(b, off);

            if (tableId != currentTableId_) return;

            // Append (creates placeholder thread if unknown)
            pushMessageLocal(threadId, /*fromId*/"", username, text, (double)ts, /*incoming*/true);
            break;
        }

        default:
            // ignore other kinds
            break;
    }
}


// ----- Emits (binary frames) -----
// Choose broadcast vs targeted by "participants empty? => General => broadcast"
void ChatManager::emitThreadCreate(const ChatThreadModel& th)
{
    auto nm = network_.lock(); if (!nm || !hasCurrent()) return;

    std::vector<uint8_t> buf;
    Serializer::serializeUInt64(buf, currentTableId_);
    Serializer::serializeUInt64(buf, th.id);
    Serializer::serializeString(buf, th.displayName);
    Serializer::serializeInt(buf, (int)th.participants.size());
    for (auto& p : th.participants) Serializer::serializeString(buf, p);

    if (th.participants.empty())
        nm->broadcastChatThreadFrame(msg::DCType::ChatThreadCreate, buf);
    else
        nm->sendChatThreadFrameTo(th.participants, msg::DCType::ChatThreadCreate, buf);
}

void ChatManager::emitThreadUpdate(const ChatThreadModel& th)
{
    auto nm = network_.lock(); if (!nm || !hasCurrent()) return;

    std::vector<uint8_t> buf;
    Serializer::serializeUInt64(buf, currentTableId_);
    Serializer::serializeUInt64(buf, th.id);
    Serializer::serializeString(buf, th.displayName);
    Serializer::serializeInt(buf, (int)th.participants.size());
    for (auto& p : th.participants) Serializer::serializeString(buf, p);

    if (th.participants.empty())
        nm->broadcastChatThreadFrame(msg::DCType::ChatThreadUpdate, buf);
    else
        nm->sendChatThreadFrameTo(th.participants, msg::DCType::ChatThreadUpdate, buf);
}

void ChatManager::emitThreadDelete(uint64_t threadId)
{
    auto nm = network_.lock(); if (!nm || !hasCurrent()) return;
    if (threadId == generalThreadId_) return;

    // Decide recipients: thread may still be in map to fetch participants
    std::set<std::string> targets;
    if (auto it = threads_.find(threadId); it != threads_.end())
        targets = it->second.participants; // may be empty (broadcast) but usually group

    std::vector<uint8_t> buf;
    Serializer::serializeUInt64(buf, currentTableId_);
    Serializer::serializeUInt64(buf, threadId);

    if (targets.empty())
        nm->broadcastChatThreadFrame(msg::DCType::ChatThreadDelete, buf);
    else
        nm->sendChatThreadFrameTo(targets, msg::DCType::ChatThreadDelete, buf);
}

void ChatManager::emitChatMessageFrame(uint64_t threadId,
                                       const std::string& username,
                                       const std::string& text,
                                       uint64_t ts)
{
    auto nm = network_.lock(); if (!nm || !hasCurrent()) return;

    // Figure recipients: General=broadcast; group=participants
    const ChatThreadModel* th = nullptr;
    if (auto it = threads_.find(threadId); it != threads_.end()) th = &it->second;

    std::vector<uint8_t> buf;
    Serializer::serializeUInt64(buf, currentTableId_);
    Serializer::serializeUInt64(buf, threadId);
    Serializer::serializeUInt64(buf, ts);
    Serializer::serializeString(buf, username);
    Serializer::serializeString(buf, text);

    if (!th || th->participants.empty())
        nm->broadcastChatThreadFrame(msg::DCType::ChatMessage, buf);
    else
        nm->sendChatThreadFrameTo(th->participants, msg::DCType::ChatMessage, buf);
}

// ---- UI ----
void ChatManager::render() {
    if (!hasCurrent()) return;

    ImGui::Begin("ChatWindow");
    chatWindowFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_RootWindow);

    // Layout: left fixed width panel, right expands
    const float leftW = 200.0f;
    ImGui::BeginChild("Left", ImVec2(leftW, 0), true);
    renderLeftPanel(leftW);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("Right", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
    renderRightPanel(leftW);
    ImGui::EndChild();

    ImGui::End();
}

void ChatManager::renderLeftPanel(float width) {
    // Top actions: Create/Delete buttons
    if (ImGui::Button("New Chat")) openCreatePopup_ = true;
    ImGui::SameLine();
    if (ImGui::Button("Delete Chat")) openDeletePopup_ = true;

    ImGui::Separator();
    ImGui::TextUnformatted("Chat Groups");
    ImGui::Separator();

    // Sort by displayName for stable UI (General first)
    std::vector<uint64_t> order; order.reserve(threads_.size());
    for (auto& [id, _] : threads_) order.push_back(id);
    std::sort(order.begin(), order.end(), [&](uint64_t a, uint64_t b){
        if (a == generalThreadId_) return true;
        if (b == generalThreadId_) return false;
        return threads_[a].displayName < threads_[b].displayName;
    });

    for (auto id : order) {
        auto& th = threads_[id];
        const bool selected = (activeThreadId_ == id);

        ImGui::PushID((int)id);
        // full row selectable
        if (ImGui::Selectable(th.displayName.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 0))) {
            activeThreadId_ = id;
            markThreadRead(id);
            focusInput_ = true;
            followScroll_ = true;
        }

        // draw right-aligned unread badge, with fixed row height (won't break layout)
        if (th.unread > 0) {
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImVec2 size(max.x - min.x, max.y - min.y);

            char buf[16];
            if (th.unread < 100) std::snprintf(buf, sizeof(buf), "%u", th.unread);
            else std::snprintf(buf, sizeof(buf), "99+");
            ImVec2 txtSize = ImGui::CalcTextSize(buf);

            const float padX = 6.f, padY = 3.f;
            const float radius = (txtSize.y + padY*2.f) * 0.5f;
            const float marginRight = 8.f;
            ImVec2 center(max.x - marginRight - radius, min.y + size.y*0.5f);

            auto* dl = ImGui::GetWindowDrawList();
            ImU32 bg = ImGui::GetColorU32(ImVec4(0.10f, 0.70f, 0.30f, 1.0f));
            ImU32 fg = ImGui::GetColorU32(ImVec4(1,1,1,1));

            ImVec2 badgeMin(center.x - std::max(radius, txtSize.x*0.5f + padX), center.y - radius);
            ImVec2 badgeMax(center.x + std::max(radius, txtSize.x*0.5f + padX), center.y + radius);
            dl->AddRectFilled(badgeMin, badgeMax, bg, radius);
            ImVec2 textPos(center.x - txtSize.x*0.5f, center.y - txtSize.y*0.5f);
            dl->AddText(textPos, fg, buf);
        }

        ImGui::PopID();
    }

    // Popups
    if (openCreatePopup_) { openCreatePopup_ = false; ImGui::OpenPopup("CreateThread"); }
    renderCreateThreadPopup();
    if (openDeletePopup_) { openDeletePopup_ = false; ImGui::OpenPopup("DeleteThread"); }
    renderDeleteThreadPopup();
}

void ChatManager::renderRightPanel(float /*leftPanelWidth*/) {
    auto* th = getThread(activeThreadId_);
    if (!th) th = getThread(generalThreadId_);

    const float headerRowH = ImGui::GetFrameHeightWithSpacing();
    const float footerRowH = ImGui::GetFrameHeightWithSpacing()*2.0f;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("Messages", ImVec2(0, avail.y - headerRowH - footerRowH), true, ImGuiWindowFlags_HorizontalScrollbar);
    if (th) {
        // if user scrolls up, disable follow
        if (ImGui::GetScrollY() < ImGui::GetScrollMaxY() - 1.0f) followScroll_ = false;

        for (auto& m : th->messages) {
            ImGui::PushTextWrapPos(0);
            ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "%s", m.username.c_str());
            ImGui::SameLine(); ImGui::TextDisabled(" - "); ImGui::SameLine();
            switch (m.kind) {
                case ChatMessageModel::Kind::TEXT: ImGui::TextWrapped("%s", m.content.c_str()); break;
                case ChatMessageModel::Kind::LINK:
                    if (ImGui::SmallButton(m.content.c_str())) {
                        // TODO: open URL (platform hook)
                    }
                    break;
                case ChatMessageModel::Kind::IMAGE:
                    ImGui::TextWrapped("%s", m.content.c_str()); // preview hook later
                    break;
            }
            ImGui::PopTextWrapPos();
        }
        if (followScroll_) ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    ImGui::BeginChild("Footer", ImVec2(0, footerRowH), false, ImGuiWindowFlags_AlwaysAutoResize);
    

    if (focusInput_) { ImGui::SetKeyboardFocusHere(); focusInput_ = false; }

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AllowTabInput;
    if (ImGui::InputText("##chat_input", input_.data(), (int)input_.size(), flags)) {
        std::string text(input_.data());
        if (!text.empty() && th) {
            sendTextToThread(th->id, text);
            input_.fill('\0');
            followScroll_ = true;
            focusInput_ = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Send") && th) {
        std::string text(input_.data());
        if (!text.empty()) {
            sendTextToThread(th->id, text);
            input_.fill('\0');
            followScroll_ = true;
        }
    }
    //ImGui::NewLine(); 
    ImGui::BeginDisabled(followScroll_);
    if (ImGui::Button("Go to bottom"))
        followScroll_ = true;
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Roll Dice")) {
        openDicePopup_ = true;
    }
    if (openDicePopup_) { openDicePopup_ = false; ImGui::OpenPopup("DiceRoller"); }
    renderDicePopup();

    ImGui::EndChild();
}

void ChatManager::renderCreateThreadPopup() {
    if (ImGui::BeginPopupModal("CreateThread", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Thread name:");
        ImGui::InputText("##thname", newThreadName_.data(), (int)newThreadName_.size());

        ImGui::Separator();
        ImGui::TextUnformatted("Participants:");
        auto nm = network_.lock();
        if (nm) {
            for (auto& [pid, link] : nm->getPeers()) {
                bool checked = newThreadSel_.count(pid) > 0;
                const std::string dn = nm->displayNameFor(pid);
                std::string label = dn.empty()? pid : (dn + " (" + pid + ")");
                if (ImGui::Checkbox(label.c_str(), &checked)) {
                    if (checked) newThreadSel_.insert(pid);
                    else newThreadSel_.erase(pid);
                }
            }
        }

        ImGui::Separator();
        if (ImGui::Button("Create")) {
            uint64_t id = ensureThreadByParticipants(newThreadSel_, newThreadName_.data());
            if (id) {
                activeThreadId_ = id;
                markThreadRead(id);
                focusInput_ = true;
                followScroll_ = true;
            }
            newThreadSel_.clear(); newThreadName_.fill('\0');
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            newThreadSel_.clear(); newThreadName_.fill('\0');
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void ChatManager::renderDeleteThreadPopup() {
    if (ImGui::BeginPopupModal("DeleteThread", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Click a thread to delete (General cannot be deleted).");
        ImGui::Separator();

        // Render buttons grid
        for (auto it = threads_.begin(); it != threads_.end(); /*increment inside*/ ) {
            auto id = it->first;
            auto& th = it->second;

            ImGui::PushID((int)id);
            bool isGeneral = (id == generalThreadId_);
            ImGui::BeginDisabled(isGeneral);
            if (ImGui::Button(th.displayName.c_str(), ImVec2(220, 0))) {
                // confirm
                ImGui::OpenPopup("ConfirmDelete");
            }
            ImGui::EndDisabled();

            // confirm modal for this item
            if (ImGui::BeginPopupModal("ConfirmDelete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Delete thread '%s' ?", th.displayName.c_str());
                if (ImGui::Button("Yes, delete")) {
                    emitThreadDelete(id);
                    if (activeThreadId_ == id) activeThreadId_ = generalThreadId_;
                    it = threads_.erase(it);
                    ImGui::CloseCurrentPopup();
                    ImGui::PopID();
                    continue; // skip ++it
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            ImGui::PopID();

            ++it;
        }

        ImGui::Separator();
        if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void ChatManager::renderDicePopup() {
    if (ImGui::BeginPopup("DiceRoller")) {
        // anchor: centered at bottom of popup
        ImGui::TextUnformatted("Dice Roller");
        ImGui::Separator();

        ImGui::InputInt("Number", &diceN_);
        ImGui::InputInt("Sides",  &diceSides_);
        ImGui::InputInt("Modifier", &diceMod_);
        ImGui::Checkbox("Apply modifier per die", &diceModPerDie_);

        // quick side buttons
        if (ImGui::Button("d4"))  diceSides_ = 4;  ImGui::SameLine();
        if (ImGui::Button("d6"))  diceSides_ = 6;  ImGui::SameLine();
        if (ImGui::Button("d8"))  diceSides_ = 8;  ImGui::SameLine();
        if (ImGui::Button("d10")) diceSides_ = 10; ImGui::SameLine();
        if (ImGui::Button("d12")) diceSides_ = 12; ImGui::SameLine();
        if (ImGui::Button("d20")) diceSides_ = 20;

        ImGui::Separator();
        if (ImGui::Button("Roll")) {
            auto* th = getThread(activeThreadId_);
            if (!th) th = getThread(generalThreadId_);
            if (th) {
                // build slash command
                int mod = diceMod_;
                int N = std::max(1, diceN_);
                int M = std::max(1, diceSides_);
                // If per-die modifier, convert into +K where K = modifier * N for the final text.
                int K = diceModPerDie_ ? (mod * N) : mod;
                std::string cmd = "/roll " + std::to_string(N) + "d" + std::to_string(M);
                if      (K > 0) cmd += "+" + std::to_string(K);
                else if (K < 0) cmd += "-" + std::to_string(-K);
                sendTextToThread(th->id, cmd);
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

/*#include "ChatManager.h"
#include "NetworkManager.h"
#include "PeerLink.h"
#include "Serializer.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <optional>

// --------- small local helpers ---------
static double nowSec()
{
    return (double)std::time(nullptr);
}

static bool ieq(const std::string& a, const char* b)
{
    return std::equal(a.begin(), a.end(), b, b + std::strlen(b),
                      [](char x, char y)
                      { return (char)std::tolower((unsigned char)x) == (char)std::tolower((unsigned char)y); });
}

static ChatMessageModel::Kind classifyMessage(const std::string& s)
{
    auto ends_with = [](const std::string& a, const char* suf)
    {
        const size_t n = std::strlen(suf);
        if (a.size() < n)
            return false;
        return std::equal(a.end() - n, a.end(), suf, suf + n,
                          [](char c1, char c2)
                          {
                              return (char)std::tolower((unsigned char)c1) == (char)std::tolower((unsigned char)c2);
                          });
    };
    if (ends_with(s, ".png") || ends_with(s, ".jpg") || ends_with(s, ".jpeg"))
        return ChatMessageModel::Kind::IMAGE;
    if (s.rfind("http://", 0) == 0 || s.rfind("https://", 0) == 0)
        return ChatMessageModel::Kind::LINK;
    return ChatMessageModel::Kind::TEXT;
}

// --------- ctor / state ---------
ChatManager::ChatManager(std::weak_ptr<NetworkManager> nm) :
    network_(std::move(nm))
{
}

void ChatManager::setNetwork(std::weak_ptr<NetworkManager> nm)
{
    network_ = std::move(nm);
}

void ChatManager::setActiveGameTable(uint64_t tableId, const std::string& gameTableName)
{
    // persist old
    if (currentTableId_ != 0)
        saveCurrent();

    currentTableId_ = tableId;
    currentTableName_ = gameTableName;
    // load or init
    loadCurrent();
    ensureGeneral();
}

bool ChatManager::hasCurrent() const
{
    return currentTableId_ != 0;
}

// --------- persistence ---------
std::filesystem::path ChatManager::chatFilePathFor(uint64_t tableId, const std::string& name) const
{
    // Keep it simple: <name>_<id>_chat.runic
    // If you prefer only name, replace accordingly (beware collisions).
    return std::filesystem::path(name + "_" + std::to_string(tableId) + "_chat.runic");
}

bool ChatManager::saveCurrent()
{
    if (!hasCurrent())
        return false;

    std::vector<unsigned char> buf;
    Serializer::serializeString(buf, "RUNIC-CHAT");
    Serializer::serializeInt(buf, 3); // version 3: threads keyed by u64 id

    // write threads count
    Serializer::serializeInt(buf, (int)threads_.size());
    for (auto& kv : threads_)
    {
        const auto& th = kv.second;
        Serializer::serializeUInt64(buf, th.id);
        Serializer::serializeString(buf, th.displayName);

        // participants
        Serializer::serializeInt(buf, (int)th.participants.size());
        for (auto& pid : th.participants)
            Serializer::serializeString(buf, pid);

        // messages
        Serializer::serializeInt(buf, (int)th.messages.size());
        for (auto& m : th.messages)
        {
            Serializer::serializeInt(buf, (int)m.kind);
            Serializer::serializeString(buf, m.senderId);
            Serializer::serializeString(buf, m.username);
            Serializer::serializeString(buf, m.content);
            Serializer::serializeUInt64(buf, (uint64_t)m.ts);
        }
    }

    try
    {
        auto p = chatFilePathFor(currentTableId_, currentTableName_);
        std::ofstream os(p, std::ios::binary);
        os.write((const char*)buf.data(), (std::streamsize)buf.size());
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool ChatManager::loadCurrent()
{
    threads_.clear();
    if (!hasCurrent())
    {
        ensureGeneral();
        return false;
    }

    auto p = chatFilePathFor(currentTableId_, currentTableName_);
    if (!std::filesystem::exists(p))
    {
        ensureGeneral();
        return false;
    }

    std::vector<unsigned char> buf;
    try
    {
        std::ifstream is(p, std::ios::binary);
        is.seekg(0, std::ios::end);
        auto sz = is.tellg();
        is.seekg(0, std::ios::beg);
        buf.resize((size_t)sz);
        is.read((char*)buf.data(), sz);
    }
    catch (...)
    {
        ensureGeneral();
        return false;
    }

    size_t off = 0;
    auto magic = Serializer::deserializeString(buf, off);
    if (magic != "RUNIC-CHAT")
    {
        ensureGeneral();
        return false;
    }
    const int version = Serializer::deserializeInt(buf, off);
    (void)version;

    const int tcount = Serializer::deserializeInt(buf, off);
    for (int i = 0; i < tcount; ++i)
    {
        ChatThreadModel th;
        th.id = Serializer::deserializeUInt64(buf, off);
        th.displayName = Serializer::deserializeString(buf, off);

        const int pcount = Serializer::deserializeInt(buf, off);
        for (int k = 0; k < pcount; ++k)
        {
            th.participants.insert(Serializer::deserializeString(buf, off));
        }

        const int mcount = Serializer::deserializeInt(buf, off);
        for (int k = 0; k < mcount; ++k)
        {
            ChatMessageModel m;
            m.kind = (ChatMessageModel::Kind)Serializer::deserializeInt(buf, off);
            m.senderId = Serializer::deserializeString(buf, off);
            m.username = Serializer::deserializeString(buf, off);
            m.content = Serializer::deserializeString(buf, off);
            m.ts = (double)Serializer::deserializeUInt64(buf, off);
            th.messages.push_back(std::move(m));
        }
        threads_.emplace(th.id, std::move(th));
    }

    ensureGeneral();
    if (threads_.find(activeThreadId_) == threads_.end())
        activeThreadId_ = generalThreadId_;
    return true;
}

// --------- general thread helpers ---------
void ChatManager::ensureGeneral()
{
    // general thread id is a fixed synthetic value (e.g., 1)
    if (threads_.find(generalThreadId_) == threads_.end())
    {
        ChatThreadModel th;
        th.id = generalThreadId_;
        th.displayName = "General";
        threads_.emplace(th.id, std::move(th));
    }
    activeThreadId_ = generalThreadId_;
}

void ChatManager::refreshGeneralParticipants(const std::unordered_map<std::string, std::shared_ptr<PeerLink>>& peers)
{
    auto it = threads_.find(generalThreadId_);
    if (it == threads_.end())
        return;
    auto& th = it->second;

    th.participants.clear();
    for (auto& kv : peers)
    {
        th.participants.insert(kv.first);
    }
    // displayName as "General (N)"
    th.displayName = "General (" + std::to_string(th.participants.size()) + ")";
}

ChatThreadModel* ChatManager::getThread(uint64_t id)
{
    auto it = threads_.find(id);
    if (it == threads_.end())
        return nullptr;
    return &it->second;
}

uint64_t ChatManager::findThreadByParticipants(const std::set<std::string>& parts) const
{
    // never match "General" here (it has empty participants)
    for (auto& kv : threads_)
    {
        const auto& th = kv.second;
        if (th.id == generalThreadId_)
            continue;
        if (th.participants == parts)
            return th.id;
    }
    return 0;
}

uint64_t ChatManager::makeDeterministicThreadId(const std::set<std::string>& parts)
{
    // simple hash on a sorted joined string "a|b|c"
    std::string joined;
    bool first = true;
    for (auto& p : parts)
    {
        if (!first)
            joined.push_back('|');
        joined += p;
        first = false;
    }
    std::hash<std::string> H;
    uint64_t id = H(joined);
    if (id == 0 || id == generalThreadId_)
        id ^= 0x9E3779B97F4A7C15ull; // avoid special collisions
    return id;
}

// --------- snapshot (GameTable) meta only ---------
void ChatManager::writeThreadsToSnapshotGT(std::vector<unsigned char>& buf) const
{
    // write only metadata (no messages): [u32 count] then for each:
    // [u64 threadId][string displayName][u32 partCount]{string peerId...}
    Serializer::serializeInt(buf, (int)threads_.size());
    for (auto& kv : threads_)
    {
        const auto& th = kv.second;
        Serializer::serializeUInt64(buf, th.id);
        Serializer::serializeString(buf, th.displayName);
        Serializer::serializeInt(buf, (int)th.participants.size());
        for (auto& p : th.participants)
            Serializer::serializeString(buf, p);
    }
}

void ChatManager::readThreadsFromSnapshotGT(const std::vector<unsigned char>& buf, size_t& off)
{
    if (!hasCurrent())
        return;
    const int count = Serializer::deserializeInt(buf, off);
    for (int i = 0; i < count; ++i)
    {
        ChatThreadModel th;
        th.id = Serializer::deserializeUInt64(buf, off);
        th.displayName = Serializer::deserializeString(buf, off);
        const int pc = Serializer::deserializeInt(buf, off);
        for (int k = 0; k < pc; ++k)
            th.participants.insert(Serializer::deserializeString(buf, off));

        auto it = threads_.find(th.id);
        if (it == threads_.end())
        {
            threads_.emplace(th.id, std::move(th));
        }
        else
        {
            it->second.displayName = th.displayName;
            it->second.participants = std::move(th.participants);
        }
    }
    ensureGeneral();
}

// --------- emits (binary frames) ---------
void ChatManager::emitThreadCreate(const ChatThreadModel& th)
{
    auto nm = network_.lock();
    if (!nm)
        return;
    std::vector<uint8_t> buf;
    Serializer::serializeUInt64(buf, currentTableId_);
    Serializer::serializeUInt64(buf, th.id);
    Serializer::serializeString(buf, th.displayName);
    Serializer::serializeInt(buf, (int)th.participants.size());
    for (auto& p : th.participants)
        Serializer::serializeString(buf, p);
    nm->broadcastChatThreadFrame(msg::DCType::ChatThreadCreate, buf);
}

void ChatManager::emitThreadUpdate(const ChatThreadModel& th)
{
    auto nm = network_.lock();
    if (!nm)
        return;
    std::vector<uint8_t> buf;
    Serializer::serializeUInt64(buf, currentTableId_);
    Serializer::serializeUInt64(buf, th.id);
    Serializer::serializeString(buf, th.displayName);
    Serializer::serializeInt(buf, (int)th.participants.size());
    for (auto& p : th.participants)
        Serializer::serializeString(buf, p);
    nm->broadcastChatThreadFrame(msg::DCType::ChatThreadUpdate, buf);
}

void ChatManager::emitThreadDelete(uint64_t threadId)
{
    auto nm = network_.lock();
    if (!nm)
        return;
    std::vector<uint8_t> buf;
    Serializer::serializeUInt64(buf, currentTableId_);
    Serializer::serializeUInt64(buf, threadId);
    nm->broadcastChatThreadFrame(msg::DCType::ChatThreadDelete, buf);
}

void ChatManager::emitChatMessageFrame(uint64_t threadId, const std::string& username,
                                       const std::string& text, uint64_t ts)
{
    auto nm = network_.lock();
    if (!nm)
        return;

    std::vector<uint8_t> buf;
    Serializer::serializeUInt64(buf, currentTableId_);
    Serializer::serializeUInt64(buf, threadId);
    Serializer::serializeUInt64(buf, ts);
    Serializer::serializeString(buf, username);
    Serializer::serializeString(buf, text);

    auto* th = getThread(threadId);
    if (!th)
        return;

    // general -> broadcast; group -> send to participants
    if (th->id == generalThreadId_ || th->participants.empty())
    {
        nm->broadcastChatThreadFrame(msg::DCType::ChatMessage, buf);
    }
    else
    {
        nm->sendChatThreadFrameTo(th->participants, msg::DCType::ChatMessage, buf);
    }
}

// --------- inbound frames (binary) ---------
void ChatManager::onChatThreadCreateFrame(const std::vector<uint8_t>& b, size_t& off)
{
    if (!hasCurrent())
        return;
    const uint64_t tableId = Serializer::deserializeUInt64(b, off);
    if (tableId != currentTableId_)
        return;

    ChatThreadModel th;
    th.id = Serializer::deserializeUInt64(b, off);
    th.displayName = Serializer::deserializeString(b, off);
    const int pc = Serializer::deserializeInt(b, off);
    for (int i = 0; i < pc; ++i)
        th.participants.insert(Serializer::deserializeString(b, off));

    // dedupe by participants (unless it's General)
    if (!th.participants.empty())
    {
        if (uint64_t dup = findThreadByParticipants(th.participants))
        {
            auto* cur = getThread(dup);
            if (cur)
                cur->displayName = th.displayName;
            return;
        }
    }
    threads_.emplace(th.id, std::move(th));
}

void ChatManager::onChatThreadUpdateFrame(const std::vector<uint8_t>& b, size_t& off)
{
    if (!hasCurrent())
        return;
    const uint64_t tableId = Serializer::deserializeUInt64(b, off);
    if (tableId != currentTableId_)
        return;

    const uint64_t threadId = Serializer::deserializeUInt64(b, off);
    std::string displayName = Serializer::deserializeString(b, off);
    const int pc = Serializer::deserializeInt(b, off);
    std::set<std::string> parts;
    for (int i = 0; i < pc; ++i)
        parts.insert(Serializer::deserializeString(b, off));

    auto* th = getThread(threadId);
    if (!th)
    {
        ChatThreadModel tmp;
        tmp.id = threadId;
        tmp.displayName = std::move(displayName);
        tmp.participants = std::move(parts);
        threads_.emplace(threadId, std::move(tmp));
    }
    else
    {
        th->displayName = std::move(displayName);
        th->participants = std::move(parts);
    }
}

void ChatManager::onChatThreadDeleteFrame(const std::vector<uint8_t>& b, size_t& off)
{
    if (!hasCurrent())
        return;
    const uint64_t tableId = Serializer::deserializeUInt64(b, off);
    if (tableId != currentTableId_)
        return;
    const uint64_t threadId = Serializer::deserializeUInt64(b, off);

    if (threadId == generalThreadId_)
        return; // never delete General
    threads_.erase(threadId);
    if (activeThreadId_ == threadId)
        activeThreadId_ = generalThreadId_;
}

void ChatManager::onIncomingChatFrame(const std::vector<uint8_t>& b, size_t& off)
{
    if (!hasCurrent())
        return;

    const uint64_t tableId = Serializer::deserializeUInt64(b, off);
    const uint64_t threadId = Serializer::deserializeUInt64(b, off);
    const uint64_t ts = Serializer::deserializeUInt64(b, off);
    const std::string username = Serializer::deserializeString(b, off);
    const std::string text = Serializer::deserializeString(b, off);

    if (tableId != currentTableId_)
        return;

    auto* th = getThread(threadId);
    if (!th)
    {
        ChatThreadModel tmp;
        tmp.id = threadId;
        tmp.displayName = "Thread " + std::to_string(threadId);
        threads_.emplace(threadId, std::move(tmp));
        th = getThread(threadId);
    }

    ChatMessageModel m;
    m.kind = classifyMessage(text);
    m.senderId = ""; // optional: fill if you carry peerId out of band
    m.username = username.empty() ? "guest" : username;
    m.content = text;
    m.ts = (double)ts;
    th->messages.push_back(std::move(m));
}

// --------- thread creation API (local UI calls) ---------
uint64_t ChatManager::ensureThreadByParticipants(const std::set<std::string>& participants,
                                                 const std::string& displayName)
{
    if (!hasCurrent())
        return 0;

    // General: empty participants maps to general
    if (participants.empty())
    {
        ensureGeneral();
        return generalThreadId_;
    }

    // dedupe
    if (uint64_t dup = findThreadByParticipants(participants))
    {
        auto* th = getThread(dup);
        if (th && !displayName.empty())
        {
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

void ChatManager::sendTextToThread(uint64_t threadId, const std::string& text)
{
    if (!hasCurrent())
        return;

    auto nm = network_.lock();
    if (!nm)
        return;
    const std::string uname = nm->getMyUsername().empty() ? "me" : nm->getMyUsername();
    const uint64_t ts = (uint64_t)nowSec();

    emitChatMessageFrame(threadId, uname, text, ts);

    // local append
    auto* th = getThread(threadId);
    if (!th)
        return;
    ChatMessageModel m;
    m.kind = classifyMessage(text);
    m.senderId = "me";
    m.username = uname;
    m.content = text;
    m.ts = (double)ts;
    th->messages.push_back(std::move(m));
}
*/


#include "ChatManager.h"
#include "NetworkManager.h"
#include "PeerLink.h"
#include "Serializer.h"
#include "Logger.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <random>
#include <cstdint>
#include <unordered_map>
#include "PathManager.h"

// ====== small utils ======
double ChatManager::nowSec()
{
    return (double)std::time(nullptr);
}

static bool ends_with_icase(const std::string& a, const char* suf)
{
    const size_t n = std::strlen(suf);
    if (a.size() < n)
        return false;
    return std::equal(a.end() - n, a.end(), suf, suf + n, [](char c1, char c2)
                      { return (char)std::tolower((unsigned char)c1) == (char)std::tolower((unsigned char)c2); });
}

ChatMessageModel::Kind ChatManager::classifyMessage(const std::string& s)
{
    if (ends_with_icase(s, ".png") || ends_with_icase(s, ".jpg") || ends_with_icase(s, ".jpeg"))
        return ChatMessageModel::Kind::IMAGE;
    if (s.rfind("http://", 0) == 0 || s.rfind("https://", 0) == 0)
        return ChatMessageModel::Kind::LINK;
    return ChatMessageModel::Kind::TEXT;
}

// ====== ctor/bind ======
ChatManager::ChatManager(std::weak_ptr<NetworkManager> nm, std::shared_ptr<IdentityManager> identity_manager) :
    network_(std::move(nm)), identity_manager(identity_manager) {}
void ChatManager::setNetwork(std::weak_ptr<NetworkManager> nm)
{
    network_ = std::move(nm);
}

// ====== context ======
void ChatManager::setActiveGameTable(uint64_t tableId, const std::string& gameTableName)
{
    if (currentTableId_ != 0)
        saveCurrent();
    currentTableId_ = tableId;
    currentTableName_ = gameTableName;

    groups_.clear();
    ensureGeneral();

    loadCurrent(); // will merge/overwrite groups_ from disk

    activeGroupId_ = generalGroupId_;
    focusInput_ = true;
    followScroll_ = true;
}

void ChatManager::ensureGeneral()
{
    if (groups_.find(generalGroupId_) == groups_.end())
    {
        ChatGroupModel g;
        g.id = generalGroupId_;
        g.name = "General";
        g.ownerUniqueId.clear(); // nobody “owns” General
        groups_.emplace(g.id, std::move(g));
    }
}

// ====== storage ======
std::filesystem::path ChatManager::chatFilePathFor(uint64_t tableId, const std::string& name) const
{
    auto gametables_folder = PathManager::getGameTablesPath();
    auto gametable_name_folder = gametables_folder / name;
    return gametable_name_folder / std::filesystem::path(name + "_" + std::to_string(tableId) + "_chatgroups.runic");
}

bool ChatManager::saveLog(std::vector<uint8_t>& buf) const
{
    Serializer::serializeString(buf, "RUNIC-CHAT-GROUPS");
    Serializer::serializeInt(buf, 2); // <— version 2

    Serializer::serializeInt(buf, (int)groups_.size());
    for (auto& [id, g] : groups_)
    {
        Serializer::serializeUInt64(buf, g.id);
        Serializer::serializeString(buf, g.name);
        Serializer::serializeString(buf, g.ownerUniqueId); // <— changed

        Serializer::serializeInt(buf, (int)g.participants.size());
        for (auto& p : g.participants)
            Serializer::serializeString(buf, p); // your set contents (uniqueIds recommended)

        Serializer::serializeInt(buf, (int)g.messages.size());
        for (auto& m : g.messages)
        {
            Serializer::serializeInt(buf, (int)m.kind);
            Serializer::serializeString(buf, m.senderUniqueId); // <— NEW in v2
            Serializer::serializeString(buf, m.username);
            Serializer::serializeString(buf, m.content);
            Serializer::serializeUInt64(buf, (uint64_t)m.ts);
        }
        Serializer::serializeInt(buf, (int)g.unread);
    }
    return true;
}
bool ChatManager::loadLog(const std::vector<uint8_t>& buf)
{
    size_t off = 0;
    auto magic = Serializer::deserializeString(buf, off);
    if (magic != "RUNIC-CHAT-GROUPS")
        return false;

    int version = Serializer::deserializeInt(buf, off);

    groups_.clear();
    int count = Serializer::deserializeInt(buf, off);
    for (int i = 0; i < count; ++i)
    {
        ChatGroupModel g;
        g.id = Serializer::deserializeUInt64(buf, off);
        g.name = Serializer::deserializeString(buf, off);

        if (version >= 2)
            g.ownerUniqueId = Serializer::deserializeString(buf, off);
        else
        {
            // v1 had ownerPeerId; read it and keep as ownerUniqueId (best-effort: it used to be peerId)
            const std::string legacyOwner = Serializer::deserializeString(buf, off);
            g.ownerUniqueId = legacyOwner; // you can map through IdentityManager if you persisted mapping
        }

        int pc = Serializer::deserializeInt(buf, off);
        for (int k = 0; k < pc; ++k)
            g.participants.insert(Serializer::deserializeString(buf, off));

        int mc = Serializer::deserializeInt(buf, off);
        for (int m = 0; m < mc; ++m)
        {
            ChatMessageModel msg;
            msg.kind = (ChatMessageModel::Kind)Serializer::deserializeInt(buf, off);

            if (version >= 2)
                msg.senderUniqueId = Serializer::deserializeString(buf, off);
            else
                msg.senderUniqueId.clear();

            msg.username = Serializer::deserializeString(buf, off);
            msg.content = Serializer::deserializeString(buf, off);
            msg.ts = (double)Serializer::deserializeUInt64(buf, off);
            g.messages.push_back(std::move(msg));
        }

        g.unread = (uint32_t)Serializer::deserializeInt(buf, off);
        groups_.emplace(g.id, std::move(g));
    }

    ensureGeneral();
    if (groups_.find(activeGroupId_) == groups_.end())
        activeGroupId_ = generalGroupId_;
    return true;
}

bool ChatManager::saveCurrent()
{
    if (!hasCurrent())
        return false;
    try
    {
        std::vector<uint8_t> buf;
        saveLog(buf);
        auto p = chatFilePathFor(currentTableId_, currentTableName_);
        std::ofstream os(p, std::ios::binary | std::ios::trunc);
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
    if (!hasCurrent())
        return false;
    auto p = chatFilePathFor(currentTableId_, currentTableName_);
    if (!std::filesystem::exists(p))
    {
        ensureGeneral();
        return false;
    }

    try
    {
        std::ifstream is(p, std::ios::binary);
        is.seekg(0, std::ios::end);
        auto sz = is.tellg();
        is.seekg(0, std::ios::beg);
        std::vector<uint8_t> buf((size_t)sz);
        if (sz > 0)
            is.read((char*)buf.data(), sz);
        return loadLog(buf);
    }
    catch (...)
    {
        ensureGeneral();
        return false;
    }
}

// ====== ids ======
uint64_t ChatManager::makeGroupIdFromName(const std::string& name) const
{
    std::hash<std::string> H;
    uint64_t id = H(name);
    if (id == 0 || id == generalGroupId_)
        id ^= 0x9E3779B97F4A7C15ull;
    return id;
}

// Returns true if my UID is currently a participant of group g.
bool ChatManager::isMeParticipantOf(const ChatGroupModel& g) const
{
    if (!identity_manager)
        return false;
    const std::string meUid = identity_manager->myUniqueId();
    return g.participants.count(meUid) > 0;
}

ChatGroupModel* ChatManager::getGroup(uint64_t id)
{
    auto it = groups_.find(id);
    return it == groups_.end() ? nullptr : &it->second;
}
void ChatManager::applyReady(const msg::ReadyMessage& m)
{
    using K = msg::DCType;

    switch (m.kind)
    {
        case K::ChatGroupCreate:
        {
            if (!m.tableId || !m.threadId)
                return;
            if (*m.tableId != currentTableId_)
                return;

            ChatGroupModel g;
            g.id = *m.threadId;
            g.name = m.name.value_or("Group");

            // derive owner uniqueId (best-effort)
            std::string ownerUid;
            if (identity_manager)
                ownerUid = identity_manager->uniqueForPeer(m.fromPeerId).value_or("Player");
            g.ownerUniqueId = ownerUid;

            if (m.participants)
                g.participants = *m.participants;

            auto it = groups_.find(g.id);
            if (it == groups_.end())
                groups_.emplace(g.id, std::move(g));
            else
            {
                it->second.name = g.name;
                it->second.participants = std::move(g.participants);
                // keep existing ownerUniqueId if you want
            }

            Logger::instance().log("chat", Logger::Level::Info, "RX GroupCreate id=" + std::to_string(*m.threadId) + " name=" + m.name.value_or("Group"));
            break;
        }

        case K::ChatGroupUpdate:
        {
            if (!m.tableId || !m.threadId)
                return;
            if (*m.tableId != currentTableId_)
                return;

            auto* g = getGroup(*m.threadId);
            if (!g)
            {
                ChatGroupModel stub;
                stub.id = *m.threadId;
                stub.name = m.name.value_or("Group");
                if (m.participants)
                    stub.participants = *m.participants;

                // best-effort owner
                if (identity_manager)
                    stub.ownerUniqueId = identity_manager->uniqueForPeer(m.fromPeerId).value_or("Player");

                groups_.emplace(stub.id, std::move(stub));
            }
            else
            {
                if (m.name)
                    g->name = *m.name;
                if (m.participants)
                    g->participants = *m.participants;
            }
            break;
        }

        case K::ChatGroupDelete:
        {
            if (!m.tableId || !m.threadId)
                return;
            if (*m.tableId != currentTableId_)
                return;
            if (*m.threadId == generalGroupId_)
                return; // never delete General

            if (auto* g = getGroup(*m.threadId))
            {
                // permission: only owner can delete
                const std::string reqOwner =
                    identity_manager ? identity_manager->uniqueForPeer(m.fromPeerId).value_or("Player") : std::string{};
                if (!g->ownerUniqueId.empty() && g->ownerUniqueId == reqOwner)
                {
                    groups_.erase(*m.threadId);
                    if (activeGroupId_ == *m.threadId)
                        activeGroupId_ = generalGroupId_;
                }
            }
            break;
        }

        case K::ChatMessage:
        {
            if (!m.tableId || !m.threadId || !m.ts || !m.name || !m.text)
                return;
            if (*m.tableId != currentTableId_)
                return;

            // map sender to uniqueId if possible (if ReadyMessage.userPeerId later carries it, use that)
            std::string senderUid =
                identity_manager ? identity_manager->uniqueForPeer(m.fromPeerId).value_or("Player") : std::string{};

            // inline append to include senderUniqueId
            ChatMessageModel msg;
            msg.kind = classifyMessage(*m.text);
            msg.senderUniqueId = senderUid;
            msg.username = *m.name;
            msg.content = *m.text;
            msg.ts = (double)*m.ts;

            auto* g = getGroup(*m.threadId);
            if (!g)
            {
                ChatGroupModel stub;
                stub.id = *m.threadId;
                stub.name = "(pending?)";
                groups_.emplace(stub.id, std::move(stub));
                g = getGroup(*m.threadId);
            }
            g->messages.push_back(std::move(msg));

            const bool isActive = (activeGroupId_ == *m.threadId);
            if (!isActive || !chatWindowFocused_ || !followScroll_)
                g->unread = std::min<uint32_t>(g->unread + 1, 999u);
            break;
        }

        default:
            break;
    }
}

// ====== local append ======
void ChatManager::pushMessageLocal(uint64_t groupId,
                                   const std::string& fromPeer,
                                   const std::string& username,
                                   const std::string& text,
                                   double ts,
                                   bool incoming)
{
    ChatMessageModel msg;
    msg.kind = classifyMessage(text);
    msg.username = username;
    msg.content = text;
    msg.ts = ts;

    if (incoming)
    {
        // map from transport peer -> stable uniqueId (best effort)
        msg.senderUniqueId = identity_manager ? identity_manager->uniqueForPeer(fromPeer).value_or("Player") : std::string{};
    }
    else
    {
        // local user’s unique id
        msg.senderUniqueId = identity_manager ? identity_manager->myUniqueId() : std::string{};
    }

    auto* g = getGroup(groupId);
    if (!g)
    {
        ChatGroupModel stub;
        stub.id = groupId;
        stub.name = "(pending?)";
        groups_.emplace(groupId, std::move(stub));
        g = getGroup(groupId);
    }

    g->messages.push_back(std::move(msg));

    const bool isActive = (activeGroupId_ == groupId);
    if (!isActive || !chatWindowFocused_ || !followScroll_)
        g->unread = std::min<uint32_t>(g->unread + 1, 999u);

    Logger::instance().log("chat", Logger::Level::Info,
                           "LOCAL append gid=" + std::to_string(groupId) + " user=" + username +
                               " text=\"" + text + "\"");
}

// ====== emitters ======

void ChatManager::emitGroupCreate(const ChatGroupModel& g)
{
    auto nm = network_.lock();
    if (!nm || !hasCurrent())
        return;

    auto j = msg::makeChatGroupCreate(currentTableId_, g.id, g.name, g.participants);

    if (g.id == generalGroupId_)
    {
        nm->broadcastChatJson(j);
        return;
    }

    auto targets = resolvePeerIdsForParticipants(g.participants);
    if (!targets.empty())
        nm->sendChatJsonTo(targets, j);
}

void ChatManager::emitGroupUpdate(const ChatGroupModel& g)
{
    auto nm = network_.lock();
    if (!nm || !hasCurrent())
        return;

    auto j = msg::makeChatGroupUpdate(currentTableId_, g.id, g.name, g.participants);

    if (g.id == generalGroupId_)
    {
        nm->broadcastChatJson(j);
        return;
    }

    auto targets = resolvePeerIdsForParticipants(g.participants);
    if (!targets.empty())
        nm->sendChatJsonTo(targets, j);
}

void ChatManager::emitGroupDelete(uint64_t groupId)
{
    auto nm = network_.lock();
    if (!nm || !hasCurrent())
        return;

    if (groupId == generalGroupId_)
        return;

    auto it = groups_.find(groupId);
    if (it == groups_.end())
        return;

    const auto& g = it->second;

    auto j = msg::makeChatGroupDelete(currentTableId_, groupId);

    auto targets = resolvePeerIdsForParticipants(g.participants);

    if (!targets.empty())
        nm->sendChatJsonTo(targets, j);
}

void ChatManager::emitChatMessageFrame(uint64_t groupId, const std::string& username, const std::string& text, uint64_t ts)
{
    auto nm = network_.lock();
    if (!nm || !hasCurrent())
        return;

    auto it = groups_.find(groupId);
    if (it == groups_.end())
        return;

    const auto& parts = it->second.participants;
    auto j = msg::makeChatMessage(currentTableId_, groupId, ts, username, text);
    if (groupId == generalGroupId_)
    {
        nm->broadcastChatJson(j);
        return;
    }
    auto targets = resolvePeerIdsForParticipants(parts);
    if (!targets.empty())
        nm->sendChatJsonTo(targets, j);
}

void ChatManager::emitGroupLeave(uint64_t groupId)
{
    auto nm = network_.lock();
    if (!nm || !hasCurrent())
        return;

    // General cannot be “left”
    if (groupId == generalGroupId_)
        return;

    auto it = groups_.find(groupId);
    if (it == groups_.end())
        return;

    auto& g = it->second;
    if (!identity_manager)
        return;

    const std::string meUid = identity_manager->myUniqueId();

    // Owner cannot leave their own group
    if (!g.ownerUniqueId.empty() && g.ownerUniqueId == meUid)
        return;

    // Remove me locally
    g.participants.erase(meUid);

    // Broadcast updated participants (re-using ChatGroupUpdate)
    auto j = msg::makeChatGroupUpdate(currentTableId_, g.id, g.name, g.participants);
    auto targets = resolvePeerIdsForParticipants(g.participants);
    if (!targets.empty())
        nm->sendChatJsonTo(targets, j);

    // UX: if I’m no longer in it, stop showing it as active
    if (activeGroupId_ == groupId && g.participants.count(meUid) == 0)
        activeGroupId_ = generalGroupId_;
}

std::set<std::string> ChatManager::resolvePeerIdsForParticipants(const std::set<std::string>& participantUids) const
{
    std::set<std::string> out;
    auto nm = network_.lock();
    if (!nm || !identity_manager)
        return out;

    for (const auto& uid : participantUids)
    {
        if (auto pid = identity_manager->peerForUnique(uid); pid && !pid->empty())
        {
            out.insert(*pid);
        }
    }
    return out;
}

void ChatManager::replaceUsernameForUnique(const std::string& uniqueId,
                                           const std::string& newUsername)
{
    // update cached labels in message history for this author
    for (auto& [gid, g] : groups_)
    {
        // optional: if you show "owner" anywhere:
        if (g.ownerUniqueId == uniqueId)
        {
            // no change to g.name; but if you render "owned by X", use identity_ at draw time.
        }

        for (auto& m : g.messages)
        {
            if (m.senderUniqueId == uniqueId)
                m.username = newUsername; // cached label shown in UI
        }
    }
}

// ====== UI ======
/*
const float fullW = ImGui::GetContentRegionAvail().x;
    const float fullH = ImGui::GetContentRegionAvail().y;

    ImGui::BeginChild("##Dir", ImVec2(leftWidth_, fullH), true);
    renderDirectory_(fullH);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::InvisibleButton("##splitter", ImVec2(3, fullH));
    if (ImGui::IsItemActive())
    {
        leftWidth_ += ImGui::GetIO().MouseDelta.x;
        leftWidth_ = std::clamp(leftWidth_, 180.0f, fullW - 240.0f);
    }
    ImGui::SameLine();
*/
void ChatManager::render()
{
    if (!hasCurrent())
        return;

    ImGui::Begin("ChatWindow");
    chatWindowFocused_ = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows | ImGuiFocusedFlags_RootWindow);

    const float leftWmin = 170.0f;

    const float fullW = ImGui::GetContentRegionAvail().x;
    const float fullH = ImGui::GetContentRegionAvail().y;

    ImGui::BeginChild("Left", ImVec2(leftWidth_, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
    renderLeftPanel(leftWidth_);
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::InvisibleButton("##splitter", ImVec2(6, fullH));
    if (ImGui::IsItemActive())
    {
        leftWidth_ += ImGui::GetIO().MouseDelta.x;
        leftWidth_ = std::clamp(leftWidth_, leftWmin, fullW - 240.0f);
    }
    ImGui::SameLine();

    ImGui::BeginChild("Right", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize);
    renderRightPanel(leftWidth_);
    ImGui::EndChild();

    ImGui::End();
}

ImVec4 ChatManager::HSVtoRGB(float h, float s, float v)
{
    if (s <= 0.0f)
        return ImVec4(v, v, v, 1.0f);
    h = std::fmodf(h, 1.0f) * 6.0f; // [0,6)
    int i = (int)h;
    float f = h - (float)i;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    float r = 0, g = 0, b = 0;
    switch (i)
    {
        case 0:
            r = v;
            g = t;
            b = p;
            break;
        case 1:
            r = q;
            g = v;
            b = p;
            break;
        case 2:
            r = p;
            g = v;
            b = t;
            break;
        case 3:
            r = p;
            g = q;
            b = v;
            break;
        case 4:
            r = t;
            g = p;
            b = v;
            break;
        default:
            r = v;
            g = p;
            b = q;
            break;
    }
    return ImVec4(r, g, b, 1.0f);
}

ImU32 ChatManager::getUsernameColor(const std::string& name) const
{
    // C++14-friendly: no init-statement in if
    auto it = nameColorCache_.find(name);
    if (it != nameColorCache_.end())
        return it->second;

    // hash -> hue
    std::hash<std::string> H;
    uint64_t hv = H(name) ^ 0x9E3779B97F4A7C15ull;
    float hue = (float)((hv % 10000) / 10000.0f); // [0,1)

    // avoid a narrow hue band if desired
    if (hue > 0.12f && hue < 0.18f)
        hue += 0.08f;
    if (hue >= 1.0f)
        hue -= 1.0f;

    const float sat = 0.65f;
    const float val = 0.90f;

    ImVec4 rgb = HSVtoRGB(hue, sat, val);
    ImU32 col = ImGui::GetColorU32(rgb);

    nameColorCache_.emplace(name, col);
    return col;
}

void ChatManager::renderColoredUsername(const std::string& name) const
{
    const ImU32 col = getUsernameColor(name);
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::TextUnformatted(name.c_str());
    ImGui::PopStyleColor();
}

void ChatManager::renderPlainMessage(const std::string& text) const
{
    ImGui::TextWrapped("%s", text.c_str());
}

void ChatManager::tryHandleSlashCommand(uint64_t threadId, const std::string& input)
{
    // /roll NdM(+K)  e.g., /roll 4d6+2  or /roll 1d20-5
    if (input.rfind("/roll", 0) != 0)
        return;

    auto nm = network_.lock();
    if (!nm)
        return;
    auto uname = nm->getMyUsername();
    if (uname.empty())
        uname = "me";

    auto parseInt = [](const std::string& s, size_t& i) -> int
    {
        int v = 0;
        bool any = false;
        while (i < s.size() && std::isdigit((unsigned char)s[i]))
        {
            any = true;
            v = v * 10 + (s[i] - '0');
            ++i;
        }
        return any ? v : -1;
    };

    size_t i = 5;
    while (i < input.size() && std::isspace((unsigned char)input[i]))
        ++i;

    int N = parseInt(input, i);
    if (i >= input.size() || input[i] != 'd' || N <= 0)
    { /*invalid*/
        return;
    }
    ++i;
    int M = parseInt(input, i);
    if (M <= 0)
        return;

    int K = 0;
    if (i < input.size() && (input[i] == '+' || input[i] == '-'))
    {
        bool neg = (input[i] == '-');
        ++i;
        int Z = parseInt(input, i);
        if (Z > 0)
            K = neg ? -Z : Z;
    }

    std::mt19937 rng((uint32_t)std::random_device{}());
    std::uniform_int_distribution<int> dist(1, std::max(1, M));

    int total = 0;
    std::string rolls;
    for (int r = 0; r < N; ++r)
    {
        int die = dist(rng);
        total += die;
        if (!rolls.empty())
            rolls += ", ";
        rolls += std::to_string(die);
    }

    // This command outputs as a chat message from "me"
    const int modApplied = K;
    const int finalTotal = total + modApplied;

    const std::string text = "rolled " + std::to_string(N) + "d" + std::to_string(M) +
                             (diceModPerDie_ ? " (per-die mod " : " (mod ") +
                             std::to_string(modApplied) + ")" +
                             " => [" + rolls + "] = " + std::to_string(finalTotal);

    const uint64_t ts = (uint64_t)nowSec();
    emitChatMessageFrame(threadId, uname, text, ts);
    pushMessageLocal(threadId, "me", uname, text, (double)ts, /*incoming*/ false);
}

void ChatManager::renderLeftPanel(float /*width*/)
{
    // Top actions
    if (ImGui::Button("New Group"))
        openCreatePopup_ = true;
    ImGui::SameLine();
    if (ImGui::Button("Delete Group"))
        openDeletePopup_ = true;

    // Enable edit only when there is a non-General active group
    bool canEdit = (activeGroupId_ != 0 && activeGroupId_ != generalGroupId_);
    ImGui::BeginDisabled(!canEdit);
    if (ImGui::Button("Edit Group"))
    {
        // seed edit state from the active group
        if (auto* g = getGroup(activeGroupId_))
        {
            editGroupId_ = g->id;
            editGroupSel_ = g->participants;
            std::fill(editGroupName_.begin(), editGroupName_.end(), '\0');
            std::snprintf(editGroupName_.data(), (int)editGroupName_.size(), "%s", g->name.c_str());
            openEditPopup_ = true;
        }
    }

    ImGui::EndDisabled();

    ImGui::Separator();
    ImGui::TextUnformatted("Chat Groups");
    ImGui::Separator();

    // Sorted by name; General at top
    std::vector<uint64_t> order;
    order.reserve(groups_.size());
    for (auto& [id, _] : groups_)
        order.push_back(id);
    std::sort(order.begin(), order.end(), [&](uint64_t a, uint64_t b)
              {
        if (a == generalGroupId_) return true;
        if (b == generalGroupId_) return false;
        return groups_[a].name < groups_[b].name; });

    for (auto id : order)
    {
        auto& g = groups_[id];
        const bool selected = (activeGroupId_ == id);

        ImGui::PushID((int)id);
        if (ImGui::Selectable(g.name.c_str(), selected, ImGuiSelectableFlags_SpanAllColumns, ImVec2(0, 0)))
        {
            activeGroupId_ = id;
            markGroupRead(id);
            focusInput_ = true;
            followScroll_ = true;
        }

        if (g.unread > 0)
        {
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImVec2 size(max.x - min.x, max.y - min.y);

            char buf[16];
            if (g.unread < 100)
                std::snprintf(buf, sizeof(buf), "%u", g.unread);
            else
                std::snprintf(buf, sizeof(buf), "99+");

            ImVec2 txtSize = ImGui::CalcTextSize(buf);
            const float padX = 6.f, padY = 3.f;
            const float radius = (txtSize.y + padY * 2.f) * 0.5f;
            const float marginRight = 8.f;
            ImVec2 center(max.x - marginRight - radius, min.y + size.y * 0.5f);

            auto* dl = ImGui::GetWindowDrawList();
            ImU32 bg = ImGui::GetColorU32(ImVec4(0.10f, 0.70f, 0.30f, 1.0f));
            ImU32 fg = ImGui::GetColorU32(ImVec4(1, 1, 1, 1));

            ImVec2 badgeMin(center.x - std::max(radius, txtSize.x * 0.5f + padX), center.y - radius);
            ImVec2 badgeMax(center.x + std::max(radius, txtSize.x * 0.5f + padX), center.y + radius);
            dl->AddRectFilled(badgeMin, badgeMax, bg, radius);
            ImVec2 textPos(center.x - txtSize.x * 0.5f, center.y - txtSize.y * 0.5f);
            dl->AddText(textPos, fg, buf);
        }
        ImGui::PopID();
    }

    if (openCreatePopup_)
    {
        openCreatePopup_ = false;
        ImGui::OpenPopup("CreateGroup");
    }
    renderCreateGroupPopup();

    if (openDeletePopup_)
    {
        openDeletePopup_ = false;
        ImGui::OpenPopup("DeleteGroup");
    }
    renderDeleteGroupPopup();

    if (openEditPopup_)
    {
        openEditPopup_ = false;
        ImGui::OpenPopup("EditGroup");
    }
    renderEditGroupPopup();
}

void ChatManager::renderRightPanel(float /*leftW*/)
{
    auto* g = getGroup(activeGroupId_);
    const float footerRowH = ImGui::GetFrameHeightWithSpacing() * 2.0f;

    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImGui::BeginChild("Messages", ImVec2(0, avail.y - footerRowH), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    if (g)
    {
        if (ImGui::GetScrollY() < ImGui::GetScrollMaxY() - 1.0f)
            followScroll_ = false;

        for (auto& m : g->messages)
        {
            renderColoredUsername(m.username);

            // separator
            ImGui::SameLine(0.0f, 6.0f);
            ImGui::TextUnformatted("-");
            ImGui::SameLine(0.0f, 6.0f);

            // message (wrapped). Put it in a child region so the wrap can use full width if you want,
            // but simplest is to just call TextWrapped now:
            // ensure we start from current cursor X (SameLine set it)
            ImGui::PushTextWrapPos(0);
            renderPlainMessage(m.content);
            ImGui::PopTextWrapPos();
        }
        if (followScroll_)
            ImGui::SetScrollHereY(1.0f);
        if (jumpToBottom_)
        {
            ImGui::SetScrollHereY(1.0f);
            jumpToBottom_ = false;
        }
    }
    ImGui::EndChild();

    // Footer input
    ImGui::BeginChild("Footer", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysAutoResize);

    if (focusInput_)
    {
        ImGui::SetKeyboardFocusHere();
        focusInput_ = false;
    }

    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AllowTabInput;
    if (ImGui::InputText("##chat_input", input_.data(), (int)input_.size(), flags))
    {
        std::string text(input_.data());
        if (!text.empty() && g)
        {
            const uint64_t ts = (uint64_t)nowSec();
            auto nm = network_.lock();
            auto uname = nm ? nm->getMyUsername() : std::string("me");
            if (uname.empty())
                uname = "me";

            if (!text.empty() && text[0] == '/')
            {
                tryHandleSlashCommand(activeGroupId_, text);
            }
            else
            {
                emitChatMessageFrame(g->id, uname, text, ts);
                pushMessageLocal(g->id, "me", uname, text, (double)ts, false);
            }

            input_.fill('\0');
            followScroll_ = true;
            focusInput_ = true;
            markGroupRead(g->id);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Send") && g)
    {
        std::string text(input_.data());
        if (!text.empty())
        {
            const uint64_t ts = (uint64_t)nowSec();
            auto nm = network_.lock();
            auto uname = nm ? nm->getMyUsername() : std::string("me");
            if (uname.empty())
                uname = "me";

            emitChatMessageFrame(g->id, uname, text, ts);
            pushMessageLocal(g->id, "me", uname, text, (double)ts, false);

            input_.fill('\0');
            followScroll_ = true;
            markGroupRead(g->id);
        }
    }

    ImGui::BeginDisabled(followScroll_);
    if (ImGui::Button("Go to bottom"))
    {
        jumpToBottom_ = true;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Roll Dice"))
    {
        openDicePopup_ = true;
    }
    if (openDicePopup_)
    {
        openDicePopup_ = false;
        ImGui::OpenPopup("DiceRoller");
    }
    renderDicePopup();

    ImGui::EndChild();
}
//
//void ChatManager::renderCreateGroupPopup()
//{
//    if (ImGui::BeginPopupModal("CreateGroup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
//    {
//        ImGui::TextUnformatted("Group name (unique):");
//        ImGui::InputText("##gname", newGroupName_.data(), (int)newGroupName_.size());
//
//        // list peers
//        ImGui::Separator();
//        ImGui::TextUnformatted("Participants:");
//        if (auto nm = network_.lock())
//        {
//            for (auto& [pid, link] : nm->getPeers())
//            {
//                bool checked = newGroupSel_.count(pid) > 0;
//                const std::string dn = nm->displayNameForPeer(pid);
//                std::string label = dn.empty() ? pid : (dn + " (" + pid + ")");
//                if (ImGui::Checkbox(label.c_str(), &checked))
//                {
//                    if (checked)
//                        newGroupSel_.insert(pid);
//                    else
//                        newGroupSel_.erase(pid);
//                }
//            }
//        }
//
//        // ensure name unique
//        bool nameTaken = false;
//        std::string desired = newGroupName_.data();
//        if (!desired.empty())
//        {
//            for (auto& [id, g] : groups_)
//                if (g.name == desired)
//                {
//                    nameTaken = true;
//                    break;
//                }
//        }
//
//        ImGui::Separator();
//        ImGui::BeginDisabled(desired.empty() || nameTaken);
//        if (ImGui::Button("Create"))
//        {
//            ChatGroupModel g;
//            g.name = desired;
//            g.id = makeGroupIdFromName(g.name);
//            g.participants = newGroupSel_;
//            if (identity_manager)
//                g.ownerUniqueId = identity_manager->myUniqueId();
//
//            groups_.emplace(g.id, g);
//            emitGroupCreate(g);
//
//            activeGroupId_ = g.id;
//            markGroupRead(g.id);
//            focusInput_ = true;
//            followScroll_ = true;
//
//            newGroupSel_.clear();
//            newGroupName_.fill('\0');
//            ImGui::CloseCurrentPopup();
//        }
//        ImGui::EndDisabled();
//
//        if (nameTaken)
//            ImGui::TextColored(ImVec4(1, 0.4f, 0.2f, 1), "A group with that name already exists.");
//
//        ImGui::SameLine();
//        if (ImGui::Button("Cancel"))
//        {
//            newGroupSel_.clear();
//            newGroupName_.fill('\0');
//            ImGui::CloseCurrentPopup();
//        }
//        ImGui::EndPopup();
//    }
//}

void ChatManager::renderCreateGroupPopup()
{
    if (ImGui::BeginPopupModal("CreateGroup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Group name (unique):");
        ImGui::InputText("##gname", newGroupName_.data(), (int)newGroupName_.size());

        // list peers
        ImGui::Separator();
        ImGui::TextUnformatted("Participants:");
        if (auto nm = network_.lock())
        {
            for (auto& [pid, link] : nm->getPeers())
            {
                // 🔸 map peerId -> uniqueId for selection storage
                std::string uid = identity_manager
                                      ? identity_manager->uniqueForPeer(pid).value_or(std::string{})
                                      : std::string{};

                if (uid.empty())
                    continue; // not bound yet; skip or show disabled if you prefer

                bool checked = newGroupSel_.count(uid) > 0; // 🔸 store UNIQUE IDs
                const std::string dn = nm->displayNameForPeer(pid);
                std::string label = dn.empty() ? pid : (dn + " (" + pid + ")");
                if (ImGui::Checkbox(label.c_str(), &checked))
                {
                    if (checked)
                        newGroupSel_.insert(uid); // 🔸 insert UID
                    else
                        newGroupSel_.erase(uid); // 🔸 erase UID
                }
            }
        }

        // ensure name unique
        bool nameTaken = false;
        std::string desired = newGroupName_.data();
        if (!desired.empty())
        {
            for (auto& [id, g] : groups_)
                if (g.name == desired)
                {
                    nameTaken = true;
                    break;
                }
        }

        ImGui::Separator();
        ImGui::BeginDisabled(desired.empty() || nameTaken);
        if (ImGui::Button("Create"))
        {
            ChatGroupModel g;
            g.name = desired;
            g.id = makeGroupIdFromName(g.name);
            g.participants = newGroupSel_; // 🔸 UNIQUE IDs

            if (identity_manager)
            {
                g.ownerUniqueId = identity_manager->myUniqueId();
                if (!g.ownerUniqueId.empty())
                    g.participants.insert(g.ownerUniqueId); // 🔸 owner always included (solo OK)
            }

            groups_.emplace(g.id, g);
            emitGroupCreate(g);

            activeGroupId_ = g.id;
            markGroupRead(g.id);
            focusInput_ = true;
            followScroll_ = true;

            newGroupSel_.clear();
            newGroupName_.fill('\0');
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        if (nameTaken)
            ImGui::TextColored(ImVec4(1, 0.4f, 0.2f, 1), "A group with that name already exists.");

        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            newGroupSel_.clear();
            newGroupName_.fill('\0');
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
//
//void ChatManager::renderEditGroupPopup()
//{
//    if (ImGui::BeginPopupModal("EditGroup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
//    {
//        ChatGroupModel* g = getGroup(editGroupId_);
//        if (!g)
//        {
//            ImGui::TextDisabled("Group not found.");
//            if (ImGui::Button("Close"))
//                ImGui::CloseCurrentPopup();
//            ImGui::EndPopup();
//            return;
//        }
//
//        // Optional: only owner can edit (comment out these 6 lines if you want anyone to edit)
//        auto nm = network_.lock();
//        const std::string meUid = identity_manager ? identity_manager->myUniqueId() : "";
//        const bool ownerOnly = true;
//        const bool canEdit = !ownerOnly || (!g->ownerUniqueId.empty() && g->ownerUniqueId == meUid);
//        if (!canEdit)
//            ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "Only the owner can edit this group.");
//
//        ImGui::Separator();
//        ImGui::TextUnformatted("Group name:");
//        ImGui::BeginDisabled(!canEdit);
//        ImGui::InputText("##edit_gname", editGroupName_.data(), (int)editGroupName_.size());
//        ImGui::EndDisabled();
//
//        // Participants list (pre-checked)
//        ImGui::Separator();
//        ImGui::TextUnformatted("Participants:");
//        if (nm)
//        {
//            ImGui::BeginDisabled(!canEdit);
//            for (auto& [pid, link] : nm->getPeers())
//            {
//                bool checked = editGroupSel_.count(pid) > 0;
//                const std::string dn = nm->displayNameForPeer(pid);
//                std::string label = dn.empty() ? pid : (dn + " (" + pid + ")");
//                if (ImGui::Checkbox(label.c_str(), &checked))
//                {
//                    if (checked)
//                        editGroupSel_.insert(pid);
//                    else
//                        editGroupSel_.erase(pid);
//                }
//            }
//            ImGui::EndDisabled();
//        }
//
//        // Validate (unique name unless unchanged)
//        bool nameTaken = false;
//        const std::string desired = editGroupName_.data();
//        if (!desired.empty() && desired != g->name)
//        {
//            for (auto& [id, gg] : groups_)
//                if (gg.name == desired)
//                {
//                    nameTaken = true;
//                    break;
//                }
//        }
//        if (nameTaken)
//            ImGui::TextColored(ImVec4(1, 0.4f, 0.2f, 1), "A group with that name already exists.");
//
//        ImGui::Separator();
//        ImGui::BeginDisabled(!canEdit || desired.empty() || nameTaken);
//        if (ImGui::Button("Update", ImVec2(120, 0)))
//        {
//            // Apply local changes
//            g->name = desired;
//            g->participants = editGroupSel_;
//
//            // Broadcast update
//            emitGroupUpdate(*g);
//
//            // nice UX
//            activeGroupId_ = g->id;
//            markGroupRead(g->id);
//            focusInput_ = true;
//            followScroll_ = true;
//
//            // keep popup open OR close—your call; let's close:
//            ImGui::CloseCurrentPopup();
//        }
//        ImGui::EndDisabled();
//
//        ImGui::SameLine();
//        if (ImGui::Button("Close", ImVec2(120, 0)))
//        {
//            ImGui::CloseCurrentPopup();
//        }
//
//        ImGui::EndPopup();
//    }
//}

void ChatManager::renderEditGroupPopup()
{
    if (ImGui::BeginPopupModal("EditGroup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ChatGroupModel* g = getGroup(editGroupId_);
        if (!g)
        {
            ImGui::TextDisabled("Group not found.");
            if (ImGui::Button("Close"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return;
        }

        // Optional: only owner can edit
        auto nm = network_.lock();
        const std::string meUid = identity_manager ? identity_manager->myUniqueId() : "";
        const bool ownerOnly = true;
        const bool canEdit = !ownerOnly || (!g->ownerUniqueId.empty() && g->ownerUniqueId == meUid);
        if (!canEdit)
            ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "Only the owner can edit this group.");

        ImGui::Separator();
        ImGui::TextUnformatted("Group name:");
        ImGui::BeginDisabled(!canEdit);
        ImGui::InputText("##edit_gname", editGroupName_.data(), (int)editGroupName_.size());
        ImGui::EndDisabled();

        // Participants list (pre-checked)
        ImGui::Separator();
        ImGui::TextUnformatted("Participants:");
        if (nm)
        {
            ImGui::BeginDisabled(!canEdit);
            for (auto& [pid, link] : nm->getPeers())
            {
                // 🔸 map peerId -> uniqueId for selection storage
                std::string uid = identity_manager
                                      ? identity_manager->uniqueForPeer(pid).value_or(std::string{})
                                      : std::string{};

                if (uid.empty())
                    continue;

                bool checked = editGroupSel_.count(uid) > 0; // 🔸 use UID
                const std::string dn = nm->displayNameForPeer(pid);
                std::string label = dn.empty() ? pid : (dn + " (" + pid + ")");
                if (ImGui::Checkbox(label.c_str(), &checked))
                {
                    if (checked)
                        editGroupSel_.insert(uid); // 🔸 insert UID
                    else
                        editGroupSel_.erase(uid); // 🔸 erase UID
                }
            }
            ImGui::EndDisabled();
        }

        // Validate (unique name unless unchanged)
        bool nameTaken = false;
        const std::string desired = editGroupName_.data();
        if (desired.empty() == false && desired != g->name)
        {
            for (auto& [id, gg] : groups_)
                if (gg.name == desired)
                {
                    nameTaken = true;
                    break;
                }
        }
        if (nameTaken)
            ImGui::TextColored(ImVec4(1, 0.4f, 0.2f, 1), "A group with that name already exists.");

        ImGui::Separator();
        ImGui::BeginDisabled(!canEdit || desired.empty() || nameTaken);
        if (ImGui::Button("Update", ImVec2(120, 0)))
        {
            // Apply local changes
            g->name = desired;
            g->participants = editGroupSel_; // 🔸 UNIQUE IDs

            // Broadcast update
            emitGroupUpdate(*g);

            // nice UX
            activeGroupId_ = g->id;
            markGroupRead(g->id);
            focusInput_ = true;
            followScroll_ = true;

            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Close", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}
//
//void ChatManager::renderDeleteGroupPopup()
//{
//    if (ImGui::BeginPopupModal("DeleteGroup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
//    {
//        ImGui::TextUnformatted("Click a group to delete (General cannot be deleted).");
//        ImGui::Separator();
//
//        for (auto it = groups_.begin(); it != groups_.end(); /*++ inside*/)
//        {
//            auto id = it->first;
//            auto& g = it->second;
//            if (id == generalGroupId_)
//            {
//                ++it;
//                continue;
//            }
//
//            ImGui::PushID((int)id);
//            const std::string meUid = identity_manager ? identity_manager->myUniqueId() : "";
//            const bool canDelete = (!g.ownerUniqueId.empty() && g.ownerUniqueId == meUid);
//            ImGui::BeginDisabled(!canDelete);
//            if (ImGui::Button(g.name.c_str(), ImVec2(220, 0)))
//            {
//                emitGroupDelete(id);
//                if (activeGroupId_ == id)
//                    activeGroupId_ = generalGroupId_;
//                it = groups_.erase(it);
//                ImGui::PopID();
//                continue;
//            }
//            ImGui::EndDisabled();
//            if (!canDelete)
//            {
//                ImGui::SameLine();
//                ImGui::TextDisabled("(owner only)");
//            }
//            ImGui::PopID();
//            ++it;
//        }
//
//        ImGui::Separator();
//        if (ImGui::Button("Close"))
//            ImGui::CloseCurrentPopup();
//        ImGui::EndPopup();
//    }
//}

void ChatManager::renderDeleteGroupPopup()
{
    if (ImGui::BeginPopupModal("DeleteGroup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Manage groups you participate in (General cannot be deleted/left).");
        ImGui::Separator();

        // Who am I and am I GM?
        std::string meUid = identity_manager ? identity_manager->myUniqueId() : "";
        bool iAmGM = false;
        if (auto nm = network_.lock())
        {
            // Use the accessor you actually have: getGMId() / gmId() / GMId() — pick the right one.
            // If your NetworkManager exposes a different name, change here only.
            const std::string gmUid = nm->getGMId(); // <-- adjust if needed in your codebase
            iAmGM = (!gmUid.empty() && !meUid.empty() && gmUid == meUid);
        }

        for (auto it = groups_.begin(); it != groups_.end(); /* ++ inside */)
        {
            const uint64_t id = it->first;
            ChatGroupModel& g = it->second;

            // Skip General
            if (id == generalGroupId_)
            {
                ++it;
                continue;
            }

            // Only show groups where I’m currently a participant (so I can Leave/Delete)
            const bool iParticipate = isMeParticipantOf(g);

            // Compute permissions
            const bool isOwner = (!g.ownerUniqueId.empty() && g.ownerUniqueId == meUid);
            const bool canDelete = isOwner;
            const bool canLeave = (iParticipate && !isOwner); // owners cannot leave by rule

            ImGui::PushID((int)id);

            // Row label
            ImGui::TextUnformatted(g.name.c_str());
            ImGui::SameLine();

            // DELETE button
            ImGui::BeginDisabled(!canDelete);
            if (ImGui::Button("Delete"))
            {
                // Emit delete to participants
                emitGroupDelete(id);

                // Remove locally
                if (activeGroupId_ == id)
                    activeGroupId_ = generalGroupId_;
                it = groups_.erase(it);

                ImGui::PopID();
                continue; // skip ++it
            }
            ImGui::EndDisabled();

            ImGui::SameLine();

            // LEAVE button
            ImGui::BeginDisabled(!canLeave);
            if (ImGui::Button("Leave"))
            {
                emitGroupLeave(id);

                // If I’m no longer a participant, optionally hide it locally right away
                if (!isMeParticipantOf(g))
                {
                    if (activeGroupId_ == id)
                        activeGroupId_ = generalGroupId_;
                    it = groups_.erase(it);
                    ImGui::PopID();
                    continue; // skip ++it
                }
            }
            ImGui::EndDisabled();

            // Helper hints
            if (!iParticipate)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(not a participant)");
            }
            else if (isOwner)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(owner)");
            }
            else if (iAmGM)
            {
                ImGui::SameLine();
                ImGui::TextDisabled("(GM)");
            }

            ImGui::PopID();
            ++it;
        }

        ImGui::Separator();
        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void ChatManager::renderDicePopup()
{
    if (ImGui::BeginPopup("DiceRoller"))
    {
        ImGui::TextUnformatted("Dice Roller");
        ImGui::Separator();

        ImGui::InputInt("Number", &diceN_);
        ImGui::InputInt("Sides", &diceSides_);
        ImGui::InputInt("Modifier", &diceMod_);
        ImGui::Checkbox("Apply modifier per die", &diceModPerDie_);

        if (ImGui::Button("d4"))
            diceSides_ = 4;
        ImGui::SameLine();
        if (ImGui::Button("d6"))
            diceSides_ = 6;
        ImGui::SameLine();
        if (ImGui::Button("d8"))
            diceSides_ = 8;
        ImGui::SameLine();
        if (ImGui::Button("d10"))
            diceSides_ = 10;
        ImGui::SameLine();
        if (ImGui::Button("d12"))
            diceSides_ = 12;
        ImGui::SameLine();
        if (ImGui::Button("d20"))
            diceSides_ = 20;

        ImGui::Separator();
        if (ImGui::Button("Roll"))
        {
            //// simple inline roll text:
            //int N = std::max(1, diceN_);
            //int M = std::max(1, diceSides_);
            //int K = diceModPerDie_ ? (diceMod_ * N) : diceMod_;
            //std::string text = "/roll " + std::to_string(N) + "d" + std::to_string(M) +
            //                   (K > 0 ? "+" + std::to_string(K) : (K < 0 ? std::string("-") + std::to_string(-K) : ""));
            //// For brevity, send as plain text:
            //auto nm = network_.lock();
            //std::string uname = nm ? nm->getMyUsername() : "me";
            //if (uname.empty())
            //    uname = "me";
            // build slash command
            int mod = diceMod_;
            int N = std::max(1, diceN_);
            int M = std::max(1, diceSides_);
            // If per-die modifier, convert into +K where K = modifier * N for the final text.
            int K = diceModPerDie_ ? (mod * N) : mod;
            std::string cmd = "/roll " + std::to_string(N) + "d" + std::to_string(M);
            if (K > 0)
                cmd += "+" + std::to_string(K);
            else if (K < 0)
                cmd += "-" + std::to_string(-K);
            tryHandleSlashCommand(activeGroupId_, cmd);

            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

void ChatManager::markGroupRead(uint64_t groupId)
{
    if (auto* g = getGroup(groupId))
        g->unread = 0;
}

//// ====== snapshot passthroughs (optional) ======
//void ChatManager::writeGroupsToSnapshotGT(std::vector<unsigned char>& buf) const
//{
//    // light snapshot (optional): just groups’ metadata; messages are local-only
//    Serializer::serializeInt(buf, 1); // version
//    Serializer::serializeInt(buf, (int)groups_.size());
//    for (auto& [id, g] : groups_)
//    {
//        Serializer::serializeUInt64(buf, g.id);
//        Serializer::serializeString(buf, g.name);
//        Serializer::serializeInt(buf, (int)g.participants.size());
//        for (auto& p : g.participants)
//            Serializer::serializeString(buf, p);
//    }
//}
//
//void ChatManager::readGroupsFromSnapshotGT(const std::vector<unsigned char>& buf, size_t& off)
//{
//    groups_.clear();
//    ensureGeneral();
//    (void)Serializer::deserializeInt(buf, off); // version
//    int n = Serializer::deserializeInt(buf, off);
//    for (int i = 0; i < n; ++i)
//    {
//        ChatGroupModel g;
//        g.id = Serializer::deserializeUInt64(buf, off);
//        g.name = Serializer::deserializeString(buf, off);
//        int pc = Serializer::deserializeInt(buf, off);
//        for (int k = 0; k < pc; ++k)
//            g.participants.insert(Serializer::deserializeString(buf, off));
//        groups_.emplace(g.id, std::move(g));
//    }
//    if (groups_.find(activeGroupId_) == groups_.end())
//        activeGroupId_ = generalGroupId_;
//}

//ALWAYS BROADCAST EVERYTHING
/*void ChatManager::emitGroupCreate(const ChatGroupModel& g)
{
    auto nm = network_.lock();
    if (!nm || !hasCurrent())
        return;

    auto j = msg::makeChatGroupCreate(currentTableId_, g.id, g.name, g.participants);
    Logger::instance().log("chat", Logger::Level::Info,
                           "SEND ChatGroupCreate id=" + std::to_string(g.id) + " name=" + g.name);
   // nm->broadcastChatJson(j); // broadcast groups list to everyone
    nm->sendChatJsonTo(g.participants, j);
}

void ChatManager::emitGroupUpdate(const ChatGroupModel& g)
{
    auto nm = network_.lock();
    if (!nm || !hasCurrent())
        return;

    auto j = msg::makeChatGroupUpdate(currentTableId_, g.id, g.name, g.participants);
   // nm->broadcastChatJson(j); // 
    nm->sendChatJsonTo(g.participants, j);
}

void ChatManager::emitGroupDelete(uint64_t groupId)
{
    auto nm = network_.lock();
    if (!nm || !hasCurrent())
        return;

    auto j = msg::makeChatGroupDelete(currentTableId_, groupId);
    nm->broadcastChatJson(j);
    nm->sendChatJsonTo(g.participants, j);
}

void ChatManager::emitChatMessageFrame(uint64_t groupId, const std::string& username, const std::string& text, uint64_t ts)
{
    auto nm = network_.lock();
    if (!nm || !hasCurrent())
        return;

    auto j = msg::makeChatMessage(currentTableId_, groupId, ts, username, text);
    // nm->broadcastChatJson(j); // 
    nm->sendChatJsonTo(g.participants, j);
}*/

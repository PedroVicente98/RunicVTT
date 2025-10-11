#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <set>
#include <unordered_map>
#include <filesystem>
#include <optional>
#include <memory>
#include <array>
#include "Message.h"

class NetworkManager; // fwd
class PeerLink;       // fwd

// ---- Chat model ----
struct ChatMessageModel
{
    enum class Kind
    {
        TEXT,
        IMAGE,
        LINK
    };
    Kind kind = Kind::TEXT;
    std::string senderId; // peer id or "me" (optional)
    std::string username; // display label (sent on wire in message frame)
    std::string content;  // text / url
    double ts = 0.0;      // seconds
};

struct ChatThreadModel
{
    uint64_t id = 0;                       // 1 reserved for General
    std::string displayName;               // "General (N)" or custom
    std::set<std::string> participants;    // empty means "General"/broadcast
    std::deque<ChatMessageModel> messages; // local history (not part of snapshot)

    // UX-only (not persisted/snapshotted)
    uint32_t unread = 0;
};

// Pure UI+logic+wire sync manager
class ChatManager : public std::enable_shared_from_this<ChatManager>
{
public:
    explicit ChatManager(std::weak_ptr<NetworkManager> nm);

    // Attach/replace network
    void setNetwork(std::weak_ptr<NetworkManager> nm);

    // Change active GameTable context (used by snapshot + save/load)
    void setActiveGameTable(uint64_t tableId, const std::string& gameTableName);
    bool hasCurrent() const;

    // Persistence (local file per table)
    bool saveCurrent();
    bool loadCurrent();

    void applyReady(const msg::ReadyMessage& m);

    void renameThread(uint64_t threadId, const std::string& newName);
    void setThreadParticipants(uint64_t threadId, std::set<std::string> parts);
    void deleteThread(uint64_t threadId);
    // UI render (call every frame in main render loop)
    void render();

    // External helpers
    void refreshGeneralParticipants(const std::unordered_map<std::string, std::shared_ptr<PeerLink>>& peers);
    uint64_t ensureThreadByParticipants(const std::set<std::string>& participants, const std::string& displayName);
    void sendTextToThread(uint64_t threadId, const std::string& text);
    void sendSnapshotTo(const std::string& peerId);

    void pushMessageLocal(uint64_t threadId, const std::string& fromId, const std::string& username, const std::string& text, double ts, bool incoming);

    static constexpr uint64_t generalThreadId_ = 1;
    uint64_t currentTableId_ = 0;
    std::string currentTableName_;

    ChatThreadModel* getThread(uint64_t id);

    // ---- inbound ----
    void onChatThreadCreateFrame(const std::vector<uint8_t>& b, size_t& off);
    void onChatThreadUpdateFrame(const std::vector<uint8_t>& b, size_t& off);
    void onChatThreadDeleteFrame(const std::vector<uint8_t>& b, size_t& off);
    void onIncomingChatFrame(const std::vector<uint8_t>& b, size_t& off);

private:
    // utils
    std::filesystem::path chatFilePathFor(uint64_t tableId, const std::string& name) const;
    void ensureGeneral();
    uint64_t findThreadByParticipants(const std::set<std::string>& parts) const;
    uint64_t makeDeterministicThreadId(const std::set<std::string>& parts);
    static ChatMessageModel::Kind classifyMessage(const std::string& s);
    static double nowSec();

    // emit frames
    void emitThreadCreate(const ChatThreadModel& th);
    void emitThreadUpdate(const ChatThreadModel& th);
    void emitThreadDelete(uint64_t threadId);
    void emitChatMessageFrame(uint64_t threadId, const std::string& username, const std::string& text, uint64_t ts);

    // UI sub-parts
    void renderLeftPanel(float width);
    void renderRightPanel(float leftPanelWidth);
    void renderCreateThreadPopup();
    void renderDeleteThreadPopup();
    void renderDicePopup();

    // UI helpers
    void markThreadRead(uint64_t threadId);

    void tryHandleSlashCommand(uint64_t threadId, const std::string& text);

    // wiring
    std::weak_ptr<NetworkManager> network_;

    // data
    std::unordered_map<uint64_t, ChatThreadModel> threads_;
    uint64_t activeThreadId_ = 0;

    // UI state
    std::array<char, 512> input_{};
    bool focusInput_ = false;
    bool followScroll_ = true; // auto-stick to bottom if true
    bool chatWindowFocused_ = false;

    // Popups
    bool openCreatePopup_ = false;
    bool openDeletePopup_ = false;
    bool openDicePopup_ = false;

    // Create popup state
    std::array<char, 128> newThreadName_{};
    std::set<std::string> newThreadSel_;

    // Dice popup state
    int diceN_ = 1;
    int diceSides_ = 20;
    int diceMod_ = 0;
    bool diceModPerDie_ = false;
};

/*
NEW USAGE:
--------------------------------------------------------------------------
if (gameTableManager->chatManager) {
    // Optionally, keep general participants in sync with current peers each frame:
    // gameTableManager->chatManager->refreshGeneralParticipants(networkManager->getPeers());
    gameTableManager->chatManager->render();
}
--------------------------------------------------------------------------
// NetworkManager.h
void setChatManager(std::weak_ptr<ChatManager> cm);

// NetworkManager.cpp
void NetworkManager::setChatManager(std::weak_ptr<ChatManager> cm) { chatManager_ = std::move(cm); }

// When a DC message arrives, switch by msg::DCType and call:
auto cm = chatManager_.lock();
if (cm) {
    size_t off = 0;
    switch (type) {
        case msg::DCType::ChatThreadCreate: cm->onChatThreadCreateFrame(payload, off); break;
        case msg::DCType::ChatThreadUpdate: cm->onChatThreadUpdateFrame(payload, off); break;
        case msg::DCType::ChatThreadDelete: cm->onChatThreadDeleteFrame(payload, off); break;
        case msg::DCType::ChatMessage:      cm->onIncomingChatFrame    (payload, off); break;
        default: break;
    }
}
--------------------------------------------------------------------------
// GameTableManager.h
class GameTableManager {
public:
    std::shared_ptr<ChatManager> chatManager;
    // ...
};

// GameTableManager.cpp (setup)
gameTableManager->chatManager = std::make_shared<ChatManager>(networkManager);

// When active table changes:
chatManager->setActiveGameTable(activeTableId, activeTableName);
--------------------------------------------------------------------------





*/

/*#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <set>
#include <unordered_map>
#include <filesystem>
#include <optional>
#include <memory>

class NetworkManager; // fwd
class PeerLink;       // fwd

// ---- Chat model ----
struct ChatMessageModel
{
    enum class Kind
    {
        TEXT,
        IMAGE,
        LINK
    };
    Kind kind = Kind::TEXT;
    std::string senderId;
    std::string username;
    std::string content;
    double ts = 0.0;
};

struct ChatThreadModel
{
    uint64_t id = 0;
    std::string displayName;
    std::set<std::string> participants;
    std::deque<ChatMessageModel> messages;
};

class ChatManager : public std::enable_shared_from_this<ChatManager>
{
public:
    explicit ChatManager(std::weak_ptr<NetworkManager> nm);

    // lifecycle / binding
    void setNetwork(std::weak_ptr<NetworkManager> nm);
    void setActiveGameTable(uint64_t tableId, const std::string& gameTableName);
    bool hasCurrent() const;

    // persistence
    bool saveCurrent();
    bool loadCurrent();

    // snapshot helpers (GameTable snapshot)
    void writeThreadsToSnapshotGT(std::vector<unsigned char>& buf) const;
    void readThreadsFromSnapshotGT(const std::vector<unsigned char>& buf, size_t& off);

    // inbound (binary DC frames)
    void onChatThreadCreateFrame(const std::vector<uint8_t>& b, size_t& off);
    void onChatThreadUpdateFrame(const std::vector<uint8_t>& b, size_t& off);
    void onChatThreadDeleteFrame(const std::vector<uint8_t>& b, size_t& off);
    void onIncomingChatFrame(const std::vector<uint8_t>& b, size_t& off);

    // UI/api helpers
    void refreshGeneralParticipants(const std::unordered_map<std::string, std::shared_ptr<PeerLink>>& peers);
    uint64_t ensureThreadByParticipants(const std::set<std::string>& participants,
                                        const std::string& displayName);
    void sendTextToThread(uint64_t threadId, const std::string& text);

private:
    // utils
    std::filesystem::path chatFilePathFor(uint64_t tableId, const std::string& name) const;
    void ensureGeneral();
    ChatThreadModel* getThread(uint64_t id);
    uint64_t findThreadByParticipants(const std::set<std::string>& parts) const;
    uint64_t makeDeterministicThreadId(const std::set<std::string>& parts);

    // emit frames
    void emitThreadCreate(const ChatThreadModel& th);
    void emitThreadUpdate(const ChatThreadModel& th);
    void emitThreadDelete(uint64_t threadId);
    void emitChatMessageFrame(uint64_t threadId, const std::string& username,
                              const std::string& text, uint64_t ts);

private:
    std::weak_ptr<NetworkManager> network_;
    uint64_t currentTableId_ = 0;
    std::string currentTableName_;

    std::unordered_map<uint64_t, ChatThreadModel> threads_;
    uint64_t activeThreadId_ = 0;
    static constexpr uint64_t generalThreadId_ = 1;
};
*/
/*#pragma once
#include "imgui.h"
#include <vector>
#include <deque>
#include <string>
#include <set>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <optional>
#include <ctime>
#include <algorithm>
#include <memory>
#include "NetworkManager.h"
#include "PeerLink.h"
#include "nlohmann/json.hpp"

 --------- DC schema ---------
namespace chatmsg {
    inline constexpr const char* Type     = "type";
    inline constexpr const char* Chat     = "chat";
    inline constexpr const char* From     = "from";
    inline constexpr const char* Username = "username";
    inline constexpr const char* To       = "to";       // array of peerIds (optional)
    inline constexpr const char* Text     = "text";
    inline constexpr const char* Ts       = "ts";
    inline constexpr const char* Channel  = "channel";  // chat-id key (stable ID)
}

 --------- Model ---------
struct ChatMessage {
    enum class Kind { TEXT, IMAGE, LINK };
    Kind        kind = Kind::TEXT;
    std::string senderId;
    std::string username;
    std::string content;      // text or URL
    ImTextureID texture = nullptr; // non-owning; renderer caches elsewhere
    double      ts = 0.0;
};

struct ChatThread {
    std::string id;                      // "all" or "peerA|peerB"
    std::string displayName;             // "General" or "A,B,C"
    std::set<std::string> participants;  // for non-general only; "all" is dynamic
    std::deque<ChatMessage> messages;
};

class ChatManager : public std::enable_shared_from_this<ChatManager> {
public:
    ChatManager(std::shared_ptr<NetworkManager> nm,
                flecs::world ecs,
                std::string gameTableName)
        : network_(std::move(nm)), ecs_(ecs), gameTableName_(std::move(gameTableName))
    {
        ensureGeneral();
    }

     ---- UI entry ----
    void render() {
         keep general thread targeting current peers (no duplicates)
        refreshGeneralParticipants();

        ImGui::Begin("Chat");
        ImGui::Columns(2, nullptr, true);

        renderLeftSidebar();
        ImGui::NextColumn();
        renderRightPanel();

        ImGui::Columns(1);
        ImGui::End();
    }

     ---- inbound DC -> chat ----
    void onIncomingChatJson(const std::string& jsonStr) {
        nlohmann::json j; try { j = nlohmann::json::parse(jsonStr); } catch (...) { return; }
        if (j.value(chatmsg::Type, "") != std::string(chatmsg::Chat)) return;

        const std::string from      = j.value(chatmsg::From, "");
        const std::string username  = j.value(chatmsg::Username, guestLabel(from));
        const std::string text      = j.value(chatmsg::Text, "");
        const double      ts        = j.value(chatmsg::Ts, nowSec());
        const std::string chan      = j.value(chatmsg::Channel, "all");

        auto th = getOrCreateThreadById(chan);
        ChatMessage m;
        m.kind = classifyMessage(text);
        m.senderId = from;
        m.username = username.empty() ? guestLabel(from) : username;
        m.content = text;
        m.ts = ts;
        th->messages.push_back(std::move(m));
    }

     ---- persistence ----
    bool saveToFile() {
        auto path = chatFilePath();
        std::vector<unsigned char> buf;

        Serializer::serializeString(buf, "RUNIC-CHAT");
        Serializer::serializeInt(buf, 2); // version 2 (threads as map of shared_ptr)

        Serializer::serializeInt(buf, (int)threads_.size());
        for (auto& kv : threads_) {
            const auto& th = *kv.second;
            Serializer::serializeString(buf, th.id);
            Serializer::serializeString(buf, th.displayName);

             For "all" we persist empty; it is dynamic on load.
            if (th.id == "all") {
                Serializer::serializeInt(buf, 0);
            } else {
                Serializer::serializeInt(buf, (int)th.participants.size());
                for (auto& pid : th.participants) Serializer::serializeString(buf, pid);
            }

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
            std::ofstream os(path, std::ios::binary);
            os.write((const char*)buf.data(), (std::streamsize)buf.size());
            return true;
        } catch (...) {
            return false;
        }
    }

    bool loadFromFile() {
        auto path = chatFilePath();
        if (!std::filesystem::exists(path)) {
            ensureGeneral();
            return false;
        }

        std::vector<unsigned char> buf;
        try {
            std::ifstream is(path, std::ios::binary);
            is.seekg(0, std::ios::end);
            const std::streamsize sz = is.tellg();
            is.seekg(0, std::ios::beg);
            buf.resize((size_t)sz);
            is.read((char*)buf.data(), sz);
        } catch (...) { ensureGeneral(); return false; }

        size_t off = 0;
        auto magic = Serializer::deserializeString(buf, off);
        if (magic != "RUNIC-CHAT") { ensureGeneral(); return false; }
        const int version = Serializer::deserializeInt(buf, off);
        (void)version;

        threads_.clear();

        const int tcount = Serializer::deserializeInt(buf, off);
        for (int i=0;i<tcount;++i) {
            auto th = std::make_shared<ChatThread>();
            th->id = Serializer::deserializeString(buf, off);
            th->displayName = Serializer::deserializeString(buf, off);

            const int pcount = Serializer::deserializeInt(buf, off);
            for (int p=0;p<pcount;++p) th->participants.insert(Serializer::deserializeString(buf, off));

            const int mcount = Serializer::deserializeInt(buf, off);
            for (int k=0;k<mcount;++k) {
                ChatMessage m;
                m.kind = (ChatMessage::Kind)Serializer::deserializeInt(buf, off);
                m.senderId = Serializer::deserializeString(buf, off);
                m.username = Serializer::deserializeString(buf, off);
                m.content = Serializer::deserializeString(buf, off);
                m.ts = (double)Serializer::deserializeUInt64(buf, off);
                th->messages.push_back(std::move(m));
            }

            threads_.emplace(th->id, std::move(th));
        }

        ensureGeneral();       // re-create if missing
        refreshGeneralParticipants(); // repopulate "all"
        if (!threads_.count(activeId_)) activeId_ = "all";
        return true;
    }

private:
    std::shared_ptr<NetworkManager> network_;
    flecs::world ecs_;
    std::string gameTableName_;

    std::unordered_map<std::string, std::shared_ptr<ChatThread>> threads_; // smart pointers only
    std::string activeId_ = "all";
    std::array<char, 512> input_{};
    bool focusInput_ = false;

     ---- helpers ----
    static double nowSec() { return (double)std::time(nullptr); }

    static ChatMessage::Kind classifyMessage(const std::string& s) {
        auto ends_with = [](const std::string& a, const char* suf){
            const size_t n = std::strlen(suf);
            return a.size() >= n &&
                std::equal(a.end()-n, a.end(), suf, [](char c1,char c2){
                    return (char)std::tolower((unsigned char)c1) == (char)std::tolower((unsigned char)c2);
                });
        };
        if (ends_with(s, ".png") || ends_with(s, ".jpg") || ends_with(s, ".jpeg")) return ChatMessage::Kind::IMAGE;
        if (s.rfind("http://",0)==0 || s.rfind("https://",0)==0) return ChatMessage::Kind::LINK;
        return ChatMessage::Kind::TEXT;
    }

    static ImVec4 colorForName(const std::string& name) {
        std::hash<std::string> H; uint64_t hv = H(name);
        float hue = float(hv % 360) / 360.f, sat=0.65f, val=0.95f;
        float r,g,b; int i=int(hue*6); float f=hue*6-i;
        float p=val*(1-sat), q=val*(1-f*sat), t=val*(1-(1-f)*sat);
        switch (i%6){case 0:r=val;g=t;b=p;break;case 1:r=q;g=val;b=p;break;case 2:r=p;g=val;b=t;break;
                      case 3:r=p;g=q;b=val;break;case 4:r=t;g=p;b=val;break;default:r=val;g=p;b=q;break;}
        return ImVec4(r,g,b,1);
    }

    std::string guestLabel(const std::string& id) const { return "guest_" + id; }

    static std::string makeThreadId(const std::set<std::string>& participants) {
        if (participants.empty()) return "all";
        std::string id; bool first=true;
        for (auto& p : participants) { if (!first) id.push_back('|'); id += p; first=false; }
        return id;
    }

    std::shared_ptr<ChatThread> getOrCreateThread(const std::set<std::string>& participants) {
        const std::string id = makeThreadId(participants);
        if (auto it = threads_.find(id); it != threads_.end()) return it->second;

        auto th = std::make_shared<ChatThread>();
        th->id = id;
        th->participants = participants;
        th->displayName = (id == "all") ? "General" : participantsDisplayName(participants);
        threads_.emplace(id, th);
        return th;
    }

    std::string participantsDisplayName(const std::set<std::string>& participants) const {
        std::string name; bool first = true;
        for (auto& pid : participants) {
            const std::string dn = network_ ? network_->displayNameFor(pid) : pid;
            if (!first) name += ", ";
            name += (dn.empty()? pid : dn);
            first = false;
        }
        return name;
    }

    std::shared_ptr<ChatThread> getOrCreateThreadById(const std::string& id) {
        if (auto it = threads_.find(id); it != threads_.end()) return it->second;
        if (id == "all") return getOrCreateThread({});
        std::set<std::string> s;
        size_t start=0;
        while (start <= id.size()) {
            size_t pos = id.find('|', start);
            const std::string part = id.substr(start, pos==std::string::npos? id.size()-start : pos-start);
            if (!part.empty()) s.insert(part);
            if (pos == std::string::npos) break;
            start = pos + 1;
        }
        return getOrCreateThread(s);
    }

    void ensureGeneral() {
        if (!threads_.count("all")) threads_.emplace("all", std::make_shared<ChatThread>(ChatThread{
            .id="all", .displayName="General", .participants={}, .messages={}
        }));
        activeId_ = "all";
    }

    void refreshGeneralParticipants() {
         General must always include ALL current peers
        auto it = threads_.find("all");
        if (it == threads_.end()) { ensureGeneral(); it = threads_.find("all"); }
        auto& th = *it->second;
        th.participants.clear(); // we don't use this to route for "all", but keep it reflective
        if (network_) {
            for (auto& kv : network_->getPeers()) th.participants.insert(kv.first);
        }
         Optional: display as "General (N)"
        th.displayName = "General (" + std::to_string(th.participants.size()) + ")";
    }

    std::shared_ptr<ChatThread> currentThread() {
        if (auto it = threads_.find(activeId_); it != threads_.end()) return it->second;
        return threads_.at("all");
    }

    void sendToThread(const std::shared_ptr<ChatThread>& th, const std::string& text) {
        if (!network_) return;

        nlohmann::json j;
        j[chatmsg::Type]     = chatmsg::Chat;
        j[chatmsg::From]     = ""; // optional; DC is direct; add if you track your id
        j[chatmsg::Username] = network_->getMyUsername().empty() ? "me" : network_->getMyUsername();
        j[chatmsg::Text]     = text;
        j[chatmsg::Ts]       = nowSec();
        j[chatmsg::Channel]  = th->id;

        const std::string payload = j.dump();

        if (th->id == "all") {
             broadcast to all peers (General always targets all)
            for (auto& [pid, link] : network_->getPeers()) {
                if (link) link->send(payload);
            }
        } else {
            for (auto& pid : th->participants) {
                auto& peers = network_->getPeers();
                auto it = peers.find(pid);
                if (it != peers.end() && it->second) it->second->send(payload);
            }
        }

         append locally
        ChatMessage m;
        m.kind = classifyMessage(text);
        m.senderId = "me";
        m.username = network_->getMyUsername().empty() ? "me" : network_->getMyUsername();
        m.content = text;
        m.ts = nowSec();
        th->messages.push_back(std::move(m));
    }

     ---- UI ----
    void renderLeftSidebar() {
        ImGui::TextUnformatted("Chats");
        ImGui::Separator();

        for (auto& kv : threads_) {
            const auto& th = *kv.second;
            const bool sel = (activeId_ == th.id);
            if (ImGui::Selectable(th.displayName.c_str(), sel)) {
                activeId_ = th.id;
                focusInput_ = true;
            }
        }

        ImGui::Separator();
        if (ImGui::Button("New Groupâ€¦")) ImGui::OpenPopup("NewGroupChat");
        if (ImGui::BeginPopupModal("NewGroupChat", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            static std::set<std::string> tempSel;

            ImGui::TextUnformatted("Select participants");
            ImGui::Separator();

            if (network_) {
                for (auto& [pid, link] : network_->getPeers()) {
                    const std::string dn = network_->displayNameFor(pid);
                    bool checked = tempSel.count(pid) > 0;
                    if (ImGui::Checkbox((dn.empty()? pid : dn).c_str(), &checked)) {
                        if (checked) tempSel.insert(pid);
                        else tempSel.erase(pid);
                    }
                }
            }

            ImGui::Separator();
            if (ImGui::Button("Create")) {
                if (!tempSel.empty()) {
                    auto th = getOrCreateThread(tempSel); // unique by set
                    activeId_ = th->id;
                    tempSel.clear();
                    ImGui::CloseCurrentPopup();
                    focusInput_ = true;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) { tempSel.clear(); ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }
    }

    void renderRightPanel() {
        auto th = currentThread();
        const std::string title = th ? th->displayName : "Chat";
        ImGui::TextUnformatted(title.c_str());
        ImGui::Separator();

        ImGui::BeginChild("ChatScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()*2), true, ImGuiWindowFlags_HorizontalScrollbar);
        if (th) {
            for (auto& m : th->messages) {
                const std::string uname = m.username.empty() ? guestLabel(m.senderId) : m.username;
                ImVec4 col = colorForName(uname);
                ImGui::TextColored(col, "%s", uname.c_str());
                ImGui::SameLine(); ImGui::TextDisabled(" - "); ImGui::SameLine();
                switch (m.kind) {
                    case ChatMessage::Kind::TEXT: ImGui::TextWrapped("%s", m.content.c_str()); break;
                    case ChatMessage::Kind::LINK:
                        if (ImGui::SmallButton(m.content.c_str())) {
                             TODO: open URL (ShellExecute)
                        }
                        break;
                    case ChatMessage::Kind::IMAGE:
                        ImGui::TextWrapped("%s", m.content.c_str()); // show URL; preview hook is yours
                        break;
                }
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        if (focusInput_) { ImGui::SetKeyboardFocusHere(); focusInput_ = false; }

        ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue |
                                    ImGuiInputTextFlags_AllowTabInput |
                                    ImGuiInputTextFlags_CallbackHistory;
        if (ImGui::InputText("##chat_input", input_.data(), (int)input_.size(), flags)) {
            onSubmit();
        }
        ImGui::SameLine();
        if (ImGui::Button("Send")) onSubmit();
    }

    void onSubmit() {
        std::string text(input_.data());
        if (!text.empty()) {
            auto th = currentThread();
            if (th) {
                if (!text.empty() && text[0] == '/') {
                    auto res = processCommand(text);
                    ChatMessage sys;
                    sys.kind = ChatMessage::Kind::TEXT;
                    sys.senderId = "system";
                    sys.username = "System";
                    sys.content = res;
                    sys.ts = nowSec();
                    th->messages.push_back(std::move(sys));
                } else {
                    sendToThread(th, text);
                }
            }
        }
        input_.fill('\0');
        focusInput_ = true;
    }

    static std::string processCommand(const std::string& input) {
         Minimal /roll NdM(+K)
        auto parseInt = [](const std::string& s, size_t& i)->int{
            int v=0; bool any=false;
            while (i<s.size() && std::isdigit((unsigned char)s[i])) { any=true; v=v*10+(s[i]-'0'); ++i; }
            return any? v : -1;
        };
        if (input.rfind("/roll",0)==0) {
            size_t i=5; while (i<input.size() && std::isspace((unsigned char)input[i])) ++i;
            int N=parseInt(input,i); if (i>=input.size()||input[i]!='d'||N<=0) return "Invalid /roll";
            ++i; int M=parseInt(input,i); int K=0;
            if (i<input.size() && (input[i]=='+'||input[i]=='-')) { bool neg=input[i]=='-'; ++i; int Z=parseInt(input,i); if (Z>0) K=neg? -Z : Z; }
            int total=0; std::string rolls;
            for (int r=0;r<N;++r) { int die = 1 + (std::rand() % std::max(1,M)); total+=die; if (r) rolls+=", "; rolls+=std::to_string(die); }
            total+=K;
            return "Roll: ["+rolls+"], mod "+std::to_string(K)+" => Total: "+std::to_string(total);
        }
        return "Unknown command";
    }

    std::filesystem::path chatFilePath() const { return std::filesystem::path(gameTableName_ + "_chat.runic"); }
};*/

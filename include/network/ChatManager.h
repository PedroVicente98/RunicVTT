#pragma once
#include "imgui.h"
#include <vector>
#include <deque>
#include <string>
#include <set>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <algorithm>
#include <functional>
#include <optional>
#include <cctype>
#include "NetworkManager.h"    // needs getPeers() & displayNameFor(id), getMyUsername()
#include "PeerLink.h"          // for send()
#include "nlohmann/json.hpp"

// Simple chat DC message schema (over DataChannel)
namespace chatmsg {
    static constexpr const char* Type     = "type";
    static constexpr const char* Chat     = "chat";
    static constexpr const char* From     = "from";
    static constexpr const char* Username = "username";
    static constexpr const char* To       = "to";       // array of peerIds
    static constexpr const char* Text     = "text";
    static constexpr const char* Ts       = "ts";
    static constexpr const char* Channel  = "channel";  // chat-id key (stable ID)
}

struct ChatMessage {
    enum Kind { TEXT, IMAGE, LINK } kind = TEXT;
    std::string senderId;
    std::string username;
    std::string content;   // text or URL
    ImTextureID tex = nullptr; // if IMAGE
    double ts = 0.0;       // seconds since epoch
};

struct ChatThread {
    // A thread is defined by its participant set (excluding "me"); General uses participants={}
    std::string id;                           // stable id: e.g., "all" or "peerA|peerB"
    std::string displayName;                  // e.g., "General" or "A,B,C"
    std::set<std::string> participants;       // sorted unique ids (NOT including me)
    std::deque<ChatMessage> messages;
};

class ChatManager {
public:
    ChatManager(std::shared_ptr<NetworkManager> nm, flecs::world ecs, const std::string& gameTableName)
        : network_(std::move(nm)), ecs_(ecs), gameTableName_(gameTableName)
    {
        ensureGeneral();
    }

    // ---- rendering entry ----
    void render() {
        ImGui::Begin("Chat");

        ImGui::Columns(2, nullptr, true);

        renderLeftSidebar();   // threads + create group
        ImGui::NextColumn();
        renderRightPanel();    // messages + composer

        ImGui::Columns(1);
        ImGui::End();
    }

    // ---- network integration ----
    // Call this from your DataChannel onMessage handler when receiving a chat JSON
    // (Detect by json["type"] == "chat")
    void onIncomingChatJson(const std::string& jsonStr) {
        nlohmann::json j;
        try { j = nlohmann::json::parse(jsonStr); } catch (...) { return; }
        if (j.value(chatmsg::Type, "") != std::string(chatmsg::Chat)) return;

        const std::string from      = j.value(chatmsg::From, "");
        const std::string username  = j.value(chatmsg::Username, guestLabel(from));
        const std::string text      = j.value(chatmsg::Text, "");
        const double      ts        = j.value(chatmsg::Ts, nowSec());
        const std::string chan      = j.value(chatmsg::Channel, "all");

        auto& thread = getOrCreateThreadById(chan);
        ChatMessage msg;
        msg.kind      = classifyMessage(text);
        msg.senderId  = from;
        msg.username  = username.empty() ? guestLabel(from) : username;
        msg.content   = text;
        msg.ts        = ts;
        thread.messages.push_back(std::move(msg));
    }

    // ---- save/load chat log ----
    // Stored as: magic, version, thread count, for each thread -> id, displayName, participants, msg list
    bool saveToFile() {
        auto path = chatFilePath();
        std::vector<unsigned char> buf;

        // header
        Serializer::serializeString(buf, "RUNIC-CHAT");
        Serializer::serializeInt(buf, 1); // version

        Serializer::serializeInt(buf, (int)threads_.size());
        for (auto& [id, th] : threads_) {
            Serializer::serializeString(buf, th.id);
            Serializer::serializeString(buf, th.displayName);

            // participants
            Serializer::serializeInt(buf, (int)th.participants.size());
            for (auto& pid : th.participants) {
                Serializer::serializeString(buf, pid);
            }

            // messages
            Serializer::serializeInt(buf, (int)th.messages.size());
            for (auto& m : th.messages) {
                Serializer::serializeInt(buf, (int)m.kind);
                Serializer::serializeString(buf, m.senderId);
                Serializer::serializeString(buf, m.username);
                Serializer::serializeString(buf, m.content);
                // no image binary for now, tex id not serialized
                Serializer::serializeUInt64(buf, (uint64_t)m.ts);
            }
        }

        try {
            std::ofstream os(path, std::ios::binary);
            os.write((const char*)buf.data(), (std::streamsize)buf.size());
            return true;
        } catch (...) { return false; }
    }

    bool loadFromFile() {
        auto path = chatFilePath();
        if (!std::filesystem::exists(path)) return false;

        std::vector<unsigned char> buf;
        try {
            std::ifstream is(path, std::ios::binary);
            is.seekg(0, std::ios::end);
            std::streamsize sz = is.tellg();
            is.seekg(0, std::ios::beg);
            buf.resize((size_t)sz);
            is.read((char*)buf.data(), sz);
        } catch (...) { return false; }

        size_t off = 0;
        const auto magic = Serializer::deserializeString(buf, off);
        if (magic != "RUNIC-CHAT") return false;
        const int version = Serializer::deserializeInt(buf, off);
        (void)version;

        threads_.clear();

        const int tcount = Serializer::deserializeInt(buf, off);
        for (int i = 0; i < tcount; ++i) {
            ChatThread th;
            th.id = Serializer::deserializeString(buf, off);
            th.displayName = Serializer::deserializeString(buf, off);

            const int pcount = Serializer::deserializeInt(buf, off);
            for (int p = 0; p < pcount; ++p) {
                th.participants.insert(Serializer::deserializeString(buf, off));
            }

            const int mcount = Serializer::deserializeInt(buf, off);
            for (int k = 0; k < mcount; ++k) {
                ChatMessage m;
                m.kind      = (ChatMessage::Kind)Serializer::deserializeInt(buf, off);
                m.senderId  = Serializer::deserializeString(buf, off);
                m.username  = Serializer::deserializeString(buf, off);
                m.content   = Serializer::deserializeString(buf, off);
                m.ts        = (double)Serializer::deserializeUInt64(buf, off);
                th.messages.push_back(std::move(m));
            }

            threads_.emplace(th.id, std::move(th));
        }

        ensureGeneral(); // always ensure "General" exists
        return true;
    }

private:
    std::shared_ptr<NetworkManager> network_;
    flecs::world ecs_;
    std::string gameTableName_;

    std::unordered_map<std::string, ChatThread> threads_; // id -> thread
    std::string activeId_ = "all";
    char input_[512] = "";
    bool focusInput_ = false;

    // ------- helpers -------
    static double nowSec() {
        return (double)std::time(nullptr);
    }

    static ChatMessage::Kind classifyMessage(const std::string& s) {
        // very simple: image if ends with common extensions, link if starts with http(s)
        auto ends_with = [](const std::string& a, const char* suf){
            const size_t n = std::strlen(suf);
            return a.size() >= n && std::equal(a.end()-n, a.end(), suf,
                [](char c1, char c2){ return std::tolower((unsigned char)c1) == std::tolower((unsigned char)c2); });
        };
        if (ends_with(s, ".png") || ends_with(s, ".jpg") || ends_with(s, ".jpeg")) return ChatMessage::IMAGE;
        if (s.rfind("http://", 0) == 0 || s.rfind("https://", 0) == 0) return ChatMessage::LINK;
        return ChatMessage::TEXT;
    }

    static ImVec4 colorForName(const std::string& name) {
        // deterministic color by hashing username into hue
        std::hash<std::string> H;
        uint64_t h = H(name);
        float hue = float(h % 360) / 360.0f; // [0,1)
        float sat = 0.65f;
        float val = 0.95f;
        // HSV->RGB quick conversion
        float r,g,b;
        int i = int(hue * 6.f);
        float f = hue * 6.f - i;
        float p = val * (1 - sat);
        float q = val * (1 - f * sat);
        float t = val * (1 - (1 - f) * sat);
        switch (i % 6) {
            case 0: r = val, g = t,   b = p;   break;
            case 1: r = q,   g = val, b = p;   break;
            case 2: r = p,   g = val, b = t;   break;
            case 3: r = p,   g = q,   b = val; break;
            case 4: r = t,   g = p,   b = val; break;
            case 5: r = val, g = p,   b = q;   break;
        }
        return ImVec4(r,g,b,1.f);
    }

    std::string guestLabel(const std::string& id) const {
        return "guest_" + id;
    }

    std::string makeThreadId(const std::set<std::string>& participants) const {
        if (participants.empty()) return "all";
        std::string id;
        bool first = true;
        for (auto& p : participants) {
            if (!first) id.push_back('|');
            id += p;
            first = false;
        }
        return id;
    }

    ChatThread& getOrCreateThread(const std::set<std::string>& participants) {
        const std::string id = makeThreadId(participants);
        auto it = threads_.find(id);
        if (it != threads_.end()) return it->second;

        ChatThread th;
        th.id = id;
        th.participants = participants;
        if (id == "all") th.displayName = "General";
        else {
            // names are participant display names
            std::string name;
            bool first = true;
            for (auto& pid : participants) {
                const std::string dn = network_ ? network_->displayNameFor(pid) : pid;
                if (!first) name += ", ";
                name += (dn.empty() ? pid : dn);
                first = false;
            }
            th.displayName = name;
        }
        auto [insIt, _] = threads_.emplace(id, std::move(th));
        return insIt->second;
    }

    ChatThread& getOrCreateThreadById(const std::string& id) {
        if (id == "all") return getOrCreateThread({});
        // reconstruct set from id
        std::set<std::string> s;
        size_t start = 0;
        while (start <= id.size()) {
            size_t pos = id.find('|', start);
            std::string part = id.substr(start, pos == std::string::npos ? id.size()-start : pos - start);
            if (!part.empty()) s.insert(part);
            if (pos == std::string::npos) break;
            start = pos + 1;
        }
        return getOrCreateThread(s);
    }

    void ensureGeneral() {
        getOrCreateThread({});
        activeId_ = "all";
    }

    // ---- sending (DC) ----
    void sendToThread(ChatThread& th, const std::string& text) {
        if (!network_) return;

        // Build JSON
        nlohmann::json j;
        const std::string myId   = ""; // server overwrites "from" in WS, but DC is direct: you can insert your own id if you track it.
        const std::string myUser = network_->getMyUsername();
        j[chatmsg::Type]     = chatmsg::Chat;
        j[chatmsg::From]     = myId; // optional; peers can show username anyway
        j[chatmsg::Username] = myUser.empty() ? "me" : myUser;
        j[chatmsg::Text]     = text;
        j[chatmsg::Ts]       = nowSec();
        j[chatmsg::Channel]  = th.id;

        const std::string payload = j.dump();

        if (th.id == "all") {
            // broadcast to all peers
            for (auto& [pid, link] : network_->getPeers()) {
                if (link) link->send(payload);
            }
        } else {
            // send to each participant in the group
            for (auto& pid : th.participants) {
                auto it = network_->getPeers().find(pid);
                if (it != network_->getPeers().end() && it->second) {
                    it->second->send(payload);
                }
            }
        }

        // Also append locally
        ChatMessage msg;
        msg.kind = classifyMessage(text);
        msg.senderId = "me";
        msg.username = myUser.empty() ? "me" : myUser;
        msg.content = text;
        msg.ts = nowSec();
        th.messages.push_back(std::move(msg));
    }

    // ---- UI pieces ----
    void renderLeftSidebar() {
        ImGui::TextUnformatted("Chats");
        ImGui::Separator();

        // thread list
        for (auto& [id, th] : threads_) {
            bool sel = (activeId_ == id);
            if (ImGui::Selectable(th.displayName.c_str(), sel)) {
                activeId_ = id;
                focusInput_ = true;
            }
        }

        ImGui::Separator();
        if (ImGui::Button("New Group…")) {
            ImGui::OpenPopup("NewGroupChat");
        }
        if (ImGui::BeginPopupModal("NewGroupChat", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            static std::set<std::string> tempSel;
            // Peer list
            ImGui::TextUnformatted("Select participants");
            ImGui::Separator();

            // List peers from NM
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
                    auto& th = getOrCreateThread(tempSel); // uniqueness guaranteed by set
                    activeId_ = th.id;
                    tempSel.clear();
                    ImGui::CloseCurrentPopup();
                    focusInput_ = true;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                tempSel.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void renderRightPanel() {
        // header
        ChatThread* th = currentThread();
        std::string title = th ? th->displayName : "Chat";
        ImGui::TextUnformatted(title.c_str());
        ImGui::Separator();

        // messages area
        ImGui::BeginChild("ChatScroll", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()*2), true, ImGuiWindowFlags_HorizontalScrollbar);
        if (th) {
            for (auto& m : th->messages) {
                ImVec4 userCol = colorForName(m.username.empty() ? guestLabel(m.senderId) : m.username);
                ImGui::TextColored(userCol, "%s", (m.username.empty() ? guestLabel(m.senderId).c_str() : m.username.c_str()));
                ImGui::SameLine();
                ImGui::TextDisabled(" - ");
                ImGui::SameLine();
                switch (m.kind) {
                    case ChatMessage::TEXT:
                        ImGui::TextWrapped("%s", m.content.c_str());
                        break;
                    case ChatMessage::LINK:
                        if (ImGui::SmallButton(m.content.c_str())) {
                            // open URL (platform specific if you want)
                        }
                        break;
                    case ChatMessage::IMAGE:
                        ImGui::TextWrapped("%s", m.content.c_str()); // keep URL as text for now
                        // If you already have a texture loader, you can show ImGui::Image
                        break;
                }
            }
            // autoscroll
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();

        // composer
        if (focusInput_) {
            ImGui::SetKeyboardFocusHere();
            focusInput_ = false;
        }
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackHistory;
        if (ImGui::InputText("##chat_input", input_, sizeof(input_), flags)) {
            onSubmit();
        }
        ImGui::SameLine();
        if (ImGui::Button("Send")) {
            onSubmit();
        }
    }

    void onSubmit() {
        std::string text(input_);
        if (!text.empty()) {
            if (auto th = currentThread()) {
                // command: /roll XdY(+Z)
                if (!text.empty() && text[0] == '/') {
                    const std::string result = processCommand(text);
                    // show a "System" message locally
                    ChatMessage sys;
                    sys.kind = ChatMessage::TEXT;
                    sys.senderId = "system";
                    sys.username = "System";
                    sys.content = result;
                    sys.ts = nowSec();
                    th->messages.push_back(std::move(sys));
                } else {
                    sendToThread(*th, text);
                }
            }
        }
        input_[0] = '\0';
        focusInput_ = true;
    }

    ChatThread* currentThread() {
        auto it = threads_.find(activeId_);
        if (it == threads_.end()) return nullptr;
        return &it->second;
    }

    // same logic as your old processCommand (trimmed a bit)
    static std::string processCommand(const std::string& input) {
        // Very simple: support "/roll NdM+K"
        // You can paste your regex logic here; I’ll keep a minimal version:
        auto parseInt = [](const std::string& s, size_t& i)->int{
            int v = 0; bool any=false;
            while (i < s.size() && std::isdigit((unsigned char)s[i])) { any=true; v = v*10 + (s[i]-'0'); ++i; }
            return any? v : -1;
        };
        if (input.rfind("/roll", 0) == 0) {
            size_t i = 5;
            while (i<input.size() && std::isspace((unsigned char)input[i])) ++i;
            int N = parseInt(input, i);
            if (i>=input.size() || input[i] != 'd' || N<=0) return "Invalid /roll syntax";
            ++i;
            int M = parseInt(input, i);
            int K = 0;
            if (i < input.size() && (input[i] == '+' || input[i] == '-')) {
                bool neg = input[i]=='-'; ++i;
                int Z = parseInt(input, i);
                if (Z>0) K = neg? -Z : Z;
            }
            int total=0;
            std::string rolls;
            for (int r=0;r<N;++r) {
                int die = 1 + (std::rand() % std::max(1,M));
                total += die;
                if (r) rolls += ", ";
                rolls += std::to_string(die);
            }
            total += K;
            return "Roll: [" + rolls + "], mod " + std::to_string(K) + " => Total: " + std::to_string(total);
        }
        return "Unknown command";
    }

    std::filesystem::path chatFilePath() const {
        // Put next to your GameTable, as requested:
        // {GameTableName}_chat.runic  (You can prepend your game tables dir if needed)
        return std::filesystem::path(gameTableName_ + "_chat.runic");
    }
};

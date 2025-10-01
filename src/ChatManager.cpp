#include "ChatManager.h"

// heavy includes live here, NOT in the header:
#include "NetworkManager.h"
#include "nlohmann/json.hpp"
#include "Serializer.h"

// ===== ctor =====
ChatManager::ChatManager(std::shared_ptr<NetworkManager> nm,
                         flecs::world ecs,
                         std::string gameTableName)
    : network_(std::move(nm)), ecs_(ecs), gameTableName_(std::move(gameTableName))
{
    ensureGeneral();
}

// ===== UI entry =====
void ChatManager::render() {
    refreshGeneralParticipants();

    ImGui::Begin("Chat");
    ImGui::Columns(2, nullptr, true);

    renderLeftSidebar();
    ImGui::NextColumn();
    renderRightPanel();

    ImGui::Columns(1);
    ImGui::End();
}

// ===== inbound (JSON over DC) =====
void ChatManager::onIncomingChatJson(const std::string& jsonStr) {
    nlohmann::json j; 
    try { j = nlohmann::json::parse(jsonStr); } catch (...) { return; }
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

// ===== persistence =====
bool ChatManager::saveToFile() {
    auto path = chatFilePath();
    std::vector<unsigned char> buf;

    Serializer::serializeString(buf, "RUNIC-CHAT");
    Serializer::serializeInt(buf, 2); // version

    Serializer::serializeInt(buf, (int)threads_.size());
    for (auto& kv : threads_) {
        const auto& th = *kv.second;
        Serializer::serializeString(buf, th.id);
        Serializer::serializeString(buf, th.displayName);

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

bool ChatManager::loadFromFile() {
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

    ensureGeneral();
    refreshGeneralParticipants();
    if (!threads_.count(activeId_)) activeId_ = "all";
    return true;
}

// ===== helpers =====
double ChatManager::nowSec() { return (double)std::time(nullptr); }

ChatMessage::Kind ChatManager::classifyMessage(const std::string& s) {
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

ImVec4 ChatManager::colorForName(const std::string& name) {
    std::hash<std::string> H; uint64_t hv = H(name);
    float hue = float(hv % 360) / 360.f, sat=0.65f, val=0.95f;
    float r,g,b; int i=int(hue*6); float f=hue*6-i;
    float p=val*(1-sat), q=val*(1-f*sat), t=val*(1-(1-f)*sat);
    switch (i%6){case 0:r=val;g=t;b=p;break;case 1:r=q;g=val;b=p;break;case 2:r=p;g=val;b=t;break;
                  case 3:r=p;g=q;b=val;break;case 4:r=t;g=p;b=val;break;default:r=val;g=p;b=q;break;}
    return ImVec4(r,g,b,1);
}

std::string ChatManager::guestLabel(const std::string& id) const { return "guest_" + id; }

std::string ChatManager::makeThreadId(const std::set<std::string>& participants) {
    if (participants.empty()) return "all";
    std::string id; bool first=true;
    for (auto& p : participants) { if (!first) id.push_back('|'); id += p; first=false; }
    return id;
}

std::shared_ptr<ChatThread> ChatManager::getOrCreateThread(const std::set<std::string>& participants) {
    const std::string id = makeThreadId(participants);
    if (auto it = threads_.find(id); it != threads_.end()) return it->second;

    auto th = std::make_shared<ChatThread>();
    th->id = id;
    th->participants = participants;
    th->displayName = (id == "all") ? "General" : participantsDisplayName(participants);
    threads_.emplace(id, th);
    return th;
}

std::string ChatManager::participantsDisplayName(const std::set<std::string>& participants) const {
    std::string name; bool first = true;
    auto nm = network_.lock();
    for (auto& pid : participants) {
        const std::string dn = nm ? nm->displayNameFor(pid) : pid;
        if (!first) name += ", ";
        name += (dn.empty()? pid : dn);
        first = false;
    }
    return name;
}

std::shared_ptr<ChatThread> ChatManager::getOrCreateThreadById(const std::string& id) {
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

void ChatManager::ensureGeneral() {
    if (!threads_.count("all")) threads_.emplace("all", std::make_shared<ChatThread>(ChatThread{
        .id="all", .displayName="General", .participants={}, .messages={}
    }));
    activeId_ = "all";
}

void ChatManager::refreshGeneralParticipants() {
    auto it = threads_.find("all");
    if (it == threads_.end()) { ensureGeneral(); it = threads_.find("all"); }
    auto& th = *it->second;
    th.participants.clear();
    if (auto nm = network_.lock()) {
        for (auto& kv : nm->getPeers()) th.participants.insert(kv.first);
    }
    th.displayName = "General (" + std::to_string(th.participants.size()) + ")";
}

std::shared_ptr<ChatThread> ChatManager::currentThread() {
    if (auto it = threads_.find(activeId_); it != threads_.end()) return it->second;
    return threads_.at("all");
}

void ChatManager::sendToThread(const std::shared_ptr<ChatThread>& th, const std::string& text) {
    auto nm = network_.lock();
    if (!nm) return;

    nlohmann::json j;
    j[chatmsg::Type]     = chatmsg::Chat;
    j[chatmsg::From]     = ""; // optional; you can fill your own id
    j[chatmsg::Username] = nm->getMyUsername().empty() ? "me" : nm->getMyUsername();
    j[chatmsg::Text]     = text;
    j[chatmsg::Ts]       = nowSec();
    j[chatmsg::Channel]  = th->id;

    const std::string payload = j.dump();

    if (th->id == "all") {
        for (auto& [pid, link] : nm->getPeers()) {
            if (link) link->send(payload);
        }
    } else {
        for (auto& pid : th->participants) {
            auto& peers = nm->getPeers();
            auto it = peers.find(pid);
            if (it != peers.end() && it->second) it->second->send(payload);
        }
    }

    ChatMessage m;
    m.kind = classifyMessage(text);
    m.senderId = "me";
    m.username = nm->getMyUsername().empty() ? "me" : nm->getMyUsername();
    m.content = text;
    m.ts = nowSec();
    th->messages.push_back(std::move(m));
}

// ===== UI subpanels =====
void ChatManager::renderLeftSidebar() {
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
    if (ImGui::Button("New Group…")) ImGui::OpenPopup("NewGroupChat");
    if (ImGui::BeginPopupModal("NewGroupChat", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static std::set<std::string> tempSel;

        ImGui::TextUnformatted("Select participants");
        ImGui::Separator();

        if (auto nm = network_.lock()) {
            for (auto& [pid, link] : nm->getPeers()) {
                const std::string dn = nm->displayNameFor(pid);
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

void ChatManager::renderRightPanel() {
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
                        // TODO: open URL (ShellExecute)
                    }
                    break;
                case ChatMessage::Kind::IMAGE:
                    ImGui::TextWrapped("%s", m.content.c_str()); // URL text; preview hook is yours
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

void ChatManager::onSubmit() {
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

std::string ChatManager::processCommand(const std::string& input) {
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

std::filesystem::path ChatManager::chatFilePath() const {
    return std::filesystem::path(gameTableName_ + "_chat.runic");
}

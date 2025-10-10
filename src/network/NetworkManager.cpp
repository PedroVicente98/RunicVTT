#include "NetworkManager.h"
#include "NetworkUtilities.h"
#include "SignalingServer.h"
#include "SignalingClient.h"
#include "BoardManager.h"
#include "GameTableManager.h"
#include "Message.h"
#include "UPnPManager.h"
#include "Serializer.h"
#include "DebugConsole.h"
#include "Logger.h"

NetworkManager::NetworkManager(flecs::world ecs) :
    ecs(ecs), peer_role(Role::NONE)
{
    getLocalIPAddress();
    getExternalIPAddress();
    rtc::InitLogger(rtc::LogLevel::Verbose);
    NetworkUtilities::setupTLS();
}

void NetworkManager::setup(std::weak_ptr<BoardManager> board_manager, std::weak_ptr<GameTableManager> gametable_manager)
{
    this->board_manager = board_manager;
    this->gametable_manager = gametable_manager;

    signalingClient = std::make_shared<SignalingClient>(weak_from_this());
    signalingServer = std::make_shared<SignalingServer>(weak_from_this());

    DebugConsole::setLocalTunnelHandlers(
        [this]() -> std::string
        {
            auto ip = NetworkUtilities::getLocalIPv4Address();
            Logger::instance().log("localtunnel", Logger::Level::Debug, "Starting with runic-" + ip + ":" + std::to_string(port));
            return NetworkUtilities::startLocalTunnel("runic-" + ip, port);
        },
        [this]()
        {
            NetworkUtilities::stopLocalTunnel();
            Logger::instance().log("localtunnel", Logger::Level::Info, "Stop requested");
        });
}

NetworkManager::~NetworkManager()
{
    closeServer();
    disconnectAllPeers();
}

void NetworkManager::startServer(ConnectionType mode, unsigned short port, bool tryUpnp)
{
    peer_role = Role::GAMEMASTER;

    // Ensure server exists (your setup() likely already does this)
    if (!signalingServer)
    {
        signalingServer = std::make_shared<SignalingServer>(shared_from_this());
    }
    if (!signalingClient)
    {
        signalingClient = std::make_shared<SignalingClient>(shared_from_this());
    }

    // Start WS server
    signalingServer->start(port);
    setPort(port);

    const std::string localIp = getLocalIPAddress();
    const std::string externalIp = getExternalIPAddress();

    switch (mode)
    {
        case ConnectionType::LOCALTUNNEL:
        {
            const auto ltUrl = NetworkUtilities::startLocalTunnel("runic-" + localIp, static_cast<int>(port));
            if (ltUrl.empty())
            {
                break;
            }
            signalingClient->connectUrl(ltUrl);

            break;
        }
        case ConnectionType::LOCAL:
        {
            signalingClient->connect("127.0.0.1", port);
            break;
        }
        case ConnectionType::EXTERNAL:
        {
            if (tryUpnp)
            {
                try
                {
                    if (!UPnPManager::addPortMapping(localIp, port, port, "TCP", "RunicVTT"))
                    {
                        bool open = true;
                        ShowPortForwardingHelpPopup(&open);
                    }
                }
                catch (...)
                {
                }
            }
            signalingClient->connect("127.0.0.1", port);
            break;
        }
    }
    pushStatusToast("Signalling Server Started!!", ImGuiToaster::Level::Good, 4);
}

// Keep your old method (back-compat). Default it to LocalTunnel behavior.
void NetworkManager::startServer(std::string internal_ip_address, unsigned short port)
{
    // Old default used LocalTunnel; keep that behavior
    startServer(ConnectionType::LOCALTUNNEL, port, /*tryUpnp=*/false);
}

void NetworkManager::closeServer()
{
    if (signalingServer.get() != nullptr)
    {
        peer_role = Role::NONE;
        signalingServer->stop();
    }
    NetworkUtilities::stopLocalTunnel();
}

bool NetworkManager::isConnected()
{
    for (auto& [peerId, link] : peers)
    {
        if (link && link->isConnected())
            return true;
    }
    return false;
}
bool NetworkManager::isHosting() const
{
    return signalingServer && signalingServer->isRunning(); // if you expose isRunning()
}
bool NetworkManager::hasAnyPeer() const
{
    return !peers.empty();
}
int NetworkManager::connectedPeerCount() const
{
    int n = 0;
    for (auto& [id, link] : peers)
        if (link && link->isConnected())
            ++n;
    return n;
}

bool NetworkManager::connectToPeer(const std::string& connectionString)
{
    std::string server;
    unsigned short port = 0;
    std::string password;
    parseConnectionString(connectionString, server, port, password);

    if (!password.empty())
        setNetworkPassword(password.c_str());
    peer_role = Role::PLAYER;

    if (hasUrlScheme(server))
    {
        return signalingClient->connectUrl(server); // NEW method (see below)
    }
    return signalingClient->connect(server, port); // your old method
}

//INFORMATION AND PARSE OPERATIONS
std::string NetworkManager::getLocalIPAddress()
{
    if (local_ip_address == "")
    {
        auto local_ip = NetworkUtilities::getLocalIPv4Address();
        local_ip_address = local_ip;
        return local_ip;
    }
    return local_ip_address;
}

std::string NetworkManager::getExternalIPAddress()
{
    if (external_ip_address == "")
    {
        //auto external_ip = UPnPManager::getExternalIPv4Address();
        std::string external_ip = NetworkUtilities::httpGet(L"loca.lt", L"/mytunnelpassword");
        external_ip_address = external_ip;
        return external_ip;
    }
    return external_ip_address;
}

std::string NetworkManager::getNetworkInfo(ConnectionType type)
{
    const auto port = getPort();
    const auto pwd = getNetworkPassword();

    if (type == ConnectionType::LOCAL)
    { // LAN (192.168.x.y)
        const auto ip = getLocalIPAddress();
        return "runic:" + ip + ":" + std::to_string(port) + "?" + pwd;
    }
    else if (type == ConnectionType::EXTERNAL)
    { // public IP
        const auto ip = getExternalIPAddress();
        return "runic:" + ip + ":" + std::to_string(port) + "?" + pwd;
    }
    else if (type == ConnectionType::LOCALTUNNEL)
    {
        const auto url = getLocalTunnelURL(); // e.g., https://sub.loca.lt
        return url + "?" + pwd;               // no port needed (LT handles it)
    }
    return {};
}

std::string NetworkManager::getLocalTunnelURL()
{
    return NetworkUtilities::getLocalTunnelUrl();
}

void NetworkManager::parseConnectionString(std::string connection_string, std::string& server, unsigned short& port, std::string& password)
{
    server.clear();
    password.clear();
    port = 0;

    const std::string prefix = "runic:";
    if (connection_string.rfind(prefix, 0) == 0)
        connection_string.erase(0, prefix.size());

    // split pass
    std::string left = connection_string;
    if (auto q = connection_string.find('?'); q != std::string::npos)
    {
        left = connection_string.substr(0, q);
        password = connection_string.substr(q + 1);
    }

    if (hasUrlScheme(left))
    { // URL path
        server = left;
        return;
    }

    // host[:port]
    if (auto colon = left.find(':'); colon != std::string::npos)
    {
        server = left.substr(0, colon);
        try
        {
            port = static_cast<unsigned short>(std::stoi(left.substr(colon + 1)));
        }
        catch (...)
        {
            port = 0;
        }
    }
    else
    {
        server = left; // no port
    }
}

void NetworkManager::setNetworkPassword(const char* password)
{
    strncpy(network_password, password, sizeof(network_password) - 1);
    network_password[sizeof(network_password) - 1] = '\0';
}

//SINGLE POPUP RENDER FOR PORT FOWARD HELPER (MOVE TO WIKI)
void NetworkManager::ShowPortForwardingHelpPopup(bool* p_open)
{
    if (*p_open)
    {
        ImGui::OpenPopup("Port Forwarding Failed");
    }

    if (ImGui::BeginPopupModal("Port Forwarding Failed", p_open, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Automatic port forwarding failed. This is likely because your router does not\n"
                    "support UPnP or it is disabled. You can still host by choosing one of the\n"
                    "following three options:");

        ImGui::Separator();

        if (ImGui::BeginTabBar("PortOptions"))
        {

            // Tab 1: Enable UPnP
            if (ImGui::BeginTabItem("Enable UPnP"))
            {
                ImGui::Text("This is the easiest solution and will allow automatic port forwarding to work.");
                ImGui::Spacing();
                ImGui::BulletText("Access your router's administration page. This is usually done by\n"
                                  "typing its IP address (e.g., 192.168.1.1) in your browser.");
                ImGui::BulletText("Log in with your router's credentials.");
                ImGui::BulletText("Look for a setting named 'UPnP' or 'Universal Plug and Play' and enable it.");
                ImGui::BulletText("Save the settings and restart your router if necessary.");
                ImGui::BulletText("Try hosting again. The application should now automatically forward the port.");
                ImGui::EndTabItem();
            }

            // Tab 2: Manual Port Forwarding
            if (ImGui::BeginTabItem("Manual Port Forwarding"))
            {
                ImGui::Text("This method always works but requires you to configure your router manually.");
                ImGui::Spacing();
                ImGui::BulletText("Find your local IP address. Open Command Prompt and type 'ipconfig' to find it\n"
                                  "(e.g., 192.168.1.100).");
                ImGui::BulletText("Access your router's administration page.");
                ImGui::BulletText("Look for a setting named 'Port Forwarding', 'Virtual Server', or 'NAT'.");
                ImGui::BulletText("Create a new rule with the following details:");
                ImGui::Indent();
                ImGui::BulletText("Protocol: TCP");
                ImGui::BulletText("Internal Port: [Your application's port, e.g., 8080]");
                ImGui::BulletText("External Port: [The same port]");
                ImGui::BulletText("Internal IP: [Your local IP from step 1]");
                ImGui::Unindent();
                ImGui::BulletText("Save the rule and try hosting again.");
                ImGui::EndTabItem();
            }

            // Tab 3: Use a VPN (Hamachi)
            if (ImGui::BeginTabItem("Use a VPN (Hamachi)"))
            {
                ImGui::Text("A VPN creates a virtual local network, bypassing the need for port forwarding.");
                ImGui::Spacing();
                ImGui::BulletText("Download and install a VPN client like Hamachi.");
                ImGui::BulletText("Create a new network within the VPN client.");
                ImGui::BulletText("Share the network name and password with your friends.");
                ImGui::BulletText("Instruct your friends to join your network.");
                ImGui::BulletText("On the host screen, you should be able to see your VPN IP address.\n"
                                  "Use this IP to host your session.");
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        if (ImGui::Button("Close"))
        {
            *p_open = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

//PEER DISCONNECT OPERATIONS------------------------------------------------------------------

// Optional: remove peers that are no longer usable (Closed/Failed or nullptr)
std::size_t NetworkManager::removeDisconnectedPeers()
{
    std::vector<std::string> toErase;
    std::vector<std::shared_ptr<PeerLink>> toClose;
    toErase.reserve(peers.size());
    toClose.reserve(peers.size());

    // 1) Decide who to remove; DO NOT mutate the map yet.
    for (auto const& [pid, link] : peers)
    {
        const bool shouldRemove = (!link) || link->isClosedOrFailed();
        if (shouldRemove)
        {
            toErase.push_back(pid);
            if (link)
                toClose.emplace_back(link);
        }
    }

    // 2) Erase all selected entries from the map (no destructors called yet for links we retained).
    for (auto const& pid : toErase)
        peers.erase(pid);

    // 3) Close outside the map to avoid re-entrancy/races.
    for (auto& link : toClose)
    {
        try
        {
            link->close(); // should be idempotent and NOT call back into NM
        }
        catch (const std::exception& e)
        {
            Logger::instance().log("main", Logger::Level::Error, std::string("[removeDisconnectedPeers] ") + e.what());
            //pushStatusToast("Peer close error", ImGuiToaster::Level::Error, 4.0f);
        }
        catch (...)
        {
            Logger::instance().log("main", Logger::Level::Error, "[removeDisconnectedPeers] unknown exception");
            //pushStatusToast("Peer close error (unknown)", ImGuiToaster::Level::Error, 4.0f);
        }
    }

    return toErase.size();
}

bool NetworkManager::removePeer(std::string peerId)
{
    // 1) Find, move the link out, erase entry first.
    std::shared_ptr<PeerLink> link;
    if (auto it = peers.find(peerId); it != peers.end())
    {
        link = std::move(it->second);
        peers.erase(it);
    }
    else
    {
        return false;
    }

    // 2) Close outside the map.
    if (link)
    {
        try
        {
            link->close(); // safe: detach callbacks first, idempotent
        }
        catch (const std::exception& e)
        {
            Logger::instance().log("main", Logger::Level::Error, std::string("[removePeer] ") + e.what());
            //pushStatusToast("Peer close error", ImGuiToaster::Level::Error, 4.0f);
        }
        catch (...)
        {
            Logger::instance().log("main", Logger::Level::Error, "[removePeer] unknown exception");
            //pushStatusToast("Peer close error (unknown)", ImGuiToaster::Level::Error, 4.0f);
        }
    }
    return true;
}

// PEER INSERT METHOD AND LOCAL CALLBACKS --------------------------------------------------------------------
std::shared_ptr<PeerLink> NetworkManager::ensurePeerLink(const std::string& peerId)
{
    if (auto it = peers.find(peerId); it != peers.end())
        return it->second;
    auto link = std::make_shared<PeerLink>(peerId, weak_from_this());
    peers.emplace(peerId, link);
    return link;
}

void NetworkManager::onPeerLocalDescription(const std::string& peerId, const rtc::Description& desc)
{
    // Build signaling message and send via SignalingClient
    nlohmann::json j;
    const std::string sdp = std::string(desc);
    if (desc.type() == rtc::Description::Type::Offer)
    {
        j = msg::makeOffer("", peerId, sdp, myUsername_);
    }
    else if (desc.type() == rtc::Description::Type::Answer)
    {
        j = msg::makeAnswer("", peerId, sdp, myUsername_);
    }
    else
    {
        return;
    }
    if (signalingClient)
        signalingClient->send(j.dump());
}

void NetworkManager::onPeerLocalCandidate(const std::string& peerId, const rtc::Candidate& cand)
{
    auto j = msg::makeCandidate("", peerId, cand.candidate());
    if (signalingClient)
        signalingClient->send(j.dump());
}

//NECESSARY NETWORK OPERATIONS -------------------------------------------------------------------------------------------
void NetworkManager::setMyIdentity(std::string myId, std::string username)
{
    if (myId.empty())
    {
        myUsername_ = std::move(username);
    }
    else
    {
        clientUsernames_[myId] = username;
        myUsername_ = std::move(username);
        myClientId_ = std::move(myId);
    }
}

void NetworkManager::upsertPeerIdentity(const std::string& id, const std::string& username)
{
    peerUsernames_[id] = username;
    // If PeerLink already exists, update its display name for logs/UI:
    if (auto it = peers.find(id); it != peers.end() && it->second)
    {
        it->second->setDisplayName(username);
    }
}

std::string NetworkManager::displayNameFor(const std::string& id) const
{
    if (auto it = peerUsernames_.find(id); it != peerUsernames_.end())
        return it->second;
    if (auto it = clientUsernames_.find(id); it != clientUsernames_.end())
        return it->second;
    return id; // fallback to raw id
}

bool NetworkManager::disconectFromPeers()
{
    // Move out to avoid mutation during destruction
    std::unordered_map<std::string, std::shared_ptr<PeerLink>> moved = std::move(peers);
    peers.clear();

    for (auto& [pid, link] : moved)
    {
        if (!link)
            continue;
        try
        {
            link->close();
        }
        catch (const std::exception& e)
        {
            Logger::instance().log("main", Logger::Level::Error, std::string("[disconnectFromPeers] ") + e.what());
            pushStatusToast("Peer disconnect error", ImGuiToaster::Level::Error, 5.0f);
        }
        catch (...)
        {
            Logger::instance().log("main", Logger::Level::Error, "[disconnectFromPeers] unknown");
            pushStatusToast("Peer disconnect error (unknown)", ImGuiToaster::Level::Error, 5.0f);
        }
    }
    moved.clear();

    if (signalingClient)
    {
        signalingClient->close();
    }

    peer_role = Role::NONE;
    return true;
}

bool NetworkManager::disconnectAllPeers()
{
    if (signalingServer)
    {
        signalingServer->broadcastShutdown();
    }

    std::unordered_map<std::string, std::shared_ptr<PeerLink>> moved = std::move(peers);
    peers.clear();

    for (auto& [pid, link] : moved)
    {
        if (!link)
            continue;
        try
        {
            link->close();
        }
        catch (const std::exception& e)
        {
            Logger::instance().log("main", Logger::Level::Error, std::string("[disconnectAllPeers] ") + e.what());
            pushStatusToast("Peer disconnect error", ImGuiToaster::Level::Error, 5.0f);
        }
        catch (...)
        {
            Logger::instance().log("main", Logger::Level::Error, "[disconnectAllPeers] unknown");
            pushStatusToast("Peer disconnect error (unknown)", ImGuiToaster::Level::Error, 5.0f);
        }
    }
    moved.clear();

    if (signalingServer)
    {
        signalingServer->disconnectAllClients();
        try
        {
            signalingServer->stop();
        }
        catch (...)
        {
        }
    }

    if (signalingClient)
    {
        signalingClient->close();
    }

    peer_role = Role::NONE;
    return true;
}

void NetworkManager::broadcastPeerDisconnect(const std::string& targetId)
{
    if (!signalingClient)
        return; // GM might be loopbacked as a client; else send via server
    auto j = msg::makePeerDisconnect(targetId, /*broadcast*/ true);
    signalingClient->send(j.dump());
}

std::vector<std::string> NetworkManager::getConnectedPeerIds() const
{
    std::vector<std::string> ids;
    ids.reserve(peers.size());
    for (auto& [pid, link] : peers)
    {
        if (link && link->isConnected())
        {
            ids.push_back(pid);
        }
    }
    return ids;
}

//-SEND AND BROADCAST CHAT FRAME-------------------------------------------------------------------------------------------------------
void NetworkManager::broadcastChatThreadFrame(msg::DCType t, const std::vector<uint8_t>& payload)
{
    //{check}--PLUG THIS METHOD AND ADAPT TO CHATMANAGER
    for (auto& [pid, link] : peers)
    {
        if (!link)
            continue;
        // build final buffer [u8 type][u32 size][bytes...] or your framing
        std::vector<uint8_t> frame;
        frame.push_back((uint8_t)t);
        Serializer::serializeInt(frame, (int)payload.size());
        frame.insert(frame.end(), payload.begin(), payload.end());
        link->sendOn(msg::dc::name::Chat, frame); // PeerLink::send(vector<uint8_t>) must exist
    }
}

void NetworkManager::sendChatThreadFrameTo(const std::set<std::string>& peers_, msg::DCType t, const std::vector<uint8_t>& payload)
{ //{check} --PLUG THIS METHOD AND ADAPT TO CHATMANAGER
    for (auto& pid : peers_)
    {
        auto it = peers.find(pid);
        if (it == peers.end() || !it->second)
            continue;
        std::vector<uint8_t> frame;
        frame.push_back((uint8_t)t);
        Serializer::serializeInt(frame, (int)payload.size());
        frame.insert(frame.end(), payload.begin(), payload.end());
        it->second->sendOn(msg::dc::name::Chat, frame);
    }
}

// ----------- GAME MESSAGE BROADCASTERS -------------------------------------------------------------------------------- -----------

void NetworkManager::broadcastGameTable(const flecs::entity& gameTable)
{ //{check}--USE THIS METHOD
    auto ids = getConnectedPeerIds();
    if (!ids.empty())
    {
        sendGameTable(gameTable, ids);
    }
}

void NetworkManager::broadcastBoard(const flecs::entity& board)
{ //{check}--USE THIS METHOD
    auto ids = getConnectedPeerIds();
    if (!ids.empty())
    {
        sendBoard(board, ids);
    }
}

void NetworkManager::broadcastMarker(uint64_t boardId, const flecs::entity& marker)
{ //{check}--USE THIS METHOD
    auto ids = getConnectedPeerIds();
    if (!ids.empty())
    {
        sendMarker(boardId, marker, ids);
    }
}

void NetworkManager::broadcastFog(uint64_t boardId, const flecs::entity& fog)
{ //{check}--USE THIS METHOD
    auto ids = getConnectedPeerIds();
    if (!ids.empty())
    {
        sendFog(boardId, fog, ids);
    }
}

// ---------- MESSAGE SENDERS --------------------------------------------------------------------------------
void NetworkManager::sendGameTable(const flecs::entity& gameTable, const std::vector<std::string>& toPeerIds)
{
    auto gtId = gameTable.get<Identifier>()->id;
    auto gt = *gameTable.get<GameTable>();
    auto frame = buildSnapshotGameTableFrame(gtId, gt.gameTableName);
    broadcastFrame(frame, toPeerIds);
}

void NetworkManager::sendBoard(const flecs::entity& board, const std::vector<std::string>& toPeerIds)
{
    // read board image (from TextureComponent.image_path)
    auto tex = board.get<TextureComponent>();
    std::vector<unsigned char> img = tex ? readFileBytes(tex->image_path) : std::vector<unsigned char>{};

    // 1) meta
    auto meta = buildSnapshotBoardFrame(board, static_cast<uint64_t>(img.size()));
    broadcastFrame(meta, toPeerIds);

    // 2) image chunks (ownerKind=0 for board)
    uint64_t bid = board.get<Identifier>()->id;
    uint64_t off = 0;
    while (off < img.size())
    {
        size_t chunk = std::min<size_t>(kChunk, img.size() - off);
        auto frame = buildImageChunkFrame(/*ownerKind=*/0, bid, off, img.data() + off, chunk);
        for (auto& pid : toPeerIds)
            sendGameTo(pid, frame);
        off += chunk;
    }

    // 3) commit
    auto commit = buildCommitBoardFrame(bid);
    broadcastFrame(commit, toPeerIds);
}

void NetworkManager::sendMarker(uint64_t boardId, const flecs::entity& marker, const std::vector<std::string>& toPeerIds)
{
    // read marker image (from TextureComponent.image_path)
    auto tex = marker.get<TextureComponent>();
    std::vector<unsigned char> img = tex ? readFileBytes(tex->image_path) : std::vector<unsigned char>{};

    // 1) meta
    auto meta = buildCreateMarkerFrame(boardId, marker, static_cast<uint64_t>(img.size()));
    broadcastFrame(meta, toPeerIds);

    // 2) image chunks (ownerKind=1 for marker)
    uint64_t mid = marker.get<Identifier>()->id;
    uint64_t off = 0;
    while (off < img.size())
    {
        size_t chunk = std::min<size_t>(kChunk, img.size() - off);
        auto frame = buildImageChunkFrame(/*ownerKind=*/1, mid, off, img.data() + off, chunk);
        broadcastFrame(frame, toPeerIds);
        off += chunk;
    }

    // 3) commit
    auto commit = buildCommitMarkerFrame(boardId, mid);
    broadcastFrame(commit, toPeerIds);
}

void NetworkManager::sendFog(uint64_t boardId, const flecs::entity& fog, const std::vector<std::string>& toPeerIds)
{
    auto frame = buildFogCreateFrame(boardId, fog);
    broadcastFrame(frame, toPeerIds);
}

//ON PEER RECEIVING MESSAGE-----------------------------------------------------------
//void NetworkManager::onDcGameBinary(const std::string& fromPeer, const std::vector<uint8_t>& b)
//{
//    size_t off = 0;
//    while (off < b.size())
//    {
//        // Need at least one byte for the type
//        if (!ensureRemaining(b, off, 1))
//        {
//            break;
//        }
//
//        auto type = static_cast<msg::DCType>(b[off]);
//        off += 1;
//
//        switch (type)
//        {
//                // -------- SNAPSHOT / BOARD ----------
//            case msg::DCType::Snapshot_Board:
//                handleBoardMeta(b, off);
//                break;
//
//                // -------- CREATE MARKER (META) ----------
//            case msg::DCType::MarkerCreate:
//                handleMarkerMeta(b, off);
//                break;
//
//                // -------- FOG CREATE (NO IMAGE) ----------
//            case msg::DCType::FogCreate:
//                handleFogCreate(b, off);
//                break;
//
//                // -------- IMAGE CHUNK (BOARD or MARKER) ----------
//            case msg::DCType::ImageChunk:
//                handleImageChunk(b, off);
//                break;
//
//                // -------- COMMIT BOARD ----------
//            case msg::DCType::CommitBoard:
//                handleCommitBoard(b, off);
//                break;
//
//                // -------- COMMIT MARKER ----------
//            case msg::DCType::CommitMarker:
//                handleCommitMarker(b, off);
//                break;
//
//                // -------- BOARD OPERATONS - AFTER ENTITIY CREATION ----------
//            case msg::DCType::MarkerUpdate:
//                //handlemarkermove(b, off);
//                break;
//
//            case msg::DCType::GridUpdate:
//                //handlegridupdate(b, off);
//                break;
//
//            case msg::DCType::FogUpdate:
//                //handleFogUpdate(b, off);
//                break;
//            default:
//                return;
//        }
//    }
//}

//---Message Received Handlers--------------------------------------------------------------------------------
// DCType::Snapshot_GameTable (100)
void NetworkManager::handleGameTableSnapshot(const std::vector<uint8_t>& b, size_t& off)
{
    msg::ReadyMessage m;
    m.kind = msg::DCType::Snapshot_GameTable;
    m.tableId = Serializer::deserializeUInt64(b, off);
    m.name = Serializer::deserializeString(b, off);
    inboundGame_.push(std::move(m));
}

// DCType::Snapshot_Board (101) -- NewBoard - (TODO)Check for Existing Board in entities
void NetworkManager::handleBoardMeta(const std::vector<uint8_t>& b, size_t& off)
{
    msg::BoardMeta bm;
    bm.boardId = Serializer::deserializeUInt64(b, off);
    bm.boardName = Serializer::deserializeString(b, off);
    bm.pan = Serializer::deserializePanning(b, off);
    bm.grid = Serializer::deserializeGrid(b, off);
    bm.size = Serializer::deserializeSize(b, off);
    uint64_t total = Serializer::deserializeUInt64(b, off); // 0 if no image

    auto& p = imagesRx_[bm.boardId];
    p.kind = msg::ImageOwnerKind::Board;
    p.id = bm.boardId;
    p.boardId = bm.boardId;
    p.boardMeta = bm;
    p.total = total;
    p.received = 0;
    p.buf.clear();
    if (total)
        p.buf.resize(static_cast<size_t>(total));
}

// DCType::CreateEntity (4)
void NetworkManager::handleMarkerMeta(const std::vector<uint8_t>& b, size_t& off)
{
    msg::MarkerMeta mm;
    mm.boardId = Serializer::deserializeUInt64(b, off);
    mm.markerId = Serializer::deserializeUInt64(b, off);
    mm.name = Serializer::deserializeString(b, off);
    mm.pos = Serializer::deserializePosition(b, off);
    mm.size = Serializer::deserializeSize(b, off);
    mm.vis = Serializer::deserializeVisibility(b, off);
    mm.mov = Serializer::deserializeMoving(b, off);
    uint64_t total = Serializer::deserializeUInt64(b, off); // 0 if no image

    auto& p = imagesRx_[mm.markerId];
    p.kind = msg::ImageOwnerKind::Marker;
    p.id = mm.markerId;
    p.boardId = mm.boardId;
    p.markerMeta = mm;
    p.total = total;
    p.received = 0;
    p.buf.clear();
    if (total)
        p.buf.resize(static_cast<size_t>(total));
}

// DCType::FogCreate (7)
void NetworkManager::handleFogCreate(const std::vector<uint8_t>& b, size_t& off)
{
    msg::ReadyMessage m;
    m.kind = msg::DCType::FogCreate;
    m.boardId = Serializer::deserializeUInt64(b, off);
    m.fogId = Serializer::deserializeUInt64(b, off);
    m.pos = Serializer::deserializePosition(b, off);
    m.size = Serializer::deserializeSize(b, off);
    m.vis = Serializer::deserializeVisibility(b, off);
    inboundGame_.push(std::move(m));
}

// DCType::Image
void NetworkManager::handleImageChunk(const std::vector<uint8_t>& b, size_t& off)
{
    auto kind = static_cast<msg::ImageOwnerKind>(b[off]);
    off += 1;
    uint64_t id = Serializer::deserializeUInt64(b, off);
    uint64_t off64 = Serializer::deserializeUInt64(b, off);
    int len = Serializer::deserializeInt(b, off);

    auto it = imagesRx_.find(id);
    if (it == imagesRx_.end())
        return; // unknown; drop

    auto& p = it->second;
    if (p.kind != kind || p.total == 0)
        return;
    // bounds check
    if (off64 + static_cast<uint64_t>(len) > p.total)
        return;

    std::memcpy(p.buf.data() + off64, b.data() + off, static_cast<size_t>(len));
    off += static_cast<size_t>(len);
    p.received += static_cast<uint64_t>(len);
}

// DCType::CommitBoard
void NetworkManager::handleCommitBoard(const std::vector<uint8_t>& b, size_t& off)
{
    uint64_t boardId = Serializer::deserializeUInt64(b, off);
    auto it = imagesRx_.find(boardId);
    if (it == imagesRx_.end())
        return;
    auto& p = it->second;
    if (p.kind != msg::ImageOwnerKind::Board)
        return;

    if (p.total == 0 || p.isComplete())
    {
        if (p.boardMeta)
        {
            msg::ReadyMessage m;
            m.kind = msg::DCType::CommitBoard;
            m.boardId = boardId;
            m.boardMeta = *p.boardMeta;
            m.bytes = std::move(p.buf); // image (may be empty)
            inboundGame_.push(std::move(m));
        }
        imagesRx_.erase(it);
    }
}

// DCType::CommitMarker
void NetworkManager::handleCommitMarker(const std::vector<uint8_t>& b, size_t& off)
{
    uint64_t boardId = Serializer::deserializeUInt64(b, off);
    uint64_t markerId = Serializer::deserializeUInt64(b, off);

    auto it = imagesRx_.find(markerId);
    if (it == imagesRx_.end())
        return;

    auto& p = it->second;
    if (p.kind != msg::ImageOwnerKind::Marker || p.boardId != boardId)
        return;

    // marker image may be optional (total==0)
    if (p.total == 0 || p.isComplete())
    {
        if (p.markerMeta)
        {
            msg::ReadyMessage m;
            m.kind = msg::DCType::CommitMarker;
            m.boardId = boardId;
            m.markerMeta = *p.markerMeta;
            m.bytes = std::move(p.buf); // image (may be empty)
            inboundGame_.push(std::move(m));
        }
        imagesRx_.erase(it);
    }
}

void NetworkManager::drainEvents()
{
    msg::NetEvent ev;
    while (events_.try_pop(ev))
    {
        if (ev.type == msg::NetEvent::Type::DcOpen)
        {
            auto it = peers.find(ev.peerId);
            if (it != peers.end() && it->second)
            {
                it->second->setOpen(ev.label, true);
            }
        }
        else if (ev.type == msg::NetEvent::Type::DcClosed)
        {
            auto it = peers.find(ev.peerId);
            if (it != peers.end() && it->second)
            {
                it->second->setOpen(ev.label, false);
                it->second->markBootstrapReset();
            }
        }
    }

    // If GM: check if any peer is now fully open → bootstrap once
    if (peer_role == Role::GAMEMASTER)
    {
        for (auto& [pid, link] : peers)
        {
            if (link && link->allRequiredOpen() && !link->bootstrapSent())
            {
                try
                {
                    bootstrapPeerIfReady(pid);
                }
                catch (const std::exception& e)
                {
                    const std::string msg = std::string("Error bootstrapping peer: ") + e.what();
                    Logger::instance().log("main", Logger::Level::Error, msg);
                    toaster_->Push(ImGuiToaster::Level::Error, msg, 5.0f);
                }
                catch (...)
                {
                    const char* msg = "Error bootstrapping peer: unknown exception";
                    Logger::instance().log("main", Logger::Level::Error, msg);
                    toaster_->Push(ImGuiToaster::Level::Error, msg, 5.0f);
                }
            }
        }
    }
}

void NetworkManager::drainInboundRaw(int maxPerTick)
{
    int processed = 0;
    msg::InboundRaw r;
    while (processed < maxPerTick && inboundRaw_.try_pop(r))
    {
        try
        {
            if (r.label == msg::dc::name::Game)
            {
                decodeRawGameBuffer(r.fromPeer, r.bytes);
            }
            else if (r.label == msg::dc::name::Chat)
            {
                decodeRawChatBuffer(r.fromPeer, r.bytes);
            }
            else if (r.label == msg::dc::name::Notes)
            {
                decodeRawNotesBuffer(r.fromPeer, r.bytes);
            }
        }
        catch (...)
        {
            // swallow & optionally push a toaster
        }
        ++processed;
    }
}

void NetworkManager::decodeRawGameBuffer(const std::string& fromPeer, const std::vector<uint8_t>& b)
{
    size_t off = 0;
    while (off < b.size())
    {
        if (!ensureRemaining(b, off, 1))
            break;

        auto type = static_cast<msg::DCType>(b[off]);
        off += 1;

        switch (type)
        {
            case msg::DCType::Snapshot_GameTable:
                handleGameTableSnapshot(b, off); // pushes ReadyMessage internally
                break;

            case msg::DCType::Snapshot_Board:
                handleBoardMeta(b, off); // fills imagesRx_
                break;

            case msg::DCType::MarkerCreate:
                handleMarkerMeta(b, off); // fills imagesRx_
                break;

            case msg::DCType::FogCreate:
                handleFogCreate(b, off); // pushes ReadyMessage
                break;

            case msg::DCType::ImageChunk:
                handleImageChunk(b, off); // fills buffers
                break;

            case msg::DCType::CommitBoard:
                handleCommitBoard(b, off); // pushes ReadyMessage
                break;

            case msg::DCType::CommitMarker:
                handleCommitMarker(b, off); // pushes ReadyMessage
                break;

            case msg::DCType::MarkerUpdate:
                // handleMarkerUpdate(b, off);     // later
                return; // or break; depends on your framing
            case msg::DCType::GridUpdate:
                // handleGridUpdate(b, off);       // later
                return;

            default:
                // unknown / out-of-sync: stop this buffer
                return;
        }
    }
}

void NetworkManager::decodeRawChatBuffer(const std::string& fromPeer, const std::vector<uint8_t>& b)
{
    size_t off = 0;
    while (off < b.size())
    {
        if (!ensureRemaining(b, off, 1))
            break;

        auto type = static_cast<msg::DCType>(b[off]);
        off += 1;

        switch (type)
        {
            case msg::DCType::ChatMessage:
                //handleGameTableSnapshot(b, off); // pushes ReadyMessage internally
                break;

            case msg::DCType::ChatThreadCreate:
                //handleBoardMeta(b, off); // fills imagesRx_
                break;

            case msg::DCType::ChatThreadDelete:
                //handleMarkerMeta(b, off); // fills imagesRx_
                break;

            case msg::DCType::ChatThreadUpdate:
                //handleFogCreate(b, off); // pushes ReadyMessage
                break;

            default:
                // unknown / out-of-sync: stop this buffer
                return;
        }
    }
}

void NetworkManager::decodeRawNotesBuffer(const std::string& fromPeer, const std::vector<uint8_t>& b)
{
    size_t off = 0;
    while (off < b.size())
    {
        if (!ensureRemaining(b, off, 1))
            break;

        auto type = static_cast<msg::DCType>(b[off]);
        off += 1;

        switch (type)
        {
            case msg::DCType::NoteCreate:
                //handleGameTableSnapshot(b, off); // pushes ReadyMessage internally
                break;

            case msg::DCType::NoteUpdate:
                //handleBoardMeta(b, off); // fills imagesRx_
                break;

            case msg::DCType::NoteDelete:
                //handleMarkerMeta(b, off); // fills imagesRx_
                break;
            default:
                // unknown / out-of-sync: stop this buffer
                return;
        }
    }
}
void NetworkManager::startRawDrainWorker()
{
    bool expected = false;
    if (!rawWorkerRunning_.compare_exchange_strong(expected, true))
        return; // already running

    rawWorkerStop_.store(false);
    rawWorker_ = std::thread([this]()
                             {
        for (;;) {
            if (rawWorkerStop_.load())
                break;

            msg::InboundRaw r;
            if (inboundRaw_.try_pop(r)) {
                try {
                    if (r.label == msg::dc::name::Game) {
                        decodeRawGameBuffer(r.fromPeer, r.bytes);
                    } else if (r.label == msg::dc::name::Chat) {
                        decodeRawChatBuffer(r.fromPeer, r.bytes);
                    } else if (r.label == msg::dc::name::Notes) {
                        decodeRawNotesBuffer(r.fromPeer, r.bytes);
                    }
                } catch (...) {
                    // swallow
                }
                continue;
            }

            // nothing in queue: sleep briefly to avoid busy spin
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        } });
}

void NetworkManager::stopRawDrainWorker()
{
    if (!rawWorkerRunning_.load())
        return;
    rawWorkerStop_.store(true);
    if (rawWorker_.joinable())
        rawWorker_.join();
    rawWorkerRunning_.store(false);
}

//-----ON PEER CONNECTED INITIAL BOOSTSTRAP----------------------------------------------------------------------------
//void NetworkManager::onPeerChannelOpen(const std::string& peerId, const std::string& label)
//{//--{check}USED WHEN PEERS CONNECTING, SEND THE SNAPSHOT TO IT
//    if (peer_role != Role::GAMEMASTER)
//        return;
//
//    auto it = peers.find(peerId);
//    if (it == peers.end() || !it->second)
//        return;
//    auto& link = it->second;
//
//    if (link->allRequiredOpen())
//    {
//        bootstrapPeerIfReady(peerId);
//    }
//}

void NetworkManager::bootstrapPeerIfReady(const std::string& peerId)
{
    auto gm = gametable_manager.lock();
    auto bm = board_manager.lock();
    if (!gm)
    {
        throw std::exception("[NetworkManager] GametableManager Expired!!");
    }
    if (!bm)
    {
        throw std::exception("[NetworkManager] BoardManager Expired!!");
    }

    auto it = peers.find(peerId);
    if (it == peers.end() || !it->second)
        return;
    auto& link = it->second;
    if (link->bootstrapSent())
        return; // one-shot per connection

    if (gm->active_game_table.is_valid() && gm->active_game_table.has<GameTable>())
    {
        sendGameTable(gm->active_game_table, {peerId});
    }

    if (bm->isBoardActive())
    {
        auto boardEnt = bm->getActiveBoard();
        if (boardEnt.is_valid() && boardEnt.has<Board>())
        {
            sendBoard(boardEnt, {peerId}); // this sends meta + image chunks + commit
        }

        boardEnt.children([&](flecs::entity child)
                          {
			if (child.has<MarkerComponent>()) {
				uint64_t bid = boardEnt.get<Identifier>()->id;
				sendMarker(bid, child, { peerId });
			}
			else if (child.has<FogOfWar>()) {
				uint64_t bid = boardEnt.get<Identifier>()->id;
				sendFog(bid, child, { peerId });
			} });
    }

    link->markBootstrapSent();
}

// ---------- GAME FRAME BUILDERS ----------
std::vector<unsigned char> NetworkManager::buildSnapshotGameTableFrame(uint64_t gameTableId, const std::string& name)
{
    std::vector<unsigned char> b;
    Serializer::serializeUInt8(b, static_cast<uint8_t>(msg::DCType::Snapshot_GameTable));
    Serializer::serializeUInt64(b, gameTableId);
    Serializer::serializeString(b, name);
    return b;
}

std::vector<unsigned char> NetworkManager::buildSnapshotBoardFrame(const flecs::entity& board, uint64_t imageBytesTotal)
{
    std::vector<unsigned char> b;
    Serializer::serializeUInt8(b, static_cast<uint8_t>(msg::DCType::Snapshot_Board));

    // Required components
    auto id = board.get<Identifier>()->id;
    auto bd = *board.get<Board>();
    auto pan = *board.get<Panning>();
    auto grid = *board.get<Grid>();
    auto size = *board.get<Size>();

    Serializer::serializeUInt64(b, id);
    Serializer::serializeString(b, bd.board_name);

    // Panning
    Serializer::serializeBool(b, pan.isPanning);

    // Grid
    Serializer::serializeVec2(b, grid.offset);
    Serializer::serializeFloat(b, grid.cell_size);
    Serializer::serializeBool(b, grid.is_hex);
    Serializer::serializeBool(b, grid.snap_to_grid);
    Serializer::serializeBool(b, grid.visible);
    Serializer::serializeFloat(b, grid.opacity);

    // Size
    Serializer::serializeFloat(b, size.width);
    Serializer::serializeFloat(b, size.height);

    // Image total size in bytes (u64)
    Serializer::serializeUInt64(b, imageBytesTotal);
    return b;
}

std::vector<unsigned char> NetworkManager::buildCreateMarkerFrame(uint64_t boardId, const flecs::entity& marker, uint64_t imageBytesTotal)
{
    std::vector<unsigned char> b;
    Serializer::serializeUInt8(b, static_cast<uint8_t>(msg::DCType::MarkerCreate));

    auto mid = marker.get<Identifier>()->id;
    auto pos = *marker.get<Position>();
    auto sz = *marker.get<Size>();
    auto vis = *marker.get<Visibility>();
    auto mov = *marker.get<Moving>();

    std::string name = "marker_" + std::to_string(mid);

    Serializer::serializeUInt64(b, boardId);
    Serializer::serializeUInt64(b, mid);
    Serializer::serializeString(b, name);

    Serializer::serializePosition(b, &pos);
    Serializer::serializeSize(b, &sz);
    Serializer::serializeVisibility(b, &vis);
    Serializer::serializeMoving(b, &mov);

    Serializer::serializeUInt64(b, imageBytesTotal);
    return b;
}

std::vector<unsigned char> NetworkManager::buildFogCreateFrame(uint64_t boardId, const flecs::entity& fog)
{
    std::vector<unsigned char> b;
    Serializer::serializeUInt8(b, static_cast<uint8_t>(msg::DCType::FogCreate));

    auto fid = fog.get<Identifier>()->id;
    auto pos = *fog.get<Position>();
    auto sz = *fog.get<Size>();
    auto vis = *fog.get<Visibility>();

    Serializer::serializeUInt64(b, boardId);
    Serializer::serializeUInt64(b, fid);
    Serializer::serializePosition(b, &pos);
    Serializer::serializeSize(b, &sz);
    Serializer::serializeVisibility(b, &vis);
    return b;
}

std::vector<unsigned char> NetworkManager::buildImageChunkFrame(uint8_t ownerKind, uint64_t id, uint64_t offset, const unsigned char* data, size_t len)
{
    std::vector<unsigned char> b;
    Serializer::serializeUInt8(b, static_cast<uint8_t>(msg::DCType::ImageChunk));
    Serializer::serializeUInt8(b, ownerKind);
    Serializer::serializeUInt64(b, id);
    Serializer::serializeUInt64(b, offset);
    // len as int32
    Serializer::serializeInt(b, static_cast<int>(len));
    b.insert(b.end(), data, data + len);
    return b;
}

std::vector<unsigned char> NetworkManager::buildCommitBoardFrame(uint64_t boardId)
{
    std::vector<unsigned char> b;
    Serializer::serializeUInt8(b, static_cast<uint8_t>(msg::DCType::CommitBoard));
    Serializer::serializeUInt64(b, boardId);
    return b;
}

std::vector<unsigned char> NetworkManager::buildCommitMarkerFrame(uint64_t boardId, uint64_t markerId)
{
    std::vector<unsigned char> b;
    Serializer::serializeUInt8(b, static_cast<uint8_t>(msg::DCType::CommitMarker));
    Serializer::serializeUInt64(b, boardId);
    Serializer::serializeUInt64(b, markerId);
    return b;
}

// ---------- (DEPRECATED) send primitives (DONT FIT CURRENT IMPLEMENTATION) ----------
//-{check}wrong logic below

bool NetworkManager::sendMarkerCreate(const std::string& to, uint64_t markerId, const std::vector<uint8_t>& img, const std::string& name)
{ //{check}-wrong logic
    if (img.empty())
        return false;

    // BEGIN: DCType::CreateEntity
    {
        std::vector<uint8_t> b;
        b.push_back(static_cast<uint8_t>(msg::DCType::MarkerCreate));
        Serializer::serializeUInt64(b, markerId);
        Serializer::serializeString(b, name);
        Serializer::serializeUInt64(b, static_cast<uint64_t>(img.size()));
        //queueMessage(OutboundMsg{ to, msg::dc::name::Game, std::move(b) });
    }

    // CHUNKS: DCType::Image
    uint64_t offset = 0;
    while (offset < img.size())
    {
        const int n = static_cast<int>(std::min<size_t>(kChunk, img.size() - offset));
        std::vector<uint8_t> b;
        b.push_back(static_cast<uint8_t>(msg::DCType::ImageChunk));
        Serializer::serializeUInt64(b, markerId);
        Serializer::serializeUInt64(b, offset);
        Serializer::serializeInt(b, n);
        b.insert(b.end(), img.data() + offset, img.data() + offset + n);
        //queueMessage(OutboundMsg{ to, msg::dc::name::Game, std::move(b) });
        offset += n;
    }

    // COMMIT: DCType::CommitMarker
    {
        std::vector<uint8_t> b;
        b.push_back(static_cast<uint8_t>(msg::DCType::CommitMarker));
        Serializer::serializeUInt64(b, markerId);
        //queueMessage(OutboundMsg{ to, msg::dc::name::Game, std::move(b) });
    }

    return true;
}

bool NetworkManager::sendBoardCreate(const std::string& to, uint64_t boardId, const std::vector<uint8_t>& img, const std::string& name)
{ //{check}-wrong logic
    if (img.empty())
        return false;

    // BEGIN: we use DCType::Snapshot_Board as â€œBoardCreateBeginâ€
    {
        std::vector<uint8_t> b;
        b.push_back(static_cast<uint8_t>(msg::DCType::Snapshot_Board));
        Serializer::serializeUInt64(b, boardId);
        Serializer::serializeString(b, name);
        Serializer::serializeUInt64(b, static_cast<uint64_t>(img.size()));
        //queueMessage(OutboundMsg{ to, msg::dc::name::Game, std::move(b) });
    }

    // CHUNKS: DCType::Image (same as marker)
    uint64_t offset = 0;
    while (offset < img.size())
    {
        const int n = static_cast<int>(std::min<size_t>(kChunk, img.size() - offset));
        std::vector<uint8_t> b;
        b.push_back(static_cast<uint8_t>(msg::DCType::ImageChunk));
        Serializer::serializeUInt64(b, boardId);
        Serializer::serializeUInt64(b, offset);
        Serializer::serializeInt(b, n);
        b.insert(b.end(), img.data() + offset, img.data() + offset + n);
        //queueMessage(OutboundMsg{ to, msg::dc::name::Game, std::move(b) });
        offset += n;
    }

    // COMMIT: DCType::CommitBoard
    {
        std::vector<uint8_t> b;
        b.push_back(static_cast<uint8_t>(msg::DCType::CommitBoard));
        Serializer::serializeUInt64(b, boardId);
        //queueMessage(OutboundMsg{ to, msg::dc::name::Game, std::move(b) });
    }

    return true;
}

void NetworkManager::sendGameTo(const std::string& peerId, const std::vector<unsigned char>& bytes)
{
    auto it = peers.find(peerId);
    if (it == peers.end() || !it->second)
        return;
    it->second->sendGame(bytes);
}

void NetworkManager::broadcastFrame(const std::vector<unsigned char>& frame, const std::vector<std::string>& toPeerIds)
{
    for (auto& pid : toPeerIds)
    {
        sendGameTo(pid, frame);
    }
}

/*bool NetworkManager::disconectFromPeers()
{
    // Close all peer links (donâ€™t let PeerLink::close() call back into NM to erase)
    for (auto& [pid, link] : peers)
    {
        if (link)
        {
            try
            {
                link->close();
            }
            catch (...)
            {
            }
        }
    }
    peers.clear();

    // Close signaling client (playerâ€™s WS)
    if (signalingClient)
    {
        signalingClient->close();
        signalingClient.reset();
    }

    // Weâ€™re no longer connected
    peer_role = Role::NONE;
    return true;
}

bool NetworkManager::disconnectAllPeers()
{
    // 1) Broadcast a shutdown message (optional, but nice UX)
    if (signalingServer)
    {
        signalingServer->broadcastShutdown(); // 1) tell everyone
    }

    // 2) Close all peer links locally
    for (auto& [pid, link] : peers)
    {
        if (link)
        {
            try
            {
                link->close();
            }
            catch (...)
            {
            }
        }
    }
    peers.clear();

    // 3) Disconnect all WS clients and stop the server
    if (signalingServer)
    {
        signalingServer->disconnectAllClients();
        try
        {
            signalingServer->stop();
        }
        catch (...)
        {
        }
        signalingServer.reset();
    }

    // 4) If GM also had a self client (loopback to its own server), close it too
    if (signalingClient)
    {
        signalingClient->close();
        signalingClient.reset();
    }

    peer_role = Role::NONE;
    return true;
}
*/

/*bool NetworkManager::removePeer(std::string peerId)
{
    auto it = peers.find(peerId);
    if (it == peers.end())
        return false;
    if (auto& link = it->second)
    {
        try
        {
            link->close(); // ensure pc/dc closed
        }
        catch (...)
        {
            // swallow â€” safe cleanup path
        }
    }
    peers.erase(it);
    return true;
}

// Optional: remove peers that are no longer usable (Closed/Failed or nullptr)
std::size_t NetworkManager::removeDisconnectedPeers()
{
    std::size_t removed = 0;
    for (auto it = peers.begin(); it != peers.end();)
    {
        const bool shouldRemove =
            !it->second ||
            it->second->isClosedOrFailed(); // helper on PeerLink below

        if (shouldRemove)
        {
            if (it->second)
            {
                try
                {
                    it->second->close();
                }
                catch (...)
                {
                }
            }
            it = peers.erase(it);
            ++removed;
        }
        else
        {
            ++it;
        }
    }
    return removed;
}
*/

//
//void NetworkManager::startServer(std::string internal_ip_address, unsigned short port)
//{
//	peer_role = Role::GAMEMASTER;
//
//	auto local_tunnel_url = NetworkUtilities::startLocalTunnel("runic-" + internal_ip_address, port);
//	//auto internalIP = UPnPManager::getLocalIPv4Address();
//	/*if (!UPnPManager::addPortMapping(internal_ip_address, port, port, "TCP", "libdatachannel Signaling Server")) {
//		bool open_port_foward_tip = true;
//		ShowPortForwardingHelpPopup(&open_port_foward_tip);
//	}*/
//	signalingServer->start(port);
//	auto status = signalingClient->connect(local_tunnel_url, port);
//}

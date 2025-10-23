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
#include <unordered_set>

NetworkManager::NetworkManager(flecs::world ecs, std::shared_ptr<IdentityManager> identity_manager) :
    ecs(ecs), identity_manager(identity_manager), peer_role(Role::NONE)
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
    DebugConsole::setIdentityLogger([this]() -> std::string
                                    { return debugIdentitySnapshot(); });
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
                        pushStatusToast("Signalling Server UPnP Failled - Check Moldem configuration!!", ImGuiToaster::Level::Error, 4);
                    }
                }
                catch (...)
                {
                    pushStatusToast("Signalling Server UPnP Failled - Check Moldem configuration!!", ImGuiToaster::Level::Error, 4);
                }
            }
            signalingClient->connect("127.0.0.1", port);
            break;
        }
    }
    pushStatusToast("Signalling Server Started!!", ImGuiToaster::Level::Good, 4);
    //startRawDrainWorker();
}

//RECONNECT
//bool NetworkManager::reconnectPeer(const std::string& peerId)
//{
//    // 0) Check we know this peer
//    auto it = peers.find(peerId);
//    if (it == peers.end())
//        return false;
//
//    Logger::instance().log("main", Logger::Level::Warn, "[NM] Reconnect requested for peer " + peerId);
//
//    // 1) Quiesce & close the old link (don’t let callbacks call back into NM anymore)
//    if (it->second)
//    {
//        try
//        {
//            //it->second->quiesce();   // (no-op if you don’t have it; just stops callbacks)
//            it->second->close(); // your safe-close (swallow exceptions inside)
//        }
//        catch (...)
//        {
//            // swallow
//        }
//    }
//
//    // 2) Replace with a fresh link object (same peerId)
//    auto fresh = replaceWithFreshLink_(peerId);
//    if (!fresh)
//    {
//        Logger::instance().log("main", Logger::Level::Error, "[NM] Reconnect failed: could not create PeerLink");
//        return false;
//    }
//    peers[peerId] = fresh; //maybe use swap?
//
//    // 3) If you are the side that normally *offers* (e.g. GM), create DCs now then create & send offer
//    if (peer_role == Role::GAMEMASTER)
//    {
//        createChannelsIfOfferer_(fresh);    // create game/chat/notes DCs; attach handlers
//        createOfferAndSend_(peerId, fresh); // hook to your signaling send
//    }
//    else
//    {
//        // Player side: wait for offer from the server/GM.
//        // Make sure PeerConnection has onDataChannel callback attached inside PeerLink setup.
//        Logger::instance().log("main", Logger::Level::Info, "[NM] Waiting for remote offer from " + peerId);
//    }
//
//    return true;
//}

// --- helpers ---------------------------------------------------------

//std::shared_ptr<PeerLink> NetworkManager::replaceWithFreshLink_(const std::string& peerId)
//{
//    // Build a new PeerLink (this constructs a new rtc::PeerConnection and wires PC-level callbacks)
//    auto link = std::make_shared<PeerLink>(peerId, weak_from_this());
//
//    // If your PeerLink exposes a “pc” and “onDataChannel” wiring internally, you’re done.
//    // Otherwise ensure PeerLink sets pc->onDataChannel to call link->attachChannelHandlers().
//
//    return link;
//}
//
//void NetworkManager::createChannelsIfOfferer_(const std::shared_ptr<PeerLink>& link)
//{
//    // Offerer creates DCs *before* creating the SDP offer so they’re negotiated and will open automatically.
//    // You already have attachChannelHandlers on PeerLink – reuse it.
//
//    auto pc = link->getPeerConnection(); // whatever getter you have
//    if (!pc)
//        return;
//
//    // Create “game” channel
//    auto dcGame = pc->createDataChannel(msg::dc::name::Game);
//    link->attachChannelHandlers(dcGame, msg::dc::name::Game);
//
//    // Create “chat” channel
//    auto dcChat = pc->createDataChannel(msg::dc::name::Chat);
//    link->attachChannelHandlers(dcChat, msg::dc::name::Chat);
//
//    // Create “notes” channel (if you use it)
//    auto dcNotes = pc->createDataChannel(msg::dc::name::Notes);
//    link->attachChannelHandlers(dcNotes, msg::dc::name::Notes);
//}
//
//void NetworkManager::createOfferAndSend_(const std::string& peerId, const std::shared_ptr<PeerLink>& link)
//{
//    auto pc = link->getPeerConnection();
//    if (!pc)
//        return;
//
//    // Hook local description/candidates → send via your signaling client
//    //pc->onLocalDescription([this, peerId](const rtc::Description& desc)
//    //                       {
//    //                           Logger::instance().log("main", Logger::Level::Debug, "[NM] Sending offer to " + peerId);
//    //                           auto offer = msg::makeOffer(myClientId_, peerId, desc, myUsername_);
//    //                           if (signalingClient)
//    //                               signalingClient->send(offer.dump()); // adapt to your API
//    //                       });
//
//    //pc->onLocalCandidate([this, peerId](const rtc::Candidate& cand)
//    //                     { auto candidate = msg::makeCandidate(myClientId_,peerId,);
//    //                         if (signalingClient)
//    //                             signalingClient->send(peerId, cand);
//    //                     });
//
//    // Trigger offer creation
//    try
//    {
//        pc->setLocalDescription(); // libdatachannel: generates an offer and fires onLocalDescription
//    }
//    catch (const std::exception& e)
//    {
//        Logger::instance().log("main", Logger::Level::Error,
//                               std::string("[NM] setLocalDescription(offer) failed: ") + e.what());
//    }
//}

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
    //stopRawDrainWorker();
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
    else if (type == ConnectionType::CUSTOM)
    { // public IP
        const auto customHost = getCustomHost();
        return "runic:" + customHost  + ":" + std::to_string(port) + "?" + pwd;
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

// NetworkManager.cpp (relevant part)

void NetworkManager::onPeerLocalDescription(const std::string& peerId, const rtc::Description& desc)
{
    nlohmann::json j;
    const std::string sdp = std::string(desc);

    // Always pull identity from IdentityManager
    const std::string username = getMyUsername(); // forwards to IdentityManager
    const std::string uniqueId = getMyUniqueId(); // forwards to IdentityManager

    if (desc.type() == rtc::Description::Type::Offer)
    {
        // server will overwrite "from", so "" is fine
        j = msg::makeOffer(/*from*/ "", /*to*/ peerId, sdp, username, uniqueId);
    }
    else if (desc.type() == rtc::Description::Type::Answer)
    {
        j = msg::makeAnswer(/*from*/ "", /*to*/ peerId, sdp, username, uniqueId);
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
void NetworkManager::housekeepPeers()
{
    const uint64_t now = nowMs();
    const uint64_t kGraceMs = 10'000; // disconnect grace period

    static std::unordered_map<std::string, uint64_t> firstDiscPeerAt;
    static std::unordered_map<std::string, uint64_t> firstDiscWsAt;

    // ---- PeerLink cleanup ----
    for (auto it = peers.begin(); it != peers.end();)
    {
        const std::string& pid = it->first;
        auto& link = it->second;

        const bool connected = (link && link->isConnected());
        if (connected)
        {
            firstDiscPeerAt.erase(pid);
            ++it;
            continue;
        }

        uint64_t& t0 = firstDiscPeerAt[pid];
        if (t0 == 0)
            t0 = now;

        if (now - t0 >= kGraceMs)
        {
            try
            {
                if (link)
                    link->close();
            }
            catch (...)
            {
            }
            it = peers.erase(it);
            firstDiscPeerAt.erase(pid);
        }
        else
        {
            ++it;
        }
    }

    // ---- WebSocket cleanup (server side) ----
    auto wsClients = signalingServer->authClients();
    for (auto it = wsClients.begin(); it != wsClients.end();)
    {
        const std::string& cid = it->first;
        auto& ws = it->second;

        const bool open = (ws && ws->isOpen()); // if your API is different, change this check
        if (open)
        {
            firstDiscWsAt.erase(cid);
            ++it;
            continue;
        }

        uint64_t& t0 = firstDiscWsAt[cid];
        if (t0 == 0)
            t0 = now;

        if (now - t0 >= kGraceMs)
        {
            try
            {
                NetworkUtilities::safeCloseWebSocket(ws);
            }
            catch (...)
            {
            }
            it = wsClients.erase(it);
            firstDiscWsAt.erase(cid);
        }
        else
        {
            ++it;
        }
    }
}

void NetworkManager::upsertPeerIdentityWithUnique(const std::string& peerId,
                                                  const std::string& uniqueId,
                                                  const std::string& username)
{
    identity_manager->bindPeer(/*peerId=*/peerId,
                               /*uniqueId=*/uniqueId,
                               /*username=*/username);

    if (auto it = peers.find(peerId); it != peers.end() && it->second)
        it->second->setDisplayName(username);
}

std::string NetworkManager::displayNameForPeer(const std::string& peerId) const
{
    if (!identity_manager)
        return peerId;
    auto u = identity_manager->usernameForPeer(peerId);
    return u.value_or(peerId);
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

std::vector<std::string> NetworkManager::getConnectedUsernames() const
{
    std::vector<std::string> user_names;
    user_names.reserve(peers.size());
    for (auto& [pid, link] : peers)
    {
        if (link && link->isConnected())
        {
            user_names.push_back(displayNameForPeer(pid));
        }
    }
    return user_names;
}

// NetworkManager.cpp
std::string NetworkManager::debugIdentitySnapshot() const
{
    std::ostringstream os;
    os << "[Identity Snapshot]\n";

    os << "self.peerId=" << (getMyPeerId().empty() ? "(none)" : getMyPeerId()) << "\n";
    if (identity_manager)
    {
        os << "self.uniqueId=" << identity_manager->myUniqueId()
           << "  username=\"" << identity_manager->myUsername() << "\"\n";
    }
    // GM (if tracked)
    if (!getGMId().empty())
    {
        os << "gm.uniqueId=" << getGMId()
           << "  username=\"" << (identity_manager ? identity_manager->usernameForUnique(getGMId()) : std::string{}) << "\"\n";
    }

    os << "\n[Peers]\n";
    for (const auto& [peerId, link] : peers)
    {
        os << "- peerId=" << peerId;

        std::string uid, uname;
        if (identity_manager)
        {
            if (auto u = identity_manager->uniqueForPeer(peerId))
                uid = *u;
            if (!uid.empty())
                uname = identity_manager->usernameForUnique(uid);
        }

        if (!uid.empty())
            os << "  uniqueId=" << uid;
        if (!uname.empty())
            os << "  username=\"" << uname << "\"";

        if (link)
            os << "  link.display=\"" << link->displayName() << "\"";

        os << "\n";
    }

    // Optionally: dump address-book keys too
    if (identity_manager)
    {
        os << "\n[Peer→Unique map]\n";
        // If you don’t expose an accessor, add one that returns a copy or iterate via a friend.
        for (const auto& pid : getConnectedPeerIds())
        {
            if (auto u = identity_manager->uniqueForPeer(pid))
                os << "  " << pid << " -> " << *u << "\n";
        }
    }

    return os.str();
}
void NetworkManager::clearDragState(uint64_t markerId)
{
    drag_.erase(markerId);
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

// ----------------------------
// Broadcast wrappers
// ----------------------------

void NetworkManager::broadcastMarkerDelete(uint64_t boardId, const flecs::entity& marker)
{
    auto ids = getConnectedPeerIds();
    if (!ids.empty())
        sendMarkerDelete(boardId, marker, ids);
}

void NetworkManager::broadcastFogUpdate(uint64_t boardId, const flecs::entity& fog)
{
    auto ids = getConnectedPeerIds();
    if (!ids.empty())
        sendFogUpdate(boardId, fog, ids);
}

void NetworkManager::broadcastFogDelete(uint64_t boardId, const flecs::entity& fog)
{
    auto ids = getConnectedPeerIds();
    if (!ids.empty())
        sendFogDelete(boardId, fog, ids);
}

// NetworkManager.cpp

void NetworkManager::buildUserNameUpdate(std::vector<uint8_t>& out,
                                         uint64_t tableId,
                                         const std::string& userUniqueId,
                                         const std::string& oldUsername,
                                         const std::string& newUsername,
                                         bool reboundFlag) const
{
    out.clear();
    Serializer::serializeUInt64(out, tableId);
    Serializer::serializeString(out, userUniqueId); // <— uniqueId
    Serializer::serializeString(out, oldUsername);
    Serializer::serializeString(out, newUsername);
    Serializer::serializeUInt8(out, reboundFlag ? 1 : 0);
}

void NetworkManager::broadcastUserNameUpdate(const std::vector<uint8_t>& payload)
{
    for (auto& [pid, link] : peers)
    {
        if (!link)
            continue;
        sendUserNameUpdateTo(pid, payload);
    }
}

void NetworkManager::sendUserNameUpdateTo(const std::string& peerId,
                                          const std::vector<uint8_t>& payload)
{
    auto it = peers.find(peerId);
    if (it == peers.end() || !it->second)
        return;

    std::vector<uint8_t> frame;
    frame.reserve(1 + payload.size());
    frame.push_back((uint8_t)msg::DCType::UserNameUpdate);
    frame.insert(frame.end(), payload.begin(), payload.end());

    it->second->sendOn(msg::dc::name::Game, frame);
}

// NetworkManager.cpp
bool NetworkManager::broadcastChatJson(const msg::Json& j)
{
    const std::string text = j.dump(); // UTF-8 JSON
    bool any = false;
    for (auto& [pid, link] : peers)
    {
        if (link->sendOn(msg::dc::name::Chat, std::string_view(text)))
            any = true;
    }
    return any;
}
bool NetworkManager::sendChatJsonTo(const std::string& peerId, const msg::Json& j)
{
    auto it = peers.find(peerId);
    if (it == peers.end() || !it->second)
        return false;
    auto& link = it->second;
    if (!link->isConnected())
        return false;
    return link->sendOn(msg::dc::name::Chat, std::string_view(j.dump()));
}
bool NetworkManager::sendChatJsonTo(const std::set<std::string>& targets, const msg::Json& j)
{
    const std::string text = j.dump();
    bool any = false;
    for (auto& pid : targets)
    {
        auto it = peers.find(pid);
        if (it == peers.end() || !it->second)
            continue;
        auto& link = it->second;
        if (!link->isConnected())
            continue;
        if (link->sendOn(msg::dc::name::Chat, std::string_view(text)))
            any = true;
    }
    return any;
}

void NetworkManager::decodeRawGameBuffer(const std::string& fromPeer, const std::vector<uint8_t>& b)
{
    decodingFromPeer_ = fromPeer;
    size_t off = 0;
    while (off < b.size())
    {
        if (!ensureRemaining(b, off, 1))
            break;

        auto type = static_cast<msg::DCType>(b[off]);
        off += 1;

        Logger::instance().log("localtunnel", Logger::Level::Info, msg::DCtypeString(type) + " Received!!");
        switch (type)
        {
            case msg::DCType::Snapshot_GameTable:
                handleGameTableSnapshot(b, off);
                Logger::instance().log("localtunnel", Logger::Level::Info, "Snapshot_GameTable Handled!!");
                break;

            case msg::DCType::Snapshot_Board:
                handleBoardMeta(b, off); // fills imagesRx_
                Logger::instance().log("localtunnel", Logger::Level::Info, "Snapshot_Board Handled!!");
                break;

            case msg::DCType::MarkerCreate:
                handleMarkerMeta(b, off); // fills imagesRx_
                //Logger::instance().log("localtunnel", Logger::Level::Info, "MarkerCreate Handled!!");
                break;

            case msg::DCType::FogCreate:
                handleFogCreate(b, off);
                //Logger::instance().log("localtunnel", Logger::Level::Info, "FogCreate Handled!!");
                break;

            case msg::DCType::ImageChunk:
                handleImageChunk(b, off);
                Logger::instance().log("localtunnel", Logger::Level::Info, "ImageChunk Handled!!");
                break;

            case msg::DCType::CommitBoard:
                handleCommitBoard(b, off);
                Logger::instance().log("localtunnel", Logger::Level::Info, "CommitBoard Handled!!");
                break;

            case msg::DCType::CommitMarker:
                handleCommitMarker(b, off);
                Logger::instance().log("localtunnel", Logger::Level::Info, "CommitMarker Handled!!");
                break;

            case msg::DCType::MarkerUpdate:
                handleMarkerUpdate(b, off);
                Logger::instance().log("localtunnel", Logger::Level::Info, "MarkerUpdate Handled!!");
                break;

            case msg::DCType::MarkerMoveState:
                handleMarkerMoveState(b, off);
                Logger::instance().log("localtunnel", Logger::Level::Info, "MarkerUpdate Handled!!");
                break;

            case msg::DCType::FogUpdate:
                handleFogUpdate(b, off);
                Logger::instance().log("localtunnel", Logger::Level::Info, "FogUpdate Handled!!");
                break;

            case msg::DCType::MarkerDelete:
                handleMarkerDelete(b, off);
                Logger::instance().log("localtunnel", Logger::Level::Info, "MarkerDelete Handled!!");
                break;

            case msg::DCType::FogDelete:
                handleFogDelete(b, off);
                Logger::instance().log("localtunnel", Logger::Level::Info, "MarkerDelete FogDelete!!");
                break;

            case msg::DCType::GridUpdate:
                handleGridUpdate(b, off);
                Logger::instance().log("localtunnel", Logger::Level::Info, "GridUpdate Handled!!");
                break;

            case msg::DCType::UserNameUpdate:
            {
                handleUserNameUpdate(b, off);
                break;
            }
            default:
                Logger::instance().log("localtunnel", Logger::Level::Warn, "Unkown Message Type not Handled!!");
                break;
        }
    }
    decodingFromPeer_.clear();
}

// MARKER MOVE OPERATIONS -----------------------------------------------------------------------------------------------------------------------------------------
void NetworkManager::decodeRawMarkerMoveBuffer(const std::string& fromPeer, const std::vector<uint8_t>& b)
{
    decodingFromPeer_ = fromPeer;
    size_t off = 0;
    while (off < b.size())
    {
        if (!ensureRemaining(b, off, 1))
            break;

        auto type = static_cast<msg::DCType>(b[off]);
        off += 1;
        Logger::instance().log("localtunnel", Logger::Level::Info, msg::DCtypeString(type) + " Received!! MarkerMove");
        if (type != msg::DCType::MarkerMove)
        {
            // If the sender packed something else on this DC, bail
            return;
        }

        handleMarkerMove(b, off); // parses one frame and updates coalescer
        Logger::instance().log("localtunnel", Logger::Level::Info, "MarkerMove Handled!!");
    }
    decodingFromPeer_.clear();
}

void NetworkManager::handleMarkerMove(const std::vector<uint8_t>& raw, size_t& off)
{
    if (!ensureRemaining(raw, off, 8 + 8 + 4 + 4 + 8 + 8)) // 8 bytes Position (2x int32)
        return;

    const auto& b = reinterpret_cast<const std::vector<unsigned char>&>(raw);

    msg::ReadyMessage m;
    m.kind = msg::DCType::MarkerMove;
    m.boardId = Serializer::deserializeUInt64(b, off);
    m.markerId = Serializer::deserializeUInt64(b, off);
    m.dragEpoch = Serializer::deserializeUInt32(b, off);
    m.seq = Serializer::deserializeUInt32(b, off);
    m.ts = Serializer::deserializeUInt64(b, off);
    Position p = Serializer::deserializePosition(b, off);
    m.pos = p;
    m.mov = Moving{true};

    m.fromPeerId = decodingFromPeer_;

    inboundGame_.push(std::move(m));
}

// MarkerUpdate
void NetworkManager::handleMarkerUpdate(const std::vector<uint8_t>& b, size_t& off)
{
    msg::ReadyMessage m;
    m.kind = msg::DCType::MarkerUpdate;

    m.boardId = Serializer::deserializeUInt64(b, off);
    m.markerId = Serializer::deserializeUInt64(b, off);
    m.size = Serializer::deserializeSize(b, off);
    m.vis = Serializer::deserializeVisibility(b, off);
    m.markerComp = Serializer::deserializeMarkerComponent(b, off);

    m.fromPeerId = decodingFromPeer_;
    inboundGame_.push(std::move(m));
}
void NetworkManager::handleMarkerMoveState(const std::vector<uint8_t>& raw, size_t& off)
{
    // Expect (after type):
    // [boardId:u64][markerId:u64][epoch:u32][seq:u32][ts:u64][Moving][(Position if final)]
    if (!ensureRemaining(raw, off, 8 + 8 + 4 + 4 + 8 + 1))
        return;

    const auto& b = reinterpret_cast<const std::vector<unsigned char>&>(raw);

    msg::ReadyMessage m;
    m.kind = msg::DCType::MarkerMoveState;
    m.boardId = Serializer::deserializeUInt64(b, off);
    m.markerId = Serializer::deserializeUInt64(b, off);
    m.dragEpoch = Serializer::deserializeUInt32(b, off);
    m.seq = Serializer::deserializeUInt32(b, off);
    m.ts = Serializer::deserializeUInt64(b, off);
    Moving mv = Serializer::deserializeMoving(b, off);
    m.mov = mv;

    if (!mv.isDragging)
    {
        if (!ensureRemaining(raw, off, 8))
            return; // Position (2x int)
        Position p = Serializer::deserializePosition(b, off);
        m.pos = p;
    }
    m.fromPeerId = decodingFromPeer_;
    inboundGame_.push(std::move(m));
}

std::vector<unsigned char> NetworkManager::buildMarkerMoveFrame(uint64_t boardId, const flecs::entity& marker, uint32_t seq)
{
    std::vector<unsigned char> out;
    const auto* id = marker.get<Identifier>();
    const auto* pos = marker.get<Position>();
    if (!id || !pos)
        return out;

    // epoch from drag_ state
    auto& s = drag_[id->id];
    if (s.locallyProposedEpoch == 0 && s.epoch == 0)
    {
        s.locallyProposedEpoch = s.epoch + 1;
        s.epoch = s.locallyProposedEpoch;
    }
    const uint32_t epoch = (s.locallyProposedEpoch ? s.locallyProposedEpoch : s.epoch);
    const uint64_t ts = nowMs();

    Serializer::serializeUInt8(out, static_cast<uint8_t>(msg::DCType::MarkerMove));
    Serializer::serializeUInt64(out, boardId);
    Serializer::serializeUInt64(out, id->id);
    Serializer::serializeUInt32(out, epoch);
    Serializer::serializeUInt32(out, seq);
    Serializer::serializeUInt64(out, ts);
    Serializer::serializePosition(out, pos);
    return out;
}
std::vector<unsigned char> NetworkManager::buildMarkerMoveStateFrame(uint64_t boardId, const flecs::entity& marker)
{
    std::vector<unsigned char> out;
    const auto* id = marker.get<Identifier>();
    const auto* mv = marker.get<Moving>();
    if (!id || !mv)
        return out;

    auto& s = drag_[id->id];
    if (s.locallyProposedEpoch == 0 && s.epoch == 0)
    {
        s.locallyProposedEpoch = s.epoch + 1;
        s.epoch = s.locallyProposedEpoch;
    }
    const uint32_t epoch = (s.locallyProposedEpoch ? s.locallyProposedEpoch : s.epoch);
    const uint32_t seq = ++s.localSeq;
    const uint64_t ts = nowMs();
    s.lastTxMs = ts;

    Serializer::serializeUInt8(out, static_cast<uint8_t>(msg::DCType::MarkerMoveState));
    Serializer::serializeUInt64(out, boardId);
    Serializer::serializeUInt64(out, id->id);
    Serializer::serializeUInt32(out, epoch);
    Serializer::serializeUInt32(out, seq);
    Serializer::serializeUInt64(out, ts);
    Serializer::serializeMoving(out, mv); // isDragging

    if (!mv->isDragging && marker.has<Position>())
    {
        const auto* pos = marker.get<Position>(); // final pos at end
        Serializer::serializePosition(out, pos);
    }
    return out;
}

// ---- MARKER UPDATE/DELETE ----
std::vector<unsigned char> NetworkManager::buildMarkerUpdateFrame(uint64_t boardId, const flecs::entity& marker)
{
    std::vector<unsigned char> b;
    Serializer::serializeUInt8(b, static_cast<uint8_t>(msg::DCType::MarkerUpdate));
    Serializer::serializeUInt64(b, boardId);

    const auto id = marker.get<Identifier>()->id;
    const auto siz = *marker.get<Size>();
    const auto vis = *marker.get<Visibility>();
    const auto mc = *marker.get<MarkerComponent>();

    Serializer::serializeUInt64(b, id);
    Serializer::serializeSize(b, &siz);
    Serializer::serializeVisibility(b, &vis);
    Serializer::serializeMarkerComponent(b, &mc);
    return b;
}

void NetworkManager::broadcastMarkerUpdate(uint64_t boardId, const flecs::entity& marker)
{
    auto ids = getConnectedPeerIds();
    if (!ids.empty())
        sendMarkerUpdate(boardId, marker, ids);
}

void NetworkManager::broadcastMarkerMove(uint64_t boardId, const flecs::entity& marker)
{
    auto ids = getConnectedPeerIds();
    if (!ids.empty())
        sendMarkerMove(boardId, marker, ids);
}
void NetworkManager::sendMarkerMove(uint64_t boardId, const flecs::entity& marker, const std::vector<std::string>& toPeerIds)
{
    if (!marker.is_valid() || !marker.has<Identifier>() || !marker.has<Position>())
        return;

    const uint64_t markerId = marker.get<Identifier>()->id;

    // per-drag local seq in DragState
    auto& s = drag_[markerId];
    if (s.locallyProposedEpoch == 0 && s.epoch == 0)
    {
        s.locallyProposedEpoch = s.epoch + 1;
        s.epoch = s.locallyProposedEpoch;
    }
    const uint32_t seq = ++s.localSeq;
    s.lastTxMs = nowMs();

    auto frame = buildMarkerMoveFrame(boardId, marker, seq);
    if (frame.empty())
        return;

    for (auto& pid : toPeerIds)
    {
        if (auto it = peers.find(pid); it != peers.end() && it->second)
            it->second->sendMarkerMove(frame);
    }
}

void NetworkManager::broadcastMarkerMoveState(uint64_t boardId, const flecs::entity& marker)
{
    auto ids = getConnectedPeerIds();
    if (!ids.empty())
        sendMarkerMoveState(boardId, marker, ids);
}

void NetworkManager::sendMarkerMoveState(uint64_t boardId, const flecs::entity& marker, const std::vector<std::string>& toPeerIds)
{
    if (!marker.is_valid() || !marker.has<Identifier>() || !marker.has<Moving>())
        return;

    auto frame = buildMarkerMoveStateFrame(boardId, marker);
    if (frame.empty())
        return;

    // Reliable: use your game channel
    broadcastGameFrame(frame, toPeerIds);
}

//bool NetworkManager::amIDragging(uint64_t markerId) const
//{
//    auto it = drag_.find(markerId);
//    if (it == drag_.end())
//        return false;
//    const auto& s = it->second;
//    return !s.closed && s.ownerPeerId == getMyPeerId();
//}

bool NetworkManager::amIDragging(uint64_t markerId) const
{
    auto it = drag_.find(markerId);
    if (it == drag_.end())
        return false;
    const auto& s = it->second;

    // If we started the drag locally, trust the local flag.
    if (!s.closed && s.locallyDragging)
        return true;

    // Otherwise fall back to peer-id ownership (remote handoff cases).
    return !s.closed && s.ownerPeerId == getMyPeerId();
}

// markDraggingLocal — called by BoardManager on start/end
void NetworkManager::markDraggingLocal(uint64_t markerId, bool dragging)
{
    auto& s = drag_[markerId];
    if (dragging)
    {
        s.locallyDragging = true;
        s.locallyProposedEpoch = (s.closed ? (s.epoch + 1) : s.epoch);
        s.localSeq = 0;
        s.ownerPeerId = getMyPeerId();
        s.epochOpenedMs = nowMs();
        s.closed = false;
        if (s.locallyProposedEpoch > s.epoch)
            s.epoch = s.locallyProposedEpoch;
    }
    else
    {
        s.locallyDragging = false; // we close epoch on final frame or forceCloseDrag
    }
}

bool NetworkManager::isMarkerBeingDragged(uint64_t markerId) const
{
    auto it = drag_.find(markerId);
    return (it != drag_.end() && !it->second.closed);
}

void NetworkManager::forceCloseDrag(uint64_t markerId)
{
    auto it = drag_.find(markerId);
    if (it == drag_.end())
        return;
    auto& s = it->second;
    s.closed = true;
    s.locallyDragging = false;
    s.localSeq = 0;
}

void NetworkManager::sendMarkerUpdate(uint64_t boardId, const flecs::entity& marker,
                                      const std::vector<std::string>& toPeerIds)
{
    if (!marker.is_valid() || !marker.has<Identifier>())
        return;

    auto frame = buildMarkerUpdateFrame(boardId, marker);
    broadcastGameFrame(frame, toPeerIds);
}

bool NetworkManager::shouldApplyMarkerMove(const msg::ReadyMessage& m)
{
    if (!m.markerId || !m.dragEpoch)
        return false;
    auto& s = drag_[*m.markerId];

    // Epoch
    if (*m.dragEpoch < s.epoch)
        return false;
    if (*m.dragEpoch > s.epoch)
    {
        s.epoch = *m.dragEpoch;
        s.closed = false;
        s.lastSeq = 0;
        s.ownerPeerId = m.fromPeerId;
    }
    else
    {
        if (s.closed)
            return false;
        if (!s.ownerPeerId.empty() && s.ownerPeerId != m.fromPeerId)
        {
            if (!tieBreakWins(m.fromPeerId, s.ownerPeerId))
                return false;
            s.ownerPeerId = m.fromPeerId;
            if (s.locallyDragging)
            {
                s.locallyDragging = false;
                s.localSeq = 0;
            }
        }
        else
        {
            s.ownerPeerId = m.fromPeerId; // first owner for this epoch
        }
    }

    // Seq
    if (m.seq && *m.seq <= s.lastSeq)
        return false;
    if (m.seq)
        s.lastSeq = *m.seq;

    // Local echo suppression while we drag
    if (s.locallyDragging)
        return false;

    return true;
}

bool NetworkManager::shouldApplyMarkerMoveStateStart(const msg::ReadyMessage& m)
{
    if (!m.markerId || !m.dragEpoch || !m.mov || !m.mov->isDragging)
        return false;
    auto& s = drag_[*m.markerId];

    if (*m.dragEpoch < s.epoch)
        return false;
    if (*m.dragEpoch > s.epoch)
    {
        s.epoch = *m.dragEpoch;
        s.closed = false;
        s.lastSeq = 0;
        s.ownerPeerId = m.fromPeerId;
    }
    else
    {
        if (s.closed)
            return false;
        if (!s.ownerPeerId.empty() && s.ownerPeerId != m.fromPeerId)
        {
            if (!tieBreakWins(m.fromPeerId, s.ownerPeerId))
                return false;
            s.ownerPeerId = m.fromPeerId;
            if (s.locallyDragging)
            {
                s.locallyDragging = false;
                s.localSeq = 0;
            }
        }
        else
        {
            s.ownerPeerId = m.fromPeerId;
        }
    }

    if (m.seq && *m.seq <= s.lastSeq)
        return false;
    if (m.seq)
        s.lastSeq = *m.seq;

    return true;
}

bool NetworkManager::shouldApplyMarkerMoveStateFinal(const msg::ReadyMessage& m)
{
    if (!m.markerId || !m.dragEpoch || !m.mov || m.mov->isDragging)
        return false;
    auto& s = drag_[*m.markerId];

    if (*m.dragEpoch < s.epoch)
        return false;
    if (*m.dragEpoch > s.epoch)
    {
        s.epoch = *m.dragEpoch;
        s.closed = false;
        s.lastSeq = 0;
        s.ownerPeerId = m.fromPeerId;
    }
    else
    {
        if (s.closed)
            return false;
        if (!s.ownerPeerId.empty() && s.ownerPeerId != m.fromPeerId)
        {
            if (!tieBreakWins(m.fromPeerId, s.ownerPeerId))
                return false;
            s.ownerPeerId = m.fromPeerId;
            if (s.locallyDragging)
            {
                s.locallyDragging = false;
                s.localSeq = 0;
            }
        }
        else
        {
            s.ownerPeerId = m.fromPeerId;
        }
        if (m.seq && *m.seq < s.lastSeq)
            return false;
        if (m.seq && *m.seq >= s.lastSeq)
            s.lastSeq = *m.seq;
    }

    // finalize epoch
    s.closed = true;
    s.locallyDragging = false;
    s.localSeq = 0;

    return true;
}
// END MARKER OPERATIONS -----------------------------------------------------------------------------------------------------------------------------------------

void NetworkManager::sendMarkerDelete(uint64_t boardId, const flecs::entity& marker,
                                      const std::vector<std::string>& toPeerIds)
{
    if (!marker.is_valid() || !marker.has<Identifier>())
        return;

    const uint64_t mid = marker.get<Identifier>()->id;
    auto frame = buildMarkerDeleteFrame(boardId, mid);
    broadcastGameFrame(frame, toPeerIds);
}

void NetworkManager::sendFogUpdate(uint64_t boardId, const flecs::entity& fog,
                                   const std::vector<std::string>& toPeerIds)
{
    if (!fog.is_valid() || !fog.has<Identifier>())
        return;

    auto frame = buildFogUpdateFrame(boardId, fog);
    broadcastGameFrame(frame, toPeerIds);
}

void NetworkManager::sendFogDelete(uint64_t boardId, const flecs::entity& fog,
                                   const std::vector<std::string>& toPeerIds)
{
    if (!fog.is_valid() || !fog.has<Identifier>())
        return;

    const uint64_t fid = fog.get<Identifier>()->id;
    auto frame = buildFogDeleteFrame(boardId, fid);
    broadcastGameFrame(frame, toPeerIds);
}

// ---------- MESSAGE SENDERS --------------------------------------------------------------------------------
void NetworkManager::sendGameTable(const flecs::entity& gameTable, const std::vector<std::string>& toPeerIds)
{
    auto gtId = gameTable.get<Identifier>()->id;
    auto gt = *gameTable.get<GameTable>();
    auto frame = buildSnapshotGameTableFrame(gtId, gt.gameTableName);
    broadcastGameFrame(frame, toPeerIds);
}

void NetworkManager::sendBoard(const flecs::entity& board, const std::vector<std::string>& toPeerIds)
{
    // read board image (from TextureComponent.image_path)
    auto tex = board.get<TextureComponent>();
    auto image_path = tex->image_path;
    Logger::instance().log("localtunnel", Logger::Level::Info, "Board Texture Path: " + image_path);
    auto is_file_only = PathManager::isFilenameOnly(tex->image_path);
    auto is_path_like = PathManager::isPathLike(tex->image_path);
    if (is_file_only || !is_path_like)
    {
        Logger::instance().log("localtunnel", Logger::Level::Info, "Board Texture Path is FILE ONLY");
        auto map_folder = PathManager::getMapsPath();
        auto image_path_ = map_folder / tex->image_path;
        image_path = image_path_.string();
    }
    Logger::instance().log("localtunnel", Logger::Level::Info, "Board Texture Path AGAIN: " + image_path);
    std::vector<unsigned char> img = tex ? readFileBytes(image_path) : std::vector<unsigned char>{};

    // 1) meta
    auto meta = buildSnapshotBoardFrame(board, static_cast<uint64_t>(img.size()));
    broadcastGameFrame(meta, toPeerIds);

    //// 2) image chunks (ownerKind=0 for board)
    uint64_t bid = board.get<Identifier>()->id;
    sendImageChunks(msg::ImageOwnerKind::Board, bid, img, toPeerIds);

    // 3) commit
    auto commit = buildCommitBoardFrame(bid);
    broadcastGameFrame(commit, toPeerIds);

    board.children([&](flecs::entity child)
                   {
			if (child.has<MarkerComponent>()) {
                uint64_t bid = board.get<Identifier>()->id;
                sendMarker(bid, child, toPeerIds);
                Logger::instance().log("localtunnel", Logger::Level::Info, "SentMarker");
			}
			else if (child.has<FogOfWar>()) {
                uint64_t bid = board.get<Identifier>()->id;
                sendFog(bid, child, toPeerIds);
                Logger::instance().log("localtunnel", Logger::Level::Info, "SentFog");
			} });
}

void NetworkManager::sendMarker(uint64_t boardId, const flecs::entity& marker, const std::vector<std::string>& toPeerIds)
{
    const TextureComponent* tex = marker.get<TextureComponent>();
    auto image_path = tex->image_path;
    Logger::instance().log("localtunnel", Logger::Level::Info, "Marker Path: " + image_path);
    auto is_file_only = PathManager::isFilenameOnly(tex->image_path);
    auto is_path_like = PathManager::isPathLike(tex->image_path);
    if (is_file_only || !is_path_like)
    {
        Logger::instance().log("localtunnel", Logger::Level::Info, "Marker Path is FILE ONLY");
        auto marker_folder = PathManager::getMarkersPath();
        auto image_path_ = marker_folder / tex->image_path;
        image_path = image_path_.string();
    }
    Logger::instance().log("localtunnel", Logger::Level::Info, "Marker Path Again: " + image_path);
    std::vector<unsigned char> img = tex ? readFileBytes(image_path) : std::vector<unsigned char>{};
    Logger::instance().log("localtunnel", Logger::Level::Info, "Marker Texture Byte Size: " + std::to_string(img.size()));
    // 1) meta
    auto meta = buildCreateMarkerFrame(boardId, marker, static_cast<uint64_t>(img.size()));
    broadcastGameFrame(meta, toPeerIds);

    // 2) image chunks (ownerKind=1 for marker)
    uint64_t mid = marker.get<Identifier>()->id;
    sendImageChunks(msg::ImageOwnerKind::Marker, mid, img, toPeerIds);
    //uint64_t off = 0;
    //while (off < img.size())
    //{
    //    size_t chunk = std::min<size_t>(kChunk, img.size() - off);
    //    auto frame = buildImageChunkFrame(/*ownerKind=*/1, mid, off, img.data() + off, chunk);
    //    Logger::instance().log("localtunnel", Logger::Level::Info, "Marker Texture Byte Size: " + img.size());
    //    broadcastGameFrame(frame, toPeerIds);
    //    off += chunk;
    //}

    // 3) commit
    auto commit = buildCommitMarkerFrame(boardId, mid);
    broadcastGameFrame(commit, toPeerIds);
}

void NetworkManager::sendFog(uint64_t boardId, const flecs::entity& fog, const std::vector<std::string>& toPeerIds)
{
    auto frame = buildFogCreateFrame(boardId, fog);
    broadcastGameFrame(frame, toPeerIds);
}

// --- tuning ---

// Sends an entire image as ImageChunk frames (reliable/ordered DC).
// kind: 0=Board, 1=Marker (msg::ImageOwnerKind)
// id:   boardId when Board; markerId when Marker
// toPeerIds: who to send to
bool NetworkManager::sendImageChunks(msg::ImageOwnerKind kind, uint64_t id, const std::vector<unsigned char>& img, const std::vector<std::string>& toPeerIds)
{
    Logger::instance().log("localtunnel", Logger::Level::Info,
                           std::string("sendImageChunks: kind=") + (kind == msg::ImageOwnerKind::Board ? "Board" : "Marker") +
                               " id=" + std::to_string(id) + " bytes=" + std::to_string(img.size()));

    size_t sent = 0;
    int paced = 0;
    while (sent < img.size())
    {
        const size_t chunk = std::min(kChunk, img.size() - sent);
        const auto frame = buildImageChunkFrame(static_cast<uint8_t>(kind), id, sent, img.data() + sent, chunk);

        // If you have per-peer send that returns bool, check and log
        bool allOk = true;
        for (auto& pid : toPeerIds)
        {
            sendGameTo(pid, frame); // if your API is void, keep it; otherwise log ok
        }

        // Fallback pacing (keeps buffers happy)
        if ((++paced % kPaceEveryN) == 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(kPaceMillis));

        sent += chunk;
    }

    Logger::instance().log("localtunnel", Logger::Level::Info,
                           "sendImageChunks: done kind=" + std::string(kind == msg::ImageOwnerKind::Board ? "Board" : "Marker") +
                               " id=" + std::to_string(id) + " total=" + std::to_string(img.size()));
    return true;
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
//

void NetworkManager::broadcastGridUpdate(uint64_t boardId, const flecs::entity& board)
{
    auto ids = getConnectedPeerIds();
    if (!ids.empty())
        sendGridUpdate(boardId, board, ids);
}

void NetworkManager::sendGridUpdate(uint64_t boardId, const flecs::entity& board,
                                    const std::vector<std::string>& toPeerIds)
{
    if (!board.is_valid() || !board.has<Grid>())
        return;

    const auto* g = board.get<Grid>();
    auto frame = buildGridUpdateFrame(boardId, *g);
    broadcastGameFrame(frame, toPeerIds);
}

std::vector<unsigned char> NetworkManager::buildGridUpdateFrame(uint64_t boardId, const Grid& grid)
{
    std::vector<unsigned char> b;
    Serializer::serializeUInt8(b, static_cast<uint8_t>(msg::DCType::GridUpdate));
    Serializer::serializeUInt64(b, boardId);
    Serializer::serializeGrid(b, &grid);
    return b;
}

void NetworkManager::handleGridUpdate(const std::vector<uint8_t>& b, size_t& off)
{
    // type byte already consumed by the caller switch
    if (!ensureRemaining(b, off, 8))
        return;

    msg::ReadyMessage m;
    m.kind = msg::DCType::GridUpdate;
    m.boardId = Serializer::deserializeUInt64(b, off);
    Grid g = Serializer::deserializeGrid(b, off);
    m.grid = g;
    inboundGame_.push(std::move(m));
}

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

void NetworkManager::handleCommitBoard(const std::vector<uint8_t>& b, size_t& off)
{
    uint64_t boardId = Serializer::deserializeUInt64(b, off);

    auto it = imagesRx_.find(boardId);
    if (it == imagesRx_.end())
    {
        // Late/meta-less commit? Create a placeholder so tryFinalize can run if chunks arrive later.
        PendingImage p;
        p.kind = msg::ImageOwnerKind::Board;
        p.id = boardId;
        imagesRx_.emplace(boardId, std::move(p));
        it = imagesRx_.find(boardId);
    }

    it->second.commitRequested = true;
    tryFinalizeImage(msg::ImageOwnerKind::Board, boardId);
}

void NetworkManager::handleImageChunk(const std::vector<uint8_t>& b, size_t& off)
{
    if (off + 1 > b.size())
    {
        Logger::instance().log("localtunnel", Logger::Level::Error, "ImageChunk: underrun(kind)");
        return;
    }
    auto kind = static_cast<msg::ImageOwnerKind>(b[off]);
    off += 1;

    if (off + 8 + 8 + 4 > b.size())
    {
        Logger::instance().log("localtunnel", Logger::Level::Error, "ImageChunk: underrun(headers)");
        return;
    }
    uint64_t id = Serializer::deserializeUInt64(b, off);
    uint64_t off64 = Serializer::deserializeUInt64(b, off);
    int len = Serializer::deserializeInt(b, off);

    if (len < 0 || off + static_cast<size_t>(len) > b.size())
    {
        Logger::instance().log("localtunnel", Logger::Level::Error,
                               "ImageChunk: invalid len id=" + std::to_string(id) + " len=" + std::to_string(len));
        return;
    }

    auto it = imagesRx_.find(id);
    if (it == imagesRx_.end())
    {
        Logger::instance().log("localtunnel", Logger::Level::Warn,
                               "ImageChunk: unknown id=" + std::to_string(id));
        off += static_cast<size_t>(len);
        return;
    }

    auto& p = it->second;
    if (p.kind != kind || p.total == 0)
    {
        Logger::instance().log("localtunnel", Logger::Level::Warn,
                               "ImageChunk: kind/total mismatch id=" + std::to_string(id));
        off += static_cast<size_t>(len);
        return;
    }

    if (off64 + static_cast<uint64_t>(len) > p.total)
    {
        Logger::instance().log("localtunnel", Logger::Level::Error,
                               "ImageChunk: out-of-bounds id=" + std::to_string(id) +
                                   " off=" + std::to_string(off64) + " len=" + std::to_string(len) +
                                   " total=" + std::to_string(p.total));
        off += static_cast<size_t>(len);
        return;
    }

    std::memcpy(p.buf.data() + off64, b.data() + off, static_cast<size_t>(len));
    off += static_cast<size_t>(len);
    p.received += static_cast<uint64_t>(len);

    // Occasional progress log (e.g., every ~1MB)
    if ((p.received % (1u << 20)) < static_cast<uint64_t>(len))
    {
        Logger::instance().log("localtunnel", Logger::Level::Info,
                               "ImageChunk: id=" + std::to_string(id) + " " +
                                   std::to_string(p.received) + "/" + std::to_string(p.total));
    }
}

void NetworkManager::handleCommitMarker(const std::vector<uint8_t>& b, size_t& off)
{
    uint64_t boardId = Serializer::deserializeUInt64(b, off);
    uint64_t markerId = Serializer::deserializeUInt64(b, off);

    auto it = imagesRx_.find(markerId);
    if (it == imagesRx_.end())
    {
        PendingImage p;
        p.kind = msg::ImageOwnerKind::Marker;
        p.id = markerId;
        p.boardId = boardId;
        imagesRx_.emplace(markerId, std::move(p));
        it = imagesRx_.find(markerId);
    }
    else
    {
        it->second.boardId = boardId; // ensure it's set
    }

    it->second.commitRequested = true;
    tryFinalizeImage(msg::ImageOwnerKind::Marker, markerId);
}

void NetworkManager::handleUserNameUpdate(const std::vector<uint8_t>& b, size_t& off)
{
    msg::ReadyMessage r;
    r.kind = msg::DCType::UserNameUpdate;
    r.fromPeerId = decodingFromPeer_;
    r.tableId = Serializer::deserializeUInt64(b, off);

    // This is UNIQUE ID now:
    const std::string uniqueId = Serializer::deserializeString(b, off);
    const std::string oldU = Serializer::deserializeString(b, off);
    const std::string newU = Serializer::deserializeString(b, off);
    const uint8_t rebound = Serializer::deserializeUInt8(b, off);

    // Keep old ReadyMessage fields for upstream handlers (uses userPeerId/name/text in your codebase):
    r.userUniqueId = uniqueId; // NOTE: field name is legacy; carries uniqueId now.
    r.text = oldU;
    r.name = newU;
    r.rebound = rebound;

    Logger::instance().log("chat", Logger::Level::Info,
                           "UserNameUpdate: tbl=" + std::to_string(r.tableId.value_or(0)) +
                               " uid=" + uniqueId + " old=" + oldU + " new=" + newU);

    inboundGame_.push(std::move(r));
}

void NetworkManager::handleMarkerDelete(const std::vector<uint8_t>& b, size_t& off)
{
    msg::ReadyMessage m;
    m.kind = msg::DCType::MarkerDelete;
    m.boardId = Serializer::deserializeUInt64(b, off);
    m.markerId = Serializer::deserializeUInt64(b, off);
    inboundGame_.push(std::move(m));
}

// FogUpdate
void NetworkManager::handleFogUpdate(const std::vector<uint8_t>& b, size_t& off)
{
    msg::ReadyMessage m;
    m.kind = msg::DCType::FogUpdate;
    m.boardId = Serializer::deserializeUInt64(b, off);
    m.fogId = Serializer::deserializeUInt64(b, off);
    m.pos = Serializer::deserializePosition(b, off);
    m.size = Serializer::deserializeSize(b, off);
    m.vis = Serializer::deserializeVisibility(b, off);
    m.mov = Serializer::deserializeMoving(b, off); // remove if you don’t use Moving on fog
    inboundGame_.push(std::move(m));
}

void NetworkManager::handleFogDelete(const std::vector<uint8_t>& b, size_t& off)
{
    msg::ReadyMessage m;
    m.kind = msg::DCType::FogDelete;
    m.boardId = Serializer::deserializeUInt64(b, off);
    m.fogId = Serializer::deserializeUInt64(b, off);
    inboundGame_.push(std::move(m));
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
                //reconnectPeer(ev.peerId);
            }
        }
        else if (ev.type == msg::NetEvent::Type::PcClosed)
        {
            auto it = peers.find(ev.peerId);
            if (it != peers.end() && it->second)
            {
                //reconnectPeer(ev.peerId);
            }
        }
        else if (ev.type == msg::NetEvent::Type::PcOpen)
        {
            pushStatusToast(std::string("[Peer] ") + ev.peerId + " Connected", ImGuiToaster::Level::Good);
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
    using clock = std::chrono::steady_clock;
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
            else if (r.label == msg::dc::name::MarkerMove)
            {
                decodeRawMarkerMoveBuffer(r.fromPeer, r.bytes); // coalesce into moveLatest_
            }
        }
        catch (...)
        {
            // swallow/log
        }
        ++processed;
    }
}

//void NetworkManager::drainInboundRaw(int maxPerTick)
//{
//    int processed = 0;
//    msg::InboundRaw r;
//    while (processed < maxPerTick && inboundRaw_.try_pop(r))
//    {
//        try
//        {
//            if (r.label == msg::dc::name::Game)
//            {
//                decodeRawGameBuffer(r.fromPeer, r.bytes);
//            }
//            else if (r.label == msg::dc::name::Chat)
//            {
//                decodeRawChatBuffer(r.fromPeer, r.bytes);
//            }
//            else if (r.label == msg::dc::name::Notes)
//            {
//                decodeRawNotesBuffer(r.fromPeer, r.bytes);
//            }
//            else if (r.label == msg::dc::name::MarkerMove)
//            {
//                decodeRawMarkerMoveBuffer(r.fromPeer, r.bytes);
//            }
//        }
//        catch (...)
//        {
//            // swallow & optionally push a toaster
//        }
//        ++processed;
//    }
//}

// NetworkManager.cpp
//void NetworkManager::decodeRawChatBuffer(const std::string& fromPeer,
//                                         const std::vector<uint8_t>& b)
//{
//    size_t off = 0;
//    while (off < b.size())
//    {
//        if (!ensureRemaining(b, off, 1))
//            break;
//        auto type = static_cast<msg::DCType>(b[off]);
//        off += 1;
//        Logger::instance().log("localtunnel", Logger::Level::Info, msg::DCtypeString(type) + " Received!! onChat");
//        switch (type)
//        {
//            case msg::DCType::ChatGroupCreate:
//            {
//                msg::ReadyMessage r;
//                r.kind = type;
//                r.fromPeer = fromPeer;
//                r.tableId = Serializer::deserializeUInt64(b, off);
//                r.threadId = Serializer::deserializeUInt64(b, off); // groupId
//                r.name = Serializer::deserializeString(b, off);     // group name (unique)
//                {
//                    const int pc = Serializer::deserializeInt(b, off);
//                    std::set<std::string> parts;
//                    for (int i = 0; i < pc; ++i)
//                        parts.insert(Serializer::deserializeString(b, off));
//                    r.participants = std::move(parts);
//                }
//                inboundGame_.push(std::move(r));
//                break;
//            }
//
//            case msg::DCType::ChatGroupUpdate:
//            {
//                msg::ReadyMessage r;
//                r.kind = type;
//                r.fromPeer = fromPeer;
//                r.tableId = Serializer::deserializeUInt64(b, off);
//                r.threadId = Serializer::deserializeUInt64(b, off);
//                r.name = Serializer::deserializeString(b, off);
//                {
//                    const int pc = Serializer::deserializeInt(b, off);
//                    std::set<std::string> parts;
//                    for (int i = 0; i < pc; ++i)
//                        parts.insert(Serializer::deserializeString(b, off));
//                    r.participants = std::move(parts);
//                }
//                inboundGame_.push(std::move(r));
//                break;
//            }
//
//            case msg::DCType::ChatGroupDelete:
//            {
//                msg::ReadyMessage r;
//                r.kind = type;
//                r.fromPeer = fromPeer;
//                r.tableId = Serializer::deserializeUInt64(b, off);
//                r.threadId = Serializer::deserializeUInt64(b, off);
//                inboundGame_.push(std::move(r));
//                break;
//            }
//
//            case msg::DCType::ChatMessage:
//            {
//                msg::ReadyMessage r;
//                r.kind = type;
//                r.fromPeer = fromPeer;
//                r.tableId = Serializer::deserializeUInt64(b, off);
//                r.threadId = Serializer::deserializeUInt64(b, off); // groupId
//                r.ts = Serializer::deserializeUInt64(b, off);
//                r.name = Serializer::deserializeString(b, off); // username
//                r.text = Serializer::deserializeString(b, off); // body
//                inboundGame_.push(std::move(r));
//                break;
//            }
//
//            default:
//                Logger::instance().log("localtunnel", Logger::Level::Warn, "Unkown Message Type not Handled!! onChat");
//                return; // stop this buffer if unknown/out-of-sync
//        }
//    }
//}

void NetworkManager::decodeRawChatBuffer(const std::string& fromPeer,
                                         const std::vector<uint8_t>& b)
{
    // ---- JSON branch ----
    auto first_non_ws = std::find_if(b.begin(), b.end(), [](uint8_t c)
                                     { return !std::isspace((unsigned char)c); });
    if (first_non_ws != b.end() && *first_non_ws == '{')
    {
        try
        {
            std::string s(b.begin(), b.end()); // UTF-8
            msg::Json j = msg::Json::parse(s);

            msg::ReadyMessage r;
            r.fromPeerId = fromPeer;

            // type
            msg::DCType t;
            if (!msg::DCTypeFromJson(msg::getString(j, "type"), t))
            {
                Logger::instance().log("chat", Logger::Level::Warn, "JSON chat: unknown type");
                return;
            }
            r.kind = t;

            // common fields
            if (j.contains("tableId"))
                r.tableId = (uint64_t)j["tableId"].get<uint64_t>();
            if (j.contains("groupId"))
                r.threadId = (uint64_t)j["groupId"].get<uint64_t>();

            switch (t)
            {
                case msg::DCType::ChatGroupCreate:
                case msg::DCType::ChatGroupUpdate:
                {
                    if (j.contains("name"))
                        r.name = j["name"].get<std::string>();
                    if (j.contains("participants") && j["participants"].is_array())
                    {
                        std::set<std::string> parts;
                        for (auto& e : j["participants"])
                            parts.insert(e.get<std::string>());
                        r.participants = std::move(parts);
                    }
                    break;
                }
                case msg::DCType::ChatGroupDelete:
                {
                    // nothing extra
                    break;
                }
                case msg::DCType::ChatMessage:
                {
                    if (j.contains("ts"))
                        r.ts = (uint64_t)j["ts"].get<uint64_t>();
                    if (j.contains("username"))
                        r.name = j["username"].get<std::string>();
                    if (j.contains("text"))
                        r.text = j["text"].get<std::string>();
                    break;
                }
                default:
                    return; // ignore non-chat types here
            }

            inboundGame_.push(std::move(r));
            return; // handled JSON
        }
        catch (const std::exception& e)
        {
            Logger::instance().log("chat", Logger::Level::Warn, std::string("JSON parse error: ") + e.what());
            // fall through to legacy-binary branch if you still support it
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

        Logger::instance().log("localtunnel", Logger::Level::Info, msg::DCtypeString(type) + " Received!!");
        switch (type)
        {
            case msg::DCType::NoteCreate:
                //handleGameTableSnapshot(b, off); // pushes ReadyMessage internally
                Logger::instance().log("localtunnel", Logger::Level::Info, "NoteCreate Handled!!");
                break;

            case msg::DCType::NoteUpdate:
                //handleBoardMeta(b, off); // fills imagesRx_
                Logger::instance().log("localtunnel", Logger::Level::Info, "NoteUpdate Handled!!");
                break;

            case msg::DCType::NoteDelete:
                //handleMarkerMeta(b, off); // fills imagesRx_
                Logger::instance().log("localtunnel", Logger::Level::Info, "NoteDelete Handled!!");
                break;
            default:
                // unknown / out-of-sync: stop this buffer
                Logger::instance().log("localtunnel", Logger::Level::Warn, "Unkown Message Type not Handled!! onNotes");
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
        using clock = std::chrono::steady_clock;
        auto lastFlush = clock::now();
        for (;;) {
            if (rawWorkerStop_.load())
                break;

            bool didWork = false;

            msg::InboundRaw r;
            if (inboundRaw_.try_pop(r)) {
                try {
                    if (r.label == msg::dc::name::Game) {
                        decodeRawGameBuffer(r.fromPeer, r.bytes);
                    } else if (r.label == msg::dc::name::Chat) {
                        decodeRawChatBuffer(r.fromPeer, r.bytes);
                    } else if (r.label == msg::dc::name::Notes) {
                        decodeRawNotesBuffer(r.fromPeer, r.bytes);
                    } else if (r.label == msg::dc::name::MarkerMove) {
                        decodeRawMarkerMoveBuffer(r.fromPeer, r.bytes);
                    }
                } catch (...) {
                    // swallow
                }
                didWork = true;
            }
            
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

void NetworkManager::bootstrapPeerIfReady(const std::string& peerId)
{
    Logger::instance().log("localtunnel", Logger::Level::Info, "BeginBoostrap");
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
        Logger::instance().log("localtunnel", Logger::Level::Info, "SentGameTable");
    }

    if (bm->isBoardActive())
    {
        auto boardEnt = bm->getActiveBoard();
        if (boardEnt.is_valid() && boardEnt.has<Board>())
        {
            sendBoard(boardEnt, {peerId}); // this sends meta + image chunks + commit
            Logger::instance().log("localtunnel", Logger::Level::Info, "SentBoard");
        }
    }

    link->markBootstrapSent();
}

void NetworkManager::tryFinalizeImage(msg::ImageOwnerKind kind, uint64_t id)
{
    auto it = imagesRx_.find(id);
    if (it == imagesRx_.end())
        return;
    auto& p = it->second;
    if (!p.commitRequested || !p.isComplete())
        return;

    msg::ReadyMessage m;
    if (kind == msg::ImageOwnerKind::Board)
    {
        if (!p.boardMeta)
            return; // need meta
        m.kind = msg::DCType::CommitBoard;
        m.boardId = p.boardMeta->boardId;
        m.boardMeta = p.boardMeta;
    }
    else
    {
        if (!p.markerMeta)
            return; // need meta
        m.kind = msg::DCType::CommitMarker;
        m.boardId = p.boardId; // for markers we also stored the boardId
        m.markerMeta = p.markerMeta;
    }
    m.bytes = std::move(p.buf);
    inboundGame_.push(std::move(m));

    Logger::instance().log("localtunnel", Logger::Level::Info,
                           std::string("tryFinalizeImage: finalized ") +
                               (kind == msg::ImageOwnerKind::Board ? "Board" : "Marker") +
                               " id=" + std::to_string(id));

    imagesRx_.erase(it);
}

// ---------- GAME FRAME BUILDERS ----------

std::vector<unsigned char> NetworkManager::buildMarkerDeleteFrame(uint64_t boardId, uint64_t markerId)
{
    std::vector<unsigned char> b;
    Serializer::serializeUInt8(b, static_cast<uint8_t>(msg::DCType::MarkerDelete));
    Serializer::serializeUInt64(b, boardId);
    Serializer::serializeUInt64(b, markerId);
    return b;
}

// ---- FOG UPDATE/DELETE ----
std::vector<unsigned char> NetworkManager::buildFogUpdateFrame(uint64_t boardId, const flecs::entity& fog)
{
    std::vector<unsigned char> b;
    Serializer::serializeUInt8(b, static_cast<uint8_t>(msg::DCType::FogUpdate));
    Serializer::serializeUInt64(b, boardId);

    const auto id = fog.get<Identifier>()->id;
    const auto pos = *fog.get<Position>();
    const auto siz = *fog.get<Size>();
    const auto vis = *fog.get<Visibility>();

    Serializer::serializeUInt64(b, id);
    Serializer::serializePosition(b, &pos);
    Serializer::serializeSize(b, &siz);
    Serializer::serializeVisibility(b, &vis);
    return b;
}

std::vector<unsigned char> NetworkManager::buildFogDeleteFrame(uint64_t boardId, uint64_t fogId)
{
    std::vector<unsigned char> b;
    Serializer::serializeUInt8(b, static_cast<uint8_t>(msg::DCType::FogDelete));
    Serializer::serializeUInt64(b, boardId);
    Serializer::serializeUInt64(b, fogId);
    return b;
}

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

void NetworkManager::sendGameTo(const std::string& peerId, const std::vector<unsigned char>& bytes)
{
    auto it = peers.find(peerId);
    if (it == peers.end() || !it->second)
        return;
    it->second->sendGame(bytes);
}

void NetworkManager::broadcastGameFrame(const std::vector<unsigned char>& frame, const std::vector<std::string>& toPeerIds)
{
    for (auto& pid : toPeerIds)
    {
        sendGameTo(pid, frame);
    }
}

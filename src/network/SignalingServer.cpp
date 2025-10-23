#include "SignalingServer.h"
#include <rtc/rtc.hpp>
#include <iostream>
#include "NetworkManager.h"
#include <nlohmann/json.hpp>
#include "Message.h"
#include "Logger.h"
#include "NetworkUtilities.h"

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

SignalingServer::SignalingServer(std::weak_ptr<NetworkManager> parent) :
    network_manager(parent)
{
}

SignalingServer::~SignalingServer()
{
    this->stop();
}

void SignalingServer::start(unsigned short port)
{
    rtc::WebSocketServerConfiguration cfg;
    cfg.bindAddress = "0.0.0.0";
    cfg.port = port;
    cfg.connectionTimeout = std::chrono::milliseconds(0);

    server = std::make_shared<rtc::WebSocketServer>(cfg);

    server->onClient([this](std::shared_ptr<rtc::WebSocket> ws)
                     {
        auto addrOpt = ws->remoteAddress();
        if (!addrOpt) { std::cout << "[SignalingServer] Client connected (no addr)\n"; return; }
        std::string clientId = *addrOpt;
        std::cout << "[SignalingServer] Client Connected: " << clientId << "\n";

        onConnect(clientId, ws);

        ws->onMessage([this, clientId](std::variant<rtc::binary, rtc::string> msg) {
            std::cout << "[SignalingServer] MESSAGE RECEIVED" <<"\n";
            if (!std::holds_alternative<rtc::string>(msg)) return;
            const auto& s = std::get<rtc::string>(msg);
            std::cout << "[SignalingServer] MESSAGE: " << s <<"\n";
            //prunePending();
            onMessage(clientId, s);
        });

        ws->onError([clientId](std::string err) {
            std::cout << "[SignalingServer] Client Error (" << clientId << "): " << err << "\n";
            });

        ws->onClosed([this, clientId]() {
            std::cout << "[SignalingServer] Client disconnected: " << clientId << "\n";
            pendingClients_.erase(clientId);
            //pendingSince_.erase(clientId);
            authClients_.erase(clientId);
            }); });

    is_running = true;
    std::cout << "[SignalingServer] Listening at ws://0.0.0.0:" << port << "\n";
}

void SignalingServer::stop()
{
    if (server)
    {
        server->stop();
        server.reset();
        is_running = false;
    }
    for (auto [id, ws] : pendingClients_)
    {
        NetworkUtilities::safeCloseWebSocket(ws);
    }

    for (auto [id, ws] : authClients_)
    {
        NetworkUtilities::safeCloseWebSocket(ws);
    }
    pendingClients_.clear();
    authClients_.clear();
}

void SignalingServer::onConnect(std::string clientId, std::shared_ptr<rtc::WebSocket> ws)
{
    pendingClients_[clientId] = std::move(ws);
    //pendingSince_[clientId] = Clock::now();
}

void SignalingServer::onMessage(const std::string& clientId, const std::string& text)
{

    json j;
    try
    {
        j = json::parse(text);
    }
    catch (...)
    {
        return;
    }

    const std::string type = j.value(msg::key::Type, "");
    if (type.empty())
        return;

    if (type == msg::signaling::Auth)
    {
        auto nm = network_manager.lock();
        if (!nm)
            throw std::runtime_error("NetworkManager expired");

        const std::string provided = j.value(std::string(msg::key::AuthToken), "");
        const std::string expected = nm->getNetworkPassword();

        const bool ok = (expected.empty() || provided == expected);
        const std::string username = j.value(std::string(msg::key::Username), "guest" + clientId);
        const std::string clientUniqueId = j.value(std::string(msg::key::UniqueId), "");

        if (ok)
        {
            moveToAuthenticated(clientId);

            std::vector<std::string> others;
            others.reserve(authClients_.size());
            for (auto& [id, _] : authClients_)
                if (id != clientId)
                    others.emplace_back(id);

            // IMPORTANT: GM id in response must be GM UNIQUE ID
            const std::string gmUniqueId = nm->getMyUniqueId();

            // you may optionally echo client uniqueId back (makeAuthResponse supports uniqueId param)
            auto resp = msg::makeAuthResponse(msg::value::True, "welcome",
                                              clientId, username,
                                              others,
                                              /*gmPeerId=*/gmUniqueId,
                                              /*uniqueId=*/clientUniqueId);
            sendTo(clientId, resp.dump());
        }
        else
        {
            auto resp = msg::makeAuthResponse(msg::value::False, "invalid password",
                                              clientId, username);
            sendTo(clientId, resp.dump());

            if (auto it = pendingClients_.find(clientId); it != pendingClients_.end() && it->second)
                it->second->close();
            else if (auto it2 = authClients_.find(clientId); it2 != authClients_.end() && it2->second)
                it2->second->close();
        }
        return;
    }

    // for all other types, require auth
    if (!isAuthenticated(clientId))
    {
        auto resp = msg::makeAuthResponse(msg::value::False, "unauthenticated",
                                          clientId, "guest" + clientId);
        sendTo(clientId, resp.dump());
        return;
    }

    // Router: overwrite from with server-trusted clientId
    j[msg::key::From] = clientId;

    // In SignalingServer::onMessage (after auth checks)
    if (type == msg::signaling::PeerDisconnect)
    {
        const std::string target = j.value(std::string(msg::key::Target), "");
        if (target.empty())
            return;

        // Broadcast to all authed clients (including target)
        const std::string dump = j.dump();
        for (auto& [id, ws] : authClients_)
        {
            if (ws && !ws->isClosed())
                ws->send(dump);
        }

        // Optionally kick the target at signaling level
        if (auto it = authClients_.find(target); it != authClients_.end())
        {
            if (it->second)
                it->second->close();
        }
        return;
    }

    // Broadcast to authenticated only
    if (j.value(msg::key::Broadcast, msg::value::False) == msg::value::True)
    {
        const std::string dump = j.dump();
        for (auto& [id, ws] : authClients_)
        {
            if (id == clientId)
                continue; // don't echo to sender
            if (ws && !ws->isClosed())
                ws->send(dump);
        }
        return;
    }

    // Direct
    const std::string to = j.value(msg::key::To, "");
    if (to.empty())
        return;
    auto it = authClients_.find(to);
    if (it != authClients_.end() && it->second && !it->second->isClosed())
    {
        it->second->send(j.dump());
    }
}

void SignalingServer::sendTo(const std::string& clientId, const std::string& message)
{
    if (auto it = authClients_.find(clientId); it != authClients_.end() && it->second && !it->second->isClosed())
    {
        it->second->send(message);
        return;
    }
    if (auto it = pendingClients_.find(clientId); it != pendingClients_.end() && it->second && !it->second->isClosed())
    {
        it->second->send(message);
    }
}

void SignalingServer::broadcast(const std::string& message)
{
    for (auto& [id, ws] : authClients_)
    {
        if (ws && !ws->isClosed())
            ws->send(message);
    }
}

void SignalingServer::broadcastShutdown()
{
    nlohmann::json j = msg::makeBroadcastShutdown();
    broadcast(j.dump());
}

void SignalingServer::disconnectAllClients()
{
    // Move out both maps to avoid erase while inside callbacks
    auto auth = std::move(authClients_);
    auto pend = std::move(pendingClients_);
    authClients_.clear();
    pendingClients_.clear();

    auto closer = [](auto& map)
    {
        for (auto& [id, ws] : map)
        {
            if (!ws)
                continue;
            try
            {
                ws->onOpen(nullptr);
                ws->onMessage(nullptr);
                ws->onClosed(nullptr);
                ws->onError(nullptr);
                ws->close();
            }
            catch (...)
            {
                Logger::instance().log("main", Logger::Level::Warn, "WS close failed for " + id);
            }
        }
        map.clear();
    };

    closer(auth);
    closer(pend);
}

void SignalingServer::moveToAuthenticated(const std::string& clientId)
{
    auto it = pendingClients_.find(clientId);
    if (it == pendingClients_.end())
        return;
    authClients_[clientId] = it->second;
    pendingClients_.erase(it);
    //pendingSince_.erase(clientId);
    std::cout << "[SignalingServer] Authenticated: " << clientId << "\n";
}

bool SignalingServer::isAuthenticated(const std::string& clientId) const
{
    return authClients_.find(clientId) != authClients_.end();
}

void SignalingServer::disconnectClient(const std::string& clientId)
{
    auto it = authClients_.find(clientId);
    if (it != authClients_.end() && it->second)
    {
        if (!it->second->isClosed())
            it->second->close();
    }
}

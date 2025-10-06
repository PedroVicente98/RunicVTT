#include "SignalingServer.h"
#include <rtc/rtc.hpp>
#include <iostream>
#include "NetworkManager.h"
#include <nlohmann/json.hpp>
#include "Message.h"
#include "Logger.h"

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

    std::cout << "[SignalingServer] Listening at ws://0.0.0.0:" << port << "\n";
}

void SignalingServer::stop()
{
    if (server)
    {
        server->stop();
        server.reset();
    }
    pendingClients_.clear();
    //pendingSince_.clear();
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

    // AUTH
    if (type == msg::signaling::Auth)
    {
        std::string provided = j.value(msg::key::AuthToken, "");
        std::string expected;

        auto nm = network_manager.lock();
        if (!nm)
        {
            throw std::runtime_error("SignalingServer::onMessage: NetworkManager expired");
        }
        expected = nm->getNetworkPassword();

        const bool ok = (expected.empty() || provided == expected);

        if (ok)
        {
            moveToAuthenticated(clientId);
            std::vector<std::string> others;
            others.reserve(authClients_.size());
            for (auto& [id, ws] : authClients_)
            {
                if (id == clientId)
                    continue;
                others.emplace_back(id);
            }

            auto username = j.value(msg::key::Username, "guest" + clientId);
            auto msg = msg::makeAuthResponse(msg::value::True, "welcome", clientId, username, others);
            sendTo(clientId, msg.dump());
        }
        else
        {
            auto username = j.value(msg::key::Username, "guest" + clientId);
            auto msg = msg::makeAuthResponse(msg::value::False, "invalid password", clientId, username);
            sendTo(clientId, msg.dump());
            if (auto it = pendingClients_.find(clientId); it != pendingClients_.end() && it->second)
            {
                it->second->close();
            }
            else if (auto it2 = authClients_.find(clientId); it2 != authClients_.end() && it2->second)
            {
                it2->second->close();
            }
        }
        return;
    }

    // All other types require authentication
    if (!isAuthenticated(clientId))
    {
        auto username = j.value(msg::key::Username, "guest" + clientId);
        auto msg = msg::makeAuthResponse(msg::value::False, "unauthenticated", clientId, username);
        sendTo(clientId, msg);
        return;
    }

    // Router: overwrite from with server-trusted clientId
    j[msg::key::From] = clientId;

    // In SignalingServer::onMessage (after auth checks)
    if (type == msg::signaling::PeerDisconnect)
    {
        // Optional: verify this clientId is GM
        // if (!isFromGM(clientId)) return;

        // Overwrite from for trust (already done in your router)
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

    auto closer = [](auto& map) {
        for (auto& [id, ws] : map) {
            if (!ws) continue;
            try {
                ws->onOpen(nullptr);
                ws->onMessage(nullptr);
                ws->onClosed(nullptr);
                ws->onError(nullptr);
                ws->close();
            } catch (...) {
                Logger::instance().log("main", Logger::Level::Warn, "WS close failed for " + id);
            }
        }
        map.clear();
    };

    closer(auth);
    closer(pend);
}

/*void SignalingServer::disconnectAllClients()
{
    for (auto& [id, ws] : authClients_)
    {
        if (ws)
        {
            try
            {
                ws->close();
            }
            catch (...)
            {
            }
        }
    }
    authClients_.clear();
    // You may also want to clear pendingClients_
    for (auto& [id, ws] : pendingClients_)
    {
        if (ws)
        {
            try
            {
                ws->close();
            }
            catch (...)
            {
            }
        }
    }
    pendingClients_.clear();
}*/

//void SignalingServer::prunePending() {
//    if (pendingTimeout_.count() <= 0) return;
//    const auto now = Clock::now();
//    std::vector<std::string> toDrop;
//    toDrop.reserve(pendingClients_.size());
//    for (auto& [id, since] : pendingSince_) {
//        if (now - since > pendingTimeout_) toDrop.push_back(id);
//    }
//    for (auto& id : toDrop) {
//        if (auto it = pendingClients_.find(id); it != pendingClients_.end() && it->second) {
//            it->second->close(); // optional
//        }
//        pendingClients_.erase(id);
//        //pendingSince_.erase(id);
//        std::cout << "[SignalingServer] Dropped pending (timeout): " << id << "\n";
//    }
//}

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
        it->second->close();
    }
}

//
//void SignalingServer::start(unsigned short port) {
//    rtc::WebSocketServerConfiguration serverConfiguration;
//    serverConfiguration.bindAddress = "0.0.0.0";
//    serverConfiguration.port = port;
//    serverConfiguration.connectionTimeout = std::chrono::milliseconds(0);
//    server = std::make_shared<rtc::WebSocketServer>(serverConfiguration);
//
//    server->onClient([this](std::shared_ptr<rtc::WebSocket> client) {
//        auto addrOpt = client->remoteAddress();
//        if (!addrOpt) { std::cout << "[SignalingServer] Client connected\n"; return; }
//        std::string client_id = *addrOpt;
//        std::cout << "[SignalingServer] Client Connected at " << client_id << "\n";
//
//        this->onConnect(client_id, client);
//
//        client->onMessage([this, client_id](std::variant<rtc::binary, rtc::string> msg) {
//            if (!std::holds_alternative<rtc::string>(msg)) return;
//            const auto& s = std::get<rtc::string>(msg);
//            this->onMessage(client_id, s);
//            });
//
//        client->onError([client_id](std::string error) {
//            std::cout << "[SignalingServer] Client Error: " << error << " (" << client_id << ")\n";
//            });
//
//        client->onClosed([this, client_id]() {
//            std::cout << "[SignalingServer] Client disconnected: " << client_id << "\n";
//            if (auto nm = network_manager.lock()) {
//                nm->removeClient(client_id);
//            }
//            else {
//                throw std::runtime_error("NetworkManager expired");
//            }
//            });
//        });
//
//    std::cout << "[SignalingServer] Listening at ws://0.0.0.0:" << port << "\n";
//}

//void SignalingServer::stop() {
//    if (server) { server->stop(); server.reset(); }
//    if (auto nm = network_manager.lock()) {
//        nm->clearPendingClients();
//        nm->clearClients();
//    }
//    else {
//        throw std::runtime_error("SignalingServer::stop: NetworkManager expired");
//    }
//}

//void SignalingServer::onConnect(std::string client_id, std::shared_ptr<rtc::WebSocket> client)
//{
//    if (auto nm = network_manager.lock()) {
//        nm->addPendingClient(client_id, client);
//    }
//    else {
//        throw std::runtime_error("SignalingServer::onConnect: NetworkManager expired");
//    }
//}
//
//void SignalingServer::onMessage(const std::string client_id, const std::string msg) {
//    auto nm = network_manager.lock();
//    if (!nm) throw std::runtime_error("NetworkManager expired");
//
//    auto& clients = nm->getClients();
//
//    nlohmann::json j;
//    try { j = nlohmann::json::parse(msg); }
//    catch (...) { return; }
//
//    // Server may auto-respond to ping if you use it
//    if (j.value("type", "") == "ping") {
//        if (clients.count(client_id)) clients[client_id]->send(R"({"type":"pong"})");
//        return;
//    }
//
//    const bool broadcast = j.value("broadcast", false);
//    if (broadcast) {
//        for (auto& [id, ws] : clients) {
//            if (id == client_id) continue; // don't echo to sender
//            if (ws && !ws->isClosed()) ws->send(msg);
//        }
//    }
//    else {
//        const std::string to = j.value("to", "");
//        auto it = clients.find(to);
//        if (it != clients.end() && it->second && !it->second->isClosed()) {
//            it->second->send(msg);
//        }
//    }
//}

//void SignalingServer::onSignal(const std::string& msg) {
//try {
//    auto j = nlohmann::json::parse(msg);

//    std::string type = j.value("type", "");
//    std::string clientId = j.value("clientId", ""); // or use ws pointer id

//    // Find client in list
//    auto it = std::find_if(clients.begin(), clients.end(),
//        [&](const ClientInfo& c) { return c.id == clientId; });

//    // If client not found, add it
//    if (it == clients.end()) {
//        clients.push_back({ clientId, false });
//        it = std::prev(clients.end());
//    }

//    // --- Password Authentication ---
//    if (type == "auth" && !it->authenticated) {
//        std::string password = j.value("password", "");
//        if (password == "") {
//            it->authenticated = true;
//            // Optionally send a success response
//        }
//        else {
//            // Invalid password → close connection
//            it->authenticated = false;
//            // close client WS here
//            // e.g., ws->close();
//        }
//        return; // do not process further until authenticated
//    }
//    // --- Other message types ---
//    if (!it->authenticated) {
//        // drop or ignore any non-auth messages from unauthenticated clients
//        return;
//    }

//    if (type == "offer") {
//        // handle WebRTC offer
//    }
//    else if (type == "answer") {
//        // handle WebRTC answer
//    }
//    else if (type == "ice") {
//        // handle ICE candidate
//    }

//}
//catch (const std::exception& e) {
//    // parsing failed → ignore or close client
//}
//}

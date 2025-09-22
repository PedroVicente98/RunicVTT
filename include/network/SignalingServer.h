#pragma once
#include <string>
#include <functional>
#include "rtc/rtc.hpp"
#include <nlohmann/json.hpp>

// Forward declare to avoid circular include
class NetworkManager;

class SignalingServer {
public:
    SignalingServer(std::weak_ptr<NetworkManager> parent);
    ~SignalingServer();
    void start(unsigned short port);
    void stop();

    // Router/API
    void onConnect(std::string clientId, std::shared_ptr<rtc::WebSocket> client);
    void onMessage(const std::string& clientId, const std::string& text);
    void sendTo(const std::string& clientId, const std::string& message);
    void broadcast(const std::string& message); 

    // Config
    void setPendingAuthTimeout(std::chrono::seconds s) { pendingTimeout_ = s; }


    // housekeeping
    void prunePending(); // drop pending clients older than timeout
    void moveToAuthenticated(const std::string& clientId);
    bool isAuthenticated(const std::string& clientId) const;


private:
    std::shared_ptr<rtc::WebSocketServer> server;
    std::weak_ptr<NetworkManager> network_manager;

    // Pending: not authenticated yet
    std::unordered_map<std::string, std::shared_ptr<rtc::WebSocket>> pendingClients_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> pendingSince_;
    std::unordered_map<std::string, std::string> pendingUsernames_;

    // Authenticated clients (only these receive routed messages)
    std::unordered_map<std::string, std::string> authUsernames_;
    std::unordered_map<std::string, std::shared_ptr<rtc::WebSocket>> authClients_;
    std::chrono::seconds pendingTimeout_{ 60 }; // default: 60s to auth

};

#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "PeerLink.h"
#include "SignalingServer.h"
#include "SignalingClient.h"
#include "flecs.h"
#include "rtc/configuration.hpp"

enum class Role { GAMEMASTER, PLAYER };

class NetworkManager {
public:
    NetworkManager(flecs::world ecs);
    ~NetworkManager();

    void startServer(unsigned short port);
    void closeServer();
    bool connectToPeer(const std::string& connectionString);

private:
    flecs::world ecs;


    void connectToWebRTCPeer(const std::string& peerId);
    void receiveSignal(const std::string& peerId, const std::string& message);
    void sendSignalToPeer(const std::string& peerId, const std::string& message);

    Role peer_role;

    std::unordered_map<std::string, std::shared_ptr<PeerLink>> peers;

    std::unique_ptr<SignalingServer> signalingServer;
    std::unique_ptr<SignalingClient> signalingClient;

    rtc::Configuration rtcConfig;

};



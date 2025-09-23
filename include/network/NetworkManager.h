#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "PeerLink.h"
#include "flecs.h"
#include "rtc/configuration.hpp"
#include "imgui.h"
#include <regex>
#include <vector>
#include <rtc/peerconnection.hpp>
#include <nlohmann/json.hpp>
#include <future>
#include <iostream>

enum class Role {
    NONE,
    GAMEMASTER,
    PLAYER 
};

enum class ConnectionType {
    EXTERNAL,
    LOCAL,
    LOCALTUNNEL
};

// Forward declare
class SignalingServer;
class SignalingClient;

class NetworkManager : public std::enable_shared_from_this<NetworkManager>  {
public:
    NetworkManager(flecs::world ecs);

    void setup();

    ~NetworkManager();

    void startServer(ConnectionType mode, unsigned short port, bool tryUpnp);
    void startServer(std::string internal_ip_address, unsigned short port);
    void closeServer();
    bool connectToPeer(const std::string& connectionString);

    bool disconectFromPeers();

    void allowPort(unsigned int port);
    void disallowPort(unsigned short port);

    // Utility methods
    std::string getNetworkInfo(ConnectionType type);     // Get network info (IP and port) (server utility)
    std::string getLocalIPAddress();  // Get local IP address (server utility)
    std::string getExternalIPAddress();  // Get external IP address (server utility)
    std::string getLocalTunnelURL();  // Get Local Tunnel URL

    void parseConnectionString(std::string connection_string,std::string& server, unsigned short& port,std::string& password);
    unsigned short getPort() const { return port; };
    std::string getNetworkPassword() const { return std::string(network_password); };
    void setPort(unsigned int port) { this->port = port; }
    void setNetworkPassword(const char* password);
    Role getPeerRole() const { return peer_role; }
    void ShowPortForwardingHelpPopup(bool* p_open);
    rtc::Configuration getRTCConfig() const { return rtcConfig; }

    ////Operations
    //void addClient(std::string client_id, std::shared_ptr<rtc::WebSocket> ws);
    //void removeClient(std::string client_id);
    //void clearClients() const { clients.empty(); }
    //std::shared_ptr<rtc::WebSocket> getClient(std::string client_id);
    //std::unordered_map<std::string, std::shared_ptr<rtc::WebSocket>>& getClients() { return clients; }
    //void addPendingClient(std::string client_id, std::shared_ptr<rtc::WebSocket> ws);
    //void removePendingClient(std::string client_id);
    //void clearPendingClients() const { pending_clients.empty(); }
    //std::shared_ptr<rtc::WebSocket> getPendingClient(std::string client_id);


    bool removePeer(std::string peerId);
    bool clearPeers() const { return peers.empty(); }
    void disconnectAllPeers();
    std::size_t removeDisconnectedPeers();

    // PeerLink -> NM (send via signaling)
    void onPeerLocalDescription(const std::string& peerId, const rtc::Description& desc);
    void onPeerLocalCandidate(const std::string& peerId, const rtc::Candidate& cand);
    std::shared_ptr<PeerLink> ensurePeerLink(const std::string& peerId);

    // NetworkManager.h
    std::string getMyUsername() const { return myUsername_; }; // you decide how to set it (UI input etc.)
    void setMyIdentity(std::string myId, std::string username);
    void upsertPeerIdentity(const std::string& id, const std::string& username);
    std::string displayNameFor(const std::string& id) const;

    // expose read-only view to peers for UI
    const std::unordered_map<std::string, std::shared_ptr<PeerLink>>& getPeers() const { return peers; }
    // expose server pointer for UI
    std::shared_ptr<SignalingServer> getSignalingServer() const { return signalingServer; }

private:

    // fields
    std::string myClientId_;
    std::string myUsername_;
    std::unordered_map<std::string, std::string> peerUsernames_;
    std::unordered_map<std::string, std::string> clientUsernames_;

    flecs::world ecs;
    unsigned int port = 8080;
    char network_password[124] = "\0";
    std::string external_ip_address = "";
    std::string local_ip_address = "";
    Role peer_role;

    rtc::Configuration rtcConfig;
    std::shared_ptr<SignalingServer> signalingServer;
    std::shared_ptr<SignalingClient> signalingClient;
    std::unordered_map<std::string, std::shared_ptr<PeerLink>> peers;


    static bool hasUrlScheme(const std::string& s) {
        auto starts = [&](const char* p) { return s.rfind(p, 0) == 0; };
        return starts("https://") || starts("http://") || starts("wss://") || starts("ws://");
    }


};



    ////Callback Methods
    ////WebSocker Client
    //void onOpenClient();                               // WebSocket connected to master
    //void onCloseClient();                              // WebSocket closed
    //void onErrorClient(const std::string& err);        // WebSocket error
    //void onMessageClient(const std::string& msg);      // Message from master server (offers, answers, ICE, etc.)

    ////Websocket Server
    //void onSignal(std::string clientId, const std::string& msg);
    //void onMessage(std::string clientId, const std::string& msg);
    //void onConnect(std::string peer_id, std::shared_ptr<rtc::WebSocket> client);

    //// --- Signaling Server (if this peer is master) ---
    //void onOpenServerClient(int clientId);                      // A client connects to our WebSocket server
    //void onCloseServerClient(int clientId);                     // A client disconnects
    //void onErrorServerClient(int clientId, const std::string&); // Error from one client
    //void onMessageServerClient(int clientId, const std::string& msg); // Msg from a connected client

    ////PeerLink
    //void onOpenPeer(std::string clientId, const std::string& msg);
    //void onMessagePeer(std::string clientId, const std::string& msg);
    //void onStateChange(rtc::PeerConnection::State state);
    //void onLocalDescription(rtc::Description desc);
    //void onLocalDescription(rtc::Candidate candidate);

    //// --- Utility/Housekeeping ---
    //void onDisconnectedPeer(int peerId); // higher-level handler when peer lost
    //void connectToWebRTCPeer(const std::string& peerId);
    //void receiveSignal(const std::string& peerId, const std::string& message);
    //void sendSignalToPeer(const std::string& peerId, const std::string& message);

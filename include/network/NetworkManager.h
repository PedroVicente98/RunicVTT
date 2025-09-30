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
#include "MessageQueue.h"
#include "Components.h"

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

struct NetworkToast {
    enum class Level { Info, Good, Warning, Error };
    std::string message;
    double      expiresAt; // ImGui::GetTime() + duration
    Level       level;
};

struct InboundMsg {
    std::string fromPeer;                // who sent
    std::string label;                   // DC label (e.g., msg::dc::name::Game)
    std::vector<uint8_t> bytes;    // payload
};

struct OutboundMsg {
    std::string toPeer;                  // empty = broadcast
    std::string label;                   // DC label
    std::vector<uint8_t> bytes;    // payload
};


// Forward declare
class SignalingServer;
class SignalingClient;
class BoardManager;

enum class ImageOwnerKind : uint8_t { Board, Marker };

struct MarkerMeta {
    uint64_t markerId = 0;
    uint64_t boardId = 0;
    std::string name;             // filename/hint if you want
    Position pos{};
    Size size{};
    Visibility vis{};
    Moving mov{};
};

struct BoardMeta {
    uint64_t boardId = 0;
    std::string boardName;
    Panning pan{};
    Grid grid{};
    Size size{};
};

struct PendingImage {
    ImageOwnerKind kind{};
    uint64_t id = 0;              // boardId when kind=Board, markerId when kind=Marker
    uint64_t boardId = 0;         // only used when kind=Marker; for Board can be same as id or 0
    std::string name;             // optional filename/path hint

    // optional meta to finalize on commit:
    std::optional<MarkerMeta> markerMeta;
    std::optional<BoardMeta>  boardMeta;

    uint64_t total = 0;
    uint64_t received = 0;
    std::vector<uint8_t> buf;

    bool isComplete() const { return total == received && total > 0; }
};



class NetworkManager : public std::enable_shared_from_this<NetworkManager>  {
public:
    NetworkManager(flecs::world ecs);

    void setup(std::weak_ptr<BoardManager> board_manager);

    ~NetworkManager();

    void startServer(ConnectionType mode, unsigned short port, bool tryUpnp);
    void startServer(std::string internal_ip_address, unsigned short port);
    void closeServer();
    
    // if not, call this from main thread; otherwise add a mutex)
    bool isConnected();

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

    bool connectToPeer(const std::string& connectionString);
    bool disconectFromPeers();
    bool removePeer(std::string peerId);
    bool clearPeers() const { return peers.empty(); }
    bool disconnectAllPeers();
    std::size_t removeDisconnectedPeers();
    void broadcastPeerDisconnect(const std::string& targetId);

    // PeerLink -> NM (send via signaling)
    void onPeerLocalDescription(const std::string& peerId, const rtc::Description& desc);
    void onPeerLocalCandidate(const std::string& peerId, const rtc::Candidate& cand);
    std::shared_ptr<PeerLink> ensurePeerLink(const std::string& peerId);

    std::string getMyUsername() const { return myUsername_; };
    void setMyIdentity(std::string myId, std::string username);
    void upsertPeerIdentity(const std::string& id, const std::string& username);
    std::string displayNameFor(const std::string& id) const;

    const std::unordered_map<std::string, std::shared_ptr<PeerLink>>& getPeers() const { return peers; }
    std::shared_ptr<SignalingServer> getSignalingServer() const { return signalingServer; }


    // Toaster Notification;
    void pushStatusToast(const std::string& msg, NetworkToast::Level lvl, double durationSec = 3.0);
    const std::deque<NetworkToast>& toasts() const { return toasts_; }
    void pruneToasts(double now);

    void queueMessage(OutboundMsg msg);
    bool tryDequeueOutbound(OutboundMsg& msg);

    void onDcGameBinary(const std::string& fromPeer, const std::vector<uint8_t>& b);

    bool sendMarkerCreate(const std::string& to, uint64_t markerId, const std::vector<uint8_t>& img, const std::string& name);
    bool sendBoardCreate(const std::string& to, uint64_t boardId, const std::vector<uint8_t>& img, const std::string& name);

    static constexpr size_t kChunk = 16 * 1024; // or smaller if needed
private:

    void handleBoardMeta(const std::vector<uint8_t>& b, size_t& off);
    void handleMarkerMeta(const std::vector<uint8_t>& b, size_t& off);
    void handleFogCreate(const std::vector<uint8_t>& b, size_t& off);
    void handleImageChunk(const std::vector<uint8_t>& b, size_t& off);
    void handleCommitBoard(const std::vector<uint8_t>& b, size_t& off);
    void handleCommitMarker(const std::vector<uint8_t>& b, size_t& off);

    void createBoardFromBytesAndMeta(const BoardMeta& bm, const std::vector<uint8_t>& bytes);
    void createFogFromMeta(uint64_t fogId, const Position& pos, const Size& size, const Visibility& vis, flecs::entity parentBoard);
    void createMarkerFromBytesAndMeta(const MarkerMeta& mm, const std::vector<uint8_t>& bytes, flecs::entity parentBoard);

    // frame builders
    std::vector<uint8_t> buildSnapshotGameTableFrame(uint64_t gameTableId, const std::string& name);
    std::vector<uint8_t> buildSnapshotBoardFrame(const flecs::entity& board, uint64_t imageBytesTotal);
    std::vector<uint8_t> buildCreateMarkerFrame(uint64_t boardId, const flecs::entity& marker, uint64_t imageBytesTotal);
    std::vector<uint8_t> buildFogCreateFrame(uint64_t boardId, const flecs::entity& fog);
    std::vector<uint8_t> buildImageChunkFrame(uint8_t ownerKind, uint64_t id, uint64_t offset, const uint8_t* data, size_t len);
    std::vector<uint8_t> buildCommitBoardFrame(uint64_t boardId);
    std::vector<uint8_t> buildCommitMarkerFrame(uint64_t boardId, uint64_t markerId);

    std::vector<std::string> getConnectedPeerIds() const;

    void broadcastGameTable(const flecs::entity& gameTable);
    void broadcastBoard(const flecs::entity& board);
    void broadcastMarker(uint64_t boardId, const flecs::entity& marker);
    void broadcastFog(uint64_t boardId, const flecs::entity& fog);

    void sendGameTo(const std::string& peerId, const std::vector<unsigned char>& bytes);
    void broadcastFrame(const std::vector<unsigned char>& frame, const std::vector<std::string>& toPeerIds);

    void sendGameTable(const flecs::entity& gameTable, const std::vector<std::string>& toPeerIds);
    void sendBoard(const flecs::entity& board, const std::vector<std::string>& toPeerIds);
    void sendMarker(uint64_t boardId, const flecs::entity& marker, const std::vector<std::string>& toPeerIds);
    void sendFog(uint64_t boardId, const flecs::entity& fog, const std::vector<std::string>& toPeerIds);

    flecs::entity findBoardById(uint64_t boardId);
    std::deque<NetworkToast> toasts_;
    std::unordered_map<uint64_t, PendingImage> imagesRx_; 

    MessageQueue<InboundMsg>  inbound_;
    MessageQueue<OutboundMsg> outbound_; 

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
    std::weak_ptr<BoardManager> board_manager;

    static bool hasUrlScheme(const std::string& s) {
        auto starts = [&](const char* p) { return s.rfind(p, 0) == 0; };
        return starts("https://") || starts("http://") || starts("wss://") || starts("ws://");
    }

    static inline bool ensureRemaining(const std::vector<uint8_t>& b, size_t off, size_t need) {
        return off + need <= b.size();
    }

    inline std::vector<unsigned char> readFileBytes(const std::string& path) {
        std::ifstream file(path, std::ios::binary);
        if (!file) {
            return {}; // return empty if file not found
        }
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);
    
        std::vector<unsigned char> buffer(size);
        if (size > 0) {
            file.read(reinterpret_cast<char*>(buffer.data()), size);
        }
        return buffer;
    }

};



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

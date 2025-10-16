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
#include "Message.h"
#include <fstream>
#include "ImGuiToaster.h"
#include "PathManager.h"
#include "Logger.h"

enum class Role
{
    NONE,
    GAMEMASTER,
    PLAYER
};

enum class ConnectionType
{
    EXTERNAL,
    LOCAL,
    LOCALTUNNEL
};

struct MoveLatest
{
    uint64_t boardId = 0;
    uint32_t seq = 0;
    glm::vec2 pos = {};
    bool dragging = false;
    bool have = false;
};

// Forward declare
class SignalingServer;
class SignalingClient;
class BoardManager;
class GameTableManager;

struct PendingImage
{
    msg::ImageOwnerKind kind{};
    uint64_t id = 0;      // boardId when kind=Board, markerId when kind=Marker
    uint64_t boardId = 0; // only used when kind=Marker; for Board can be same as id or 0
    std::string name;     // optional filename/path hint

    // optional meta to finalize on commit:
    std::optional<msg::MarkerMeta> markerMeta;
    std::optional<msg::BoardMeta> boardMeta;

    uint64_t total = 0;
    uint64_t received = 0;
    std::vector<uint8_t> buf;

    bool commitRequested = false;

    bool isComplete() const
    {
        return total == received && total > 0;
    }
};

class NetworkManager : public std::enable_shared_from_this<NetworkManager>
{
public:
    NetworkManager(flecs::world ecs);

    void setup(std::weak_ptr<BoardManager> board_manager, std::weak_ptr<GameTableManager> gametable_manager);

    ~NetworkManager();

    void startServer(ConnectionType mode, unsigned short port, bool tryUpnp);
    void startServer(std::string internal_ip_address, unsigned short port);
    void closeServer();

    // if not, call this from main thread; otherwise add a mutex)
    bool isConnected();
    bool isHosting() const;
    bool hasAnyPeer() const;
    int connectedPeerCount() const;

    void allowPort(unsigned int port);
    void disallowPort(unsigned short port);

    // Utility methods
    std::string getNetworkInfo(ConnectionType type); // Get network info (IP and port) (server utility)
    std::string getLocalIPAddress();                 // Get local IP address (server utility)
    std::string getExternalIPAddress();              // Get external IP address (server utility)
    std::string getLocalTunnelURL();                 // Get Local Tunnel URL

    void parseConnectionString(std::string connection_string, std::string& server, unsigned short& port, std::string& password);
    unsigned short getPort() const
    {
        return port;
    };
    std::string getNetworkPassword() const
    {
        return std::string(network_password);
    };
    void setPort(unsigned int port)
    {
        this->port = port;
    }
    void setNetworkPassword(const char* password);
    Role getPeerRole() const
    {
        return peer_role;
    }
    void ShowPortForwardingHelpPopup(bool* p_open);
    rtc::Configuration getRTCConfig() const
    {
        return rtcConfig;
    }

    bool connectToPeer(const std::string& connectionString);
    bool disconectFromPeers();
    bool removePeer(std::string peerId);
    bool clearPeers() const
    {
        return peers.empty();
    }
    bool disconnectAllPeers();
    std::size_t removeDisconnectedPeers();
    void broadcastPeerDisconnect(const std::string& targetId);
    // Single-shot reconnect of one peer. Returns true if we kicked off a rebuild.
    /*bool reconnectPeer(const std::string& peerId);*/

    // PeerLink -> NM (send via signaling)
    void onPeerLocalDescription(const std::string& peerId, const rtc::Description& desc);
    void onPeerLocalCandidate(const std::string& peerId, const rtc::Candidate& cand);
    std::shared_ptr<PeerLink> ensurePeerLink(const std::string& peerId);

    std::string getMyUsername() const
    {
        return myUsername_;
    };

    std::string getMyId() const
    {
        return myClientId_;
    };
    void setMyIdentity(std::string myId, std::string username);
    void upsertPeerIdentity(const std::string& id, const std::string& username);
    std::string displayNameFor(const std::string& id) const;

    const std::unordered_map<std::string, std::shared_ptr<PeerLink>>& getPeers() const
    {
        return peers;
    }
    std::shared_ptr<SignalingServer> getSignalingServer() const
    {
        return signalingServer;
    }

    bool tryPopReadyMessage(msg::ReadyMessage& out)
    {
        return inboundGame_.try_pop(out);
    }

    bool tryPopRawMessage(msg::InboundRaw& out)
    {
        return inboundRaw_.try_pop(out);
    }

    void drainEvents();
    void drainInboundRaw(int maxPerTick);

    void decodeRawGameBuffer(const std::string& fromPeer, const std::vector<uint8_t>& b);
    void decodeRawChatBuffer(const std::string& fromPeer, const std::vector<uint8_t>& b);
    void decodeRawNotesBuffer(const std::string& fromPeer, const std::vector<uint8_t>& b);
    void decodeRawMarkerMoveBuffer(const std::string& fromPeer, const std::vector<uint8_t>& b);

    void startRawDrainWorker();
    void stopRawDrainWorker();

    void flushCoalescedMoves();
    //void onDcGameBinary(const std::string& fromPeer, const std::vector<uint8_t>& b);
    //void onDcChatBinary(const std::string& fromPeer, const std::vector<uint8_t>& b);
    //void onDcNotesBinary(const std::string& fromPeer, const std::vector<uint8_t>& b);

    void onPeerChannelOpen(const std::string& peerId, const std::string& label);
    void bootstrapPeerIfReady(const std::string& peerId);

    void broadcastGameTable(const flecs::entity& gameTable);
    void broadcastBoard(const flecs::entity& board);
    void broadcastMarker(uint64_t boardId, const flecs::entity& marker);
    void broadcastFog(uint64_t boardId, const flecs::entity& fog);

    void broadcastFogUpdate(uint64_t boardId, const flecs::entity& fog);
    void broadcastFogDelete(uint64_t boardId, const flecs::entity& fog);
    void broadcastMarkerUpdate(uint64_t boardId, const flecs::entity& marker);
    void broadcastMarkerDelete(uint64_t boardId, const flecs::entity& marker);

    void broadcastMarkerMove(uint64_t boardId, const flecs::entity& marker);
    void sendMarkerMove(uint64_t boardId, const flecs::entity& marker, const std::vector<std::string>& toPeerIds);
   
    void broadcastMarkerMoveState(uint64_t boardId, const flecs::entity& marker);
    void sendMarkerMoveState(uint64_t boardId, const flecs::entity& marker, const std::vector<std::string>& toPeerIds);

    void sendMarkerUpdate(uint64_t boardId, const flecs::entity& marker, const std::vector<std::string>& toPeerIds);
    void sendMarkerDelete(uint64_t boardId, const flecs::entity& marker, const std::vector<std::string>& toPeerIds);
    void sendFogUpdate(uint64_t boardId, const flecs::entity& fog, const std::vector<std::string>& toPeerIds);
    void sendFogDelete(uint64_t boardId, const flecs::entity& fog, const std::vector<std::string>& toPeerIds);

    void sendGameTo(const std::string& peerId, const std::vector<unsigned char>& bytes);
    void broadcastGameFrame(const std::vector<unsigned char>& frame, const std::vector<std::string>& toPeerIds);

    void sendGameTable(const flecs::entity& gameTable, const std::vector<std::string>& toPeerIds);
    void sendBoard(const flecs::entity& board, const std::vector<std::string>& toPeerIds);
    void sendMarker(uint64_t boardId, const flecs::entity& marker, const std::vector<std::string>& toPeerIds);
    void sendFog(uint64_t boardId, const flecs::entity& fog, const std::vector<std::string>& toPeerIds);

    bool sendImageChunks(msg::ImageOwnerKind kind, uint64_t id, const std::vector<unsigned char>& img, const std::vector<std::string>& toPeerIds);

    void broadcastChatThreadFrame(msg::DCType t, const std::vector<uint8_t>& payload);
    void sendChatThreadFrameTo(const std::set<std::string>& peers, msg::DCType t, const std::vector<uint8_t>& payload);

    void setToaster(std::shared_ptr<ImGuiToaster> t)
    {
        toaster_ = t;
    }
    // Unified push (replaces your old pushStatusToast)
    void pushStatusToast(const std::string& msg, ImGuiToaster::Level lvl, float durationSec = 5.0f)
    {
        if (toaster_)
            toaster_->Push(lvl, msg, durationSec);
    }

    MessageQueue<msg::NetEvent> events_;
    MessageQueue<msg::InboundRaw> inboundRaw_;
    std::vector<std::string> getConnectedPeerIds() const;

    // Called from local UI when we start/stop dragging.
    void markDraggingLocal(uint64_t markerId, bool dragging);

    // Called from inbound decode/apply paths.
    void noteDraggingRemote(uint64_t markerId, const std::string& peerId, bool dragging);

    // Single check for BoardManager::canMoveMarker()
    bool isMarkerBeingDragged(uint64_t markerId) const;

    // Optional helper if you want to allow self-drag to pass the check
    bool amIDragging(uint64_t markerId) const;
    // GM identity
    void setGMId(const std::string& id) { gmPeerId_ = id; }
    const std::string& getGMId() const { return gmPeerId_; }

private:
    std::string gmPeerId_;

    std::unordered_map<uint64_t, PendingImage> imagesRx_;
    MessageQueue<msg::ReadyMessage> inboundGame_;
    // optional background raw-drain worker
    std::atomic<bool> rawWorkerRunning_{false};
    std::atomic<bool> rawWorkerStop_{false};
    std::thread rawWorker_;
    // NetworkManager.h
    std::chrono::steady_clock::time_point lastMoveFlush_{std::chrono::steady_clock::now()};

    std::mutex moveMtx_;
    std::unordered_map<uint64_t, MoveLatest> moveLatest_; // markerId -> latest
    std::unordered_map<uint64_t, uint32_t> moveSeq_;      // markerId -> last seq
    mutable std::mutex dragMtx_;
    // markerId -> peerId currently dragging (myId when it's me)
    std::unordered_map<uint64_t, std::string> dragging_;
    std::shared_ptr<ImGuiToaster> toaster_;

    static constexpr size_t kChunk = 8 * 1024; // 8KB chunk
    //static constexpr size_t kHighWater = 2 * 1024 * 1024; // 2 MB queued
    //static constexpr size_t kLowWater = 512 * 1024;       // 512 KB before resuming
    static constexpr int kPaceEveryN = 48; // crude pacing step
    static constexpr int kPaceMillis = 2;

    void handleGameTableSnapshot(const std::vector<uint8_t>& b, size_t& off);
    void handleBoardMeta(const std::vector<uint8_t>& b, size_t& off);
    void handleMarkerMeta(const std::vector<uint8_t>& b, size_t& off);
    void handleFogCreate(const std::vector<uint8_t>& b, size_t& off);
    void handleImageChunk(const std::vector<uint8_t>& b, size_t& off);
    void handleCommitBoard(const std::vector<uint8_t>& b, size_t& off);
    void handleCommitMarker(const std::vector<uint8_t>& b, size_t& off);

    // MarkerUpdate
    void handleMarkerMove(const std::vector<uint8_t>& b, size_t& off);
    void handleMarkerUpdate(const std::vector<uint8_t>& b, size_t& off);
    void handleMarkerDelete(const std::vector<uint8_t>& b, size_t& off);
    // FogUpdate
    void handleFogUpdate(const std::vector<uint8_t>& b, size_t& off);
    void handleFogDelete(const std::vector<uint8_t>& b, size_t& off);

    void tryFinalizeImage(msg::ImageOwnerKind kind, uint64_t id);
    // frame builders
    std::vector<uint8_t> buildSnapshotGameTableFrame(uint64_t gameTableId, const std::string& name);
    std::vector<uint8_t> buildSnapshotBoardFrame(const flecs::entity& board, uint64_t imageBytesTotal);
    std::vector<uint8_t> buildCreateMarkerFrame(uint64_t boardId, const flecs::entity& marker, uint64_t imageBytesTotal);
    std::vector<uint8_t> buildFogCreateFrame(uint64_t boardId, const flecs::entity& fog);
    std::vector<uint8_t> buildImageChunkFrame(uint8_t ownerKind, uint64_t id, uint64_t offset, const uint8_t* data, size_t len);
    std::vector<uint8_t> buildCommitBoardFrame(uint64_t boardId);
    std::vector<uint8_t> buildCommitMarkerFrame(uint64_t boardId, uint64_t markerId);

    // ---- MARKER UPDATE/DELETE ----
    std::vector<unsigned char> buildMarkerMoveFrame(uint64_t boardId, const flecs::entity& marker, uint32_t seq, bool dragging);
    std::vector<unsigned char> buildMarkerUpdateFrame(uint64_t boardId, const flecs::entity& marker, bool isPlayerOp);
    std::vector<unsigned char> buildMarkerDeleteFrame(uint64_t boardId, uint64_t markerId);
    // ---- FOG UPDATE/DELETE ----
    std::vector<unsigned char> buildFogUpdateFrame(uint64_t boardId, const flecs::entity& fog);
    std::vector<unsigned char> buildFogDeleteFrame(uint64_t boardId, uint64_t fogId);

    // Helpers used by reconnectPeer (thin wrappers around what you already have)
    /*std::shared_ptr<PeerLink> replaceWithFreshLink_(const std::string& peerId);
    void createOfferAndSend_(const std::string& peerId, const std::shared_ptr<PeerLink>& link);
    void createChannelsIfOfferer_(const std::shared_ptr<PeerLink>& link);*/

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
    std::weak_ptr<GameTableManager> gametable_manager;

    static bool hasUrlScheme(const std::string& s)
    {
        auto starts = [&](const char* p)
        { return s.rfind(p, 0) == 0; };
        return starts("https://") || starts("http://") || starts("wss://") || starts("ws://");
    }

    static inline bool ensureRemaining(const std::vector<uint8_t>& b, size_t off, size_t need)
    {
        return off + need <= b.size();
    }

    inline std::vector<unsigned char> readFileBytes(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            return {}; // return empty if file not found
        }
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<unsigned char> buffer(size);
        if (size > 0)
        {
            file.read(reinterpret_cast<char*>(buffer.data()), size);
        }
        return buffer;
    }
    //inline std::vector<unsigned char> readFileBytes(const std::string& path)
    //{
    //    namespace fs = std::filesystem;
    //    std::error_code ec;

    //    auto tryOpen = [](const fs::path& p) -> std::vector<unsigned char>
    //    {
    //        std::ifstream file(p, std::ios::binary);
    //        if (!file)
    //            return {};
    //        file.seekg(0, std::ios::end);
    //        size_t size = static_cast<size_t>(file.tellg());
    //        file.seekg(0, std::ios::beg);
    //        std::vector<unsigned char> buffer(size);
    //        if (size > 0)
    //            file.read(reinterpret_cast<char*>(buffer.data()), size);
    //        return buffer;
    //    };

    //    fs::path p = path;
    //    // 1) Direct (absolute or relative to CWD)
    //    if (auto buf = tryOpen(p); !buf.empty())
    //        return buf;

    //    // 2) Resolve relative under known asset roots
    //    std::vector<fs::path> roots = {
    //        PathManager::getMapsPath(),
    //        PathManager::getMarkersPath(),
    //        fs::path("res"),   // if you ship a res/
    //        fs::current_path() // last-ditch CWD
    //    };

    //    for (const auto& r : roots)
    //    {
    //        fs::path candidate = fs::weakly_canonical(r / p, ec);
    //        if (ec)
    //            continue;
    //        if (auto buf = tryOpen(candidate); !buf.empty())
    //        {
    //            return buf;
    //        }
    //    }

    //    // Log a helpful error once
    //    Logger::instance().log("assets", Logger::Level::Error, "readFileBytes: cannot open '" + path + "' (tried known roots).");
    //    return {};
    //}
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

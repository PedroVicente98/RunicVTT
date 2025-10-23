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
#include "IdentityManager.h"

struct DragState
{
    uint32_t epoch{0};
    bool closed{true};
    uint32_t lastSeq{0};
    std::string ownerPeerId;
    bool locallyDragging{false};
    uint32_t locallyProposedEpoch{0};
    uint32_t localSeq{0};
    uint64_t lastTxMs{0};
    uint64_t epochOpenedMs{0};
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
    NetworkManager(flecs::world ecs, std::shared_ptr<IdentityManager> identity_manager);

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
        for (auto [id, peer] : peers)
        {
            peer->close();
        }
        return peers.empty();
    }
    bool disconnectAllPeers();
    std::size_t removeDisconnectedPeers();
    void broadcastPeerDisconnect(const std::string& targetId);
    /*bool reconnectPeer(const std::string& peerId);*/

    void onPeerLocalDescription(const std::string& peerId, const rtc::Description& desc);
    void onPeerLocalCandidate(const std::string& peerId, const rtc::Candidate& cand);
    std::shared_ptr<PeerLink> ensurePeerLink(const std::string& peerId);

    std::string displayNameForPeer(const std::string& peerId) const;

    std::string getMyUsername() const
    {
        return identity_manager->myUsername();
    }
    std::string getMyUniqueId() const
    {
        return identity_manager->myUniqueId();
    }

    const std::string& getMyPeerId() const
    {
        return myPeerId_;
    }
    void setMyPeerId(std::string v)
    {
        myPeerId_ = std::move(v);
    }

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

    void decodeRawChatBuffer(const std::string& fromPeer, const std::vector<uint8_t>& b);
    void decodeRawNotesBuffer(const std::string& fromPeer, const std::vector<uint8_t>& b);

    void startRawDrainWorker();
    void stopRawDrainWorker();

    void onPeerChannelOpen(const std::string& peerId, const std::string& label);
    void bootstrapPeerIfReady(const std::string& peerId);

    void broadcastGameTable(const flecs::entity& gameTable);
    void broadcastBoard(const flecs::entity& board);
    void broadcastFog(uint64_t boardId, const flecs::entity& fog);

    void broadcastFogUpdate(uint64_t boardId, const flecs::entity& fog);
    void broadcastFogDelete(uint64_t boardId, const flecs::entity& fog);

    void sendFogUpdate(uint64_t boardId, const flecs::entity& fog, const std::vector<std::string>& toPeerIds);
    void sendFogDelete(uint64_t boardId, const flecs::entity& fog, const std::vector<std::string>& toPeerIds);

    void sendGameTo(const std::string& peerId, const std::vector<unsigned char>& bytes);
    void broadcastGameFrame(const std::vector<unsigned char>& frame, const std::vector<std::string>& toPeerIds);

    void sendGameTable(const flecs::entity& gameTable, const std::vector<std::string>& toPeerIds);
    void sendBoard(const flecs::entity& board, const std::vector<std::string>& toPeerIds);
    void sendFog(uint64_t boardId, const flecs::entity& fog, const std::vector<std::string>& toPeerIds);

    bool sendImageChunks(msg::ImageOwnerKind kind, uint64_t id, const std::vector<unsigned char>& img, const std::vector<std::string>& toPeerIds);

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
    std::vector<std::string> getConnectedUsernames() const;

    // GM identity
    void setGMId(const std::string& id)
    {
        gmPeerId_ = id;
    }
    const std::string& getGMId() const
    {
        return gmPeerId_;
    }

    void decodeRawGameBuffer(const std::string& fromPeer, const std::vector<uint8_t>& b);

    void sendMarkerDelete(uint64_t boardId, const flecs::entity& marker, const std::vector<std::string>& toPeerIds);
    void broadcastMarkerDelete(uint64_t boardId, const flecs::entity& marker);
    void broadcastMarker(uint64_t boardId, const flecs::entity& marker);
    void sendMarker(uint64_t boardId, const flecs::entity& marker, const std::vector<std::string>& toPeerIds);
    //PUBLIC MARKER STUFF--------------------------------------------------------------------------------

    //END STABLE
    void decodeRawMarkerMoveBuffer(const std::string& fromPeer, const std::vector<uint8_t>& b);

    void markDraggingLocal(uint64_t markerId, bool dragging);
    bool isMarkerBeingDragged(uint64_t markerId) const;
    bool amIDragging(uint64_t markerId) const;
    void forceCloseDrag(uint64_t markerId);

    void broadcastMarkerMove(uint64_t boardId, const flecs::entity& marker);
    void sendMarkerMove(uint64_t boardId, const flecs::entity& marker, const std::vector<std::string>& toPeerIds);

    void broadcastMarkerMoveState(uint64_t boardId, const flecs::entity& marker);
    void sendMarkerMoveState(uint64_t boardId, const flecs::entity& marker, const std::vector<std::string>& toPeerIds);

    void broadcastMarkerUpdate(uint64_t boardId, const flecs::entity& marker);
    void sendMarkerUpdate(uint64_t boardId, const flecs::entity& marker, const std::vector<std::string>& toPeerIds);

    bool shouldApplyMarkerMove(const msg::ReadyMessage& m);           // DCType::MarkerMove
    bool shouldApplyMarkerMoveStateStart(const msg::ReadyMessage& m); // DCType::MarkerMoveState + mov.isDragging==true
    bool shouldApplyMarkerMoveStateFinal(const msg::ReadyMessage& m); // DCType::MarkerMoveState + mov.isDragging==false

    // send/broadcast
    void broadcastGridUpdate(uint64_t boardId, const flecs::entity& board);
    void sendGridUpdate(uint64_t boardId, const flecs::entity& board, const std::vector<std::string>& toPeerIds);

    //PUBLIC END MARKER STUFF----------------------------------------------------------------------------

    void buildUserNameUpdate(std::vector<uint8_t>& out,
                             uint64_t tableId,
                             const std::string& userUniqueId,
                             const std::string& oldUsername,
                             const std::string& newUsername,
                             bool reboundFlag) const;

    void broadcastUserNameUpdate(const std::vector<uint8_t>& payload); // send on Game DC to all
    void sendUserNameUpdateTo(const std::string& peerId,               // direct (rare)
                              const std::vector<uint8_t>& payload);

    void upsertPeerIdentityWithUnique(const std::string& peerId,
                                      const std::string& uniqueId,
                                      const std::string& username);

    // NetworkManager.h (public)
    bool broadcastChatJson(const msg::Json& j);
    bool sendChatJsonTo(const std::string& peerId, const msg::Json& j);
    bool sendChatJsonTo(const std::set<std::string>& peers, const msg::Json& j);
    std::shared_ptr<IdentityManager> getIdentityManager()
    {
        return identity_manager;
    }

    std::unordered_map<uint64_t /*markerId*/, DragState> drag_;
    // NetworkManager.h
    std::string debugIdentitySnapshot() const;
    void clearDragState(uint64_t markerId);
    void housekeepPeers();

private:
    // build
    std::vector<unsigned char> buildGridUpdateFrame(uint64_t boardId, const Grid& grid);
    // handle
    void handleGridUpdate(const std::vector<uint8_t>& b, size_t& off);

    //MARKER STUFF--------------------------------------------------------------------------------
    std::string decodingFromPeer_;
    uint32_t sendMoveMinPeriodMs_{50}; // pacing target (~20Hz)
    uint32_t getSendMoveMinPeriodMs() const
    {
        return sendMoveMinPeriodMs_;
    }

    // MarkerUpdate
    void handleMarkerMove(const std::vector<uint8_t>& b, size_t& off);
    void handleMarkerUpdate(const std::vector<uint8_t>& b, size_t& off);
    void handleMarkerMoveState(const std::vector<uint8_t>& b, size_t& off);

    // ---- MARKER UPDATE/DELETE ----
    std::vector<unsigned char> buildMarkerMoveFrame(uint64_t boardId, const flecs::entity& marker, uint32_t seq);
    std::vector<unsigned char> buildMarkerMoveStateFrame(uint64_t boardId, const flecs::entity& marker);
    std::vector<unsigned char> buildMarkerUpdateFrame(uint64_t boardId, const flecs::entity& marker);

    static bool tieBreakWins(const std::string& challengerPeerId, const std::string& currentOwnerPeerId) //NEEDS REVISITING
    {
        return challengerPeerId < currentOwnerPeerId;
    }

    //STABLE
    void handleMarkerDelete(const std::vector<uint8_t>& b, size_t& off);
    std::vector<unsigned char> buildMarkerDeleteFrame(uint64_t boardId, uint64_t markerId);
    std::vector<uint8_t> buildCommitMarkerFrame(uint64_t boardId, uint64_t markerId);
    std::vector<uint8_t> buildCreateMarkerFrame(uint64_t boardId, const flecs::entity& marker, uint64_t imageBytesTotal);
    void handleMarkerMeta(const std::vector<uint8_t>& b, size_t& off);
    //END MARKER STUFF----------------------------------------------------------------------------

    std::unordered_map<uint64_t, PendingImage> imagesRx_;
    MessageQueue<msg::ReadyMessage> inboundGame_;
    // optional background raw-drain worker
    std::atomic<bool> rawWorkerRunning_{false};
    std::atomic<bool> rawWorkerStop_{false};
    std::thread rawWorker_;
    // NetworkManager.h
    std::shared_ptr<IdentityManager> identity_manager;
    std::shared_ptr<ImGuiToaster> toaster_;
    static constexpr size_t kChunk = 8 * 1024; // 8KB chunk
    //static constexpr size_t kHighWater = 2 * 1024 * 1024; // 2 MB queued
    //static constexpr size_t kLowWater = 512 * 1024;       // 512 KB before resuming
    static constexpr int kPaceEveryN = 48; // crude pacing step
    static constexpr int kPaceMillis = 2;

    void handleGameTableSnapshot(const std::vector<uint8_t>& b, size_t& off);
    void handleBoardMeta(const std::vector<uint8_t>& b, size_t& off);
    void handleFogCreate(const std::vector<uint8_t>& b, size_t& off);
    void handleImageChunk(const std::vector<uint8_t>& b, size_t& off);
    void handleCommitBoard(const std::vector<uint8_t>& b, size_t& off);
    void handleCommitMarker(const std::vector<uint8_t>& b, size_t& off);

    void handleUserNameUpdate(const std::vector<uint8_t>& b, size_t& off);

    // FogUpdate
    void handleFogUpdate(const std::vector<uint8_t>& b, size_t& off);
    void handleFogDelete(const std::vector<uint8_t>& b, size_t& off);

    void tryFinalizeImage(msg::ImageOwnerKind kind, uint64_t id);
    // frame builders
    std::vector<uint8_t> buildSnapshotGameTableFrame(uint64_t gameTableId, const std::string& name);
    std::vector<uint8_t> buildSnapshotBoardFrame(const flecs::entity& board, uint64_t imageBytesTotal);
    std::vector<uint8_t> buildFogCreateFrame(uint64_t boardId, const flecs::entity& fog);
    std::vector<uint8_t> buildImageChunkFrame(uint8_t ownerKind, uint64_t id, uint64_t offset, const uint8_t* data, size_t len);
    std::vector<uint8_t> buildCommitBoardFrame(uint64_t boardId);

    // ---- FOG UPDATE/DELETE ----
    std::vector<unsigned char> buildFogUpdateFrame(uint64_t boardId, const flecs::entity& fog);
    std::vector<unsigned char> buildFogDeleteFrame(uint64_t boardId, uint64_t fogId);

    // Helpers used by reconnectPeer (thin wrappers around what you already have)
    /*std::shared_ptr<PeerLink> replaceWithFreshLink_(const std::string& peerId);
    void createOfferAndSend_(const std::string& peerId, const std::shared_ptr<PeerLink>& link);
    void createChannelsIfOfferer_(const std::shared_ptr<PeerLink>& link);*/

    std::string myPeerId_;
    std::string gmPeerId_;

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

    static uint64_t nowMs()
    {
        using Clock = std::chrono::steady_clock;
        using namespace std::chrono;
        return duration_cast<milliseconds>(Clock::now().time_since_epoch()).count();
    }
};

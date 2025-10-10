#pragma once
#include <rtc/rtc.hpp>
#include <functional>
#include <string>

class NetworkManager; // forward declare

class PeerLink
{
public:
    PeerLink(const std::string& id, std::weak_ptr<NetworkManager> parent);
    PeerLink();

    void close();

    void createChannels(); // creates Intent, State, Snapshot, Chat (as offerer)

    //void createPeerConnection();
    void createDataChannel(const std::string& label);
    rtc::Description createOffer();
    rtc::Description createAnswer();
    void setRemoteDescription(const rtc::Description& desc);
    void addIceCandidate(const rtc::Candidate& candidate);

    void send(const std::string& msg);
    bool sendOn(const std::string& label, const std::vector<uint8_t>& bytes);
    bool sendGame(const std::vector<uint8_t>& bytes);
    bool sendChat(const std::vector<uint8_t>& bytes);
    bool sendNote(const std::vector<uint8_t>& bytes);

    void setDisplayName(std::string n);
    const std::string& displayName() const;

    void attachChannelHandlers(const std::shared_ptr<rtc::DataChannel>& ch, const std::string& label);

    bool isDataChannelOpen() const;
    rtc::PeerConnection::State pcState() const; // optional
    const char* pcStateString() const;
    bool isClosedOrFailed() const;
    const char* pcStateToStr(rtc::PeerConnection::State s);

    // send helpers
    /*  void sendIntent(const std::vector<uint8_t>& bytes);
    void sendState(const std::vector<uint8_t>& bytes);
    void sendSnapshot(const std::vector<uint8_t>& bytes);
    void sendChat(const std::vector<uint8_t>& bytes);*/

    // accessors
    bool isConnected() const; // PC connected + *at least* Intent channel open
    bool isPcConnectedOnly() const;
    static constexpr size_t kMaxBufferedBytes = 5 /*MB*/ * 1024 * 1024; //(tune as you like)
    void setOpen(std::string label, bool open) {
        dcOpen_[label] = open;
    }

    bool allRequiredOpen() const;
    bool bootstrapSent() const
    {
        return bootstrapSent_;
    }
    void markBootstrapSent()
    {
        bootstrapSent_ = true;
    }
    void markBootstrapReset()
    {
        bootstrapSent_ = false;
    }

private:
    std::string peerId;
    std::string displayName_;
    std::shared_ptr<rtc::PeerConnection> pc;
    //std::shared_ptr<rtc::DataChannel> dc;
    std::unordered_map<std::string, std::shared_ptr<rtc::DataChannel>> dcs_;
    std::atomic<bool> closing_{false};
    std::weak_ptr<NetworkManager> network_manager;

    std::atomic<rtc::PeerConnection::State> lastState_{rtc::PeerConnection::State::New};
    std::atomic<double> lastStateAt_{0.0};

    std::atomic<bool> remoteDescSet_{false};
    std::vector<rtc::Candidate> pendingRemoteCandidates_;
    std::mutex candMx_;

    bool bootstrapSent_ = false;
    std::unordered_map<std::string, bool> dcOpen_;

    // internal handler dispatch (called from each dc->onMessage)
    void onIntentMessage(const std::vector<uint8_t>& bytes);
    void onStateMessage(const std::vector<uint8_t>& bytes);
    void onSnapshotMessage(const std::vector<uint8_t>& bytes);
    void onChatMessage(const std::vector<uint8_t>& bytes);

    void setupCallbacks(); // internal
};

/*
peerLink->onSendDescription = [this](const rtc::Description& desc) {
    signalingClient.sendSdp(desc);
};

peerLink->onSendCandidate = [this](const rtc::Candidate& cand) {
    signalingClient.sendCandidate(cand);
};


*/

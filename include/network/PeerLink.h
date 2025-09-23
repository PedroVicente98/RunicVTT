#pragma once
#include <rtc/rtc.hpp>
#include <functional>
#include <string>

class NetworkManager; // forward declare

class PeerLink {
public:
    PeerLink(const std::string& id, std::weak_ptr<NetworkManager> parent);
    PeerLink();

    void close();

    void createPeerConnection();        
    void createDataChannel(const std::string& label);  
    rtc::Description createOffer();          
    rtc::Description createAnswer();         
    void setRemoteAnswer(const rtc::Description& desc); 
    void addIceCandidate(const rtc::Candidate& candidate); 

    void send(const std::string& msg);
    void setDisplayName(std::string n);
    const std::string& displayName() const;

  
    bool isDataChannelOpen() const;
    rtc::PeerConnection::State pcState() const; // optional
    bool isClosedOrFailed() const;

private:
    std::string peerId;
    std::string displayName_;
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::DataChannel> dc;
    std::weak_ptr<NetworkManager> network_manager;

    void setupCallbacks();   // internal
};


/*
peerLink->onSendDescription = [this](const rtc::Description& desc) {
    signalingClient.sendSdp(desc);
};

peerLink->onSendCandidate = [this](const rtc::Candidate& cand) {
    signalingClient.sendCandidate(cand);
};


*/
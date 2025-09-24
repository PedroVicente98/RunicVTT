#include "PeerLink.h"
#include "NetworkManager.h" 

PeerLink::PeerLink(const std::string& id, std::weak_ptr<NetworkManager> parent)
    : peerId(id), network_manager(parent)
{
    if (auto nm = network_manager.lock()) {
        auto config = nm->getRTCConfig();
        config.iceServers.push_back({ "stun:stun.l.google.com:19302" });    // Google
        config.iceServers.push_back({ "stun:stun1.l.google.com:19302" });   // Google alt
        config.iceServers.push_back({ "stun:stun.stunprotocol.org:3478" }); // Community server
        pc = std::make_shared<rtc::PeerConnection>(config);
        setupCallbacks();
    }
    else {
        throw std::runtime_error("NetworkManager expired");
    }
}

// PeerLink.cpp
void PeerLink::setDisplayName(std::string n) 
{ 
    displayName_ = std::move(n);
}

const std::string& PeerLink::displayName() const 
{ 
    return displayName_; 
}

void PeerLink::send(const std::string& msg) {
    if (dc && dc->isOpen()) {
        dc->send(msg);
    }
}


void PeerLink::createDataChannel(const std::string& label) {
    if (!pc) return;                       // ensure pc exists
    if (dc && dc->isOpen()) return;        // idempotent-ish: keep existing dc

    dc = pc->createDataChannel(label);     // negotiated=false by default
    attachChannelHandlers(dc);
}


rtc::Description PeerLink::createOffer() {
    auto offer = pc->createOffer();
    rtc::LocalDescriptionInit init;
    init.icePwd = offer.icePwd();   // string
    init.iceUfrag = offer.iceUfrag();   // string
    pc->setLocalDescription(offer.type(), init);
    return offer;
}

rtc::Description PeerLink::createAnswer() {
    auto answer = pc->createAnswer();
    rtc::LocalDescriptionInit init;
    init.icePwd = answer.icePwd();
    init.iceUfrag = answer.iceUfrag();
    pc->setLocalDescription(answer.type(), init);
    return answer; // Send this via signaling
}

void PeerLink::setRemoteDescription(const rtc::Description& desc) {
    pc->setRemoteDescription(desc);
    {
        std::lock_guard<std::mutex> lk(candMx_);
        remoteDescSet_.store(true, std::memory_order_release);
        for (auto& c : pendingRemoteCandidates_) {
            pc->addRemoteCandidate(c);
        }
        pendingRemoteCandidates_.clear();
    }
}

void PeerLink::addIceCandidate(const rtc::Candidate& candidate) {
    std::lock_guard<std::mutex> lk(candMx_);
    if (!remoteDescSet_.load(std::memory_order_acquire)) {
        pendingRemoteCandidates_.push_back(candidate);
    } else {
        pc->addRemoteCandidate(candidate);
    }
}

void PeerLink::setupCallbacks() {
    pc->onStateChange([this](rtc::PeerConnection::State state) {
        lastState_ = state;
        lastStateAt_ = std::chrono::seconds().count();
        std::cout << "[PeerLink] State(" << peerId << "): " << (int)state << "at " << lastStateAt_ << "\n";
    });

    pc->onLocalDescription([wk = network_manager, id = peerId](rtc::Description desc) {
        if (auto nm = wk.lock()) nm->onPeerLocalDescription(id, desc);
    });

    pc->onLocalCandidate([wk = network_manager, id = peerId](rtc::Candidate cand) {
        if (auto nm = wk.lock()) nm->onPeerLocalCandidate(id, cand);
    });

    pc->onDataChannel([this](std::shared_ptr<rtc::DataChannel> ch) {
        dc = std::move(ch);
        attachChannelHandlers(dc);
    });
}


void PeerLink::attachChannelHandlers(const std::shared_ptr<rtc::DataChannel>& ch) {
    ch->onOpen([this] {
        std::cout << "[PeerLink] DataChannel open (" << peerId << ")\n";
    });
    ch->onClosed([this] {
        std::cout << "[PeerLink] DataChannel closed (" << peerId << ")\n";
    });
    ch->onMessage([this](rtc::message_variant msg) {
        if (std::holds_alternative<std::string>(msg)) {
            std::cout << "[PeerLink] Received: " << std::get<std::string>(msg) << "\n";
            // TODO: route to your game message router
        }
        // handle binary if you need:
        // else if (std::holds_alternative<rtc::binary>(msg)) { ... }
    });
}



bool PeerLink::isDataChannelOpen() const {
    return dc && dc->isOpen();
}

void PeerLink::close() {
    // Close datachannel first
    if (dc) {
        try { dc->close(); }
        catch (...) {}
    }
    // Close peer connection
    if (pc) {
        try { pc->close(); }
        catch (...) {}
    }
    // Release references
    dc.reset();
    pc.reset();
    if (auto nm = network_manager.lock()) nm->removePeer(peerId);
}

rtc::PeerConnection::State PeerLink::pcState() const {
    if (!pc) return rtc::PeerConnection::State::Closed;
    return pc->state();
}

bool PeerLink::isClosedOrFailed() const {
    if (!pc) return true;
    auto s = pc->state();
    return s == rtc::PeerConnection::State::Closed ||
        s == rtc::PeerConnection::State::Failed;
}

const char* PeerLink::pcStateString() const {
    auto s = pcState();
    using S = rtc::PeerConnection::State;
    switch (s) {
    case S::New:         return "New";
    case S::Connecting:  return "Connecting";
    case S::Connected:   return "Connected";
    case S::Disconnected:return "Disconnected";
    case S::Failed:      return "Failed";
    case S::Closed:      return "Closed";
    }
    return "Unknown";
}


//
//void PeerLink::close() {
//    if (dc) dc->close();
//    if (pc) pc->close();
//}

//void PeerLink::createPeerConnection() {
//    if (pc) return; // already created
//    if (auto nm = network_manager.lock()) {
//        auto config = nm->getRTCConfig();
//        config.iceServers.push_back({ "stun:stun.l.google.com:19302" }); 
//        pc = std::make_shared<rtc::PeerConnection>(config);
//        setupCallbacks(); // bind onStateChange, onLocalDescription, onLocalCandidate...
//    }
//    else {
//        throw std::runtime_error("NetworkManager expired");
//    }
//}
/////
//
//#include "PeerLink.h"
//#include "NetworkManager.h"
//#include <stdexcept>
//#include <iostream>
//
//PeerLink::PeerLink(const std::string& id, std::shared_ptr<NetworkManager> parent)
//    : peerId(id), network_manager(parent)
//{
//    if (auto nm = network_manager.lock()) {
//        auto config = nm->getRTCConfig();
//        pc = std::make_shared<rtc::PeerConnection>(config);
//        setupCallbacks();
//    } else {
//        throw std::runtime_error("PeerLink ctor: NetworkManager expired");
//    }
//}
//
//void PeerLink::send(const std::string& msg) {
//    if (dc && dc->isOpen()) dc->send(msg);
//}
//

//
//void PeerLink::createPeerConnection() {
//    // Prefer taking config from NetworkManager (keeps a single source of truth)
//    if (auto nm = network_manager.lock()) {
//        auto config = nm->getRTCConfig();
//        pc = std::make_shared<rtc::PeerConnection>(config);
//        setupCallbacks();
//    } else {
//        throw std::runtime_error("PeerLink::createPeerConnection: NetworkManager expired");
//    }
//}
//
//void PeerLink::createDataChannel(const std::string& label) {
//    if (!pc) throw std::runtime_error("PeerLink::createDataChannel: PC not created");
//    dc = pc->createDataChannel(label);
//
//    // optional per-channel callbacks (messages routed via NM in setupCallbacks for onDataChannel)
//    dc->onOpen([this]() {
//        std::cout << "[PeerLink] DataChannel open: " << dc->label() << "\n";
//    });
//    dc->onMessage([this](rtc::message_variant msg) {
//        if (std::holds_alternative<std::string>(msg)) {
//            std::cout << "[PeerLink] Received: " << std::get<std::string>(msg) << "\n";
//        }
//    });
//}
//
//rtc::Description PeerLink::createOffer() {
//    if (!pc) throw std::runtime_error("PeerLink::createOffer: PC not created");
//    auto offer = pc->createOffer();
//    rtc::LocalDescriptionInit init;
//    init.icePwd   = offer.icePwd();
//    init.iceUfrag = offer.iceUfrag();
//    pc->setLocalDescription(offer.type(), init);
//    return offer;
//}
//
//rtc::Description PeerLink::createAnswer() {
//    if (!pc) throw std::runtime_error("PeerLink::createAnswer: PC not created");
//    auto answer = pc->createAnswer();
//    rtc::LocalDescriptionInit init;
//    init.icePwd   = answer.icePwd();
//    init.iceUfrag = answer.iceUfrag();
//    pc->setLocalDescription(answer.type(), init);
//    return answer;
//}
//
//void PeerLink::setRemoteAnswer(const rtc::Description& desc) {
//    if (!pc) throw std::runtime_error("PeerLink::setRemoteAnswer: PC not created");
//    pc->setRemoteDescription(desc);
//}
//
//void PeerLink::addIceCandidate(const rtc::Candidate& candidate) {
//    if (!pc) throw std::runtime_error("PeerLink::addIceCandidate: PC not created");
//    pc->addRemoteCandidate(candidate);
//}
//
//void PeerLink::setupCallbacks() {
//    if (!pc) throw std::runtime_error("PeerLink::setupCallbacks: PC not created");
//
//    pc->onStateChange([wptr = network_manager, pid = peerId](rtc::PeerConnection::State state) {
//        if (auto nm = wptr.lock()) {
//            std::cout << "[PeerLink] PC state(" << pid << "): " << (int)state << "\n";
//            // You can add nm->handlePeerState(pid, state) later if you want.
//            if (state == rtc::PeerConnection::State::Failed) {
//                // Let NetworkManager handle removal if desired
//                // nm->removePeerLink(pid);
//            }
//        } else {
//            throw std::runtime_error("PeerLink::onStateChange: NetworkManager expired");
//        }
//    });
//
//    pc->onLocalDescription([wptr = network_manager, pid = peerId](rtc::Description desc) {
//        if (auto nm = wptr.lock()) {
//            nm->sendSdpToPeer(pid, desc);
//        } else {
//            throw std::runtime_error("PeerLink::onLocalDescription: NetworkManager expired");
//        }
//    });
//
//    pc->onLocalCandidate([wptr = network_manager, pid = peerId](rtc::Candidate cand) {
//        if (auto nm = wptr.lock()) {
//            nm->sendIceToPeer(pid, cand);
//        } else {
//            throw std::runtime_error("PeerLink::onLocalCandidate: NetworkManager expired");
//        }
//    });
//
//    // Remote-created datachannels
//    pc->onDataChannel([this](std::shared_ptr<rtc::DataChannel> remoteDc) {
//        dc = remoteDc; // if you only keep one; otherwise map by label
//        // If you want to inform NM about DC open/close/messages, bind here:
//        remoteDc->onOpen([wptr = network_manager, pid = peerId, label = remoteDc->label()]() {
//            if (auto nm = wptr.lock()) {
//                // you can add nm->onChannelOpen(pid, label);
//                std::cout << "[PeerLink] Remote DC open: " << label << "\n";
//            } else {
//                throw std::runtime_error("PeerLink::onDataChannel::onOpen: NetworkManager expired");
//            }
//        });
//        remoteDc->onMessage([wptr = network_manager, pid = peerId, label = remoteDc->label()](rtc::message_variant msg) {
//            if (auto nm = wptr.lock()) {
//                // you can forward bytes to NM here later if desired
//                if (auto* s = std::get_if<std::string>(&msg)) {
//                    std::cout << "[PeerLink] DC msg (" << pid << ":" << label << "): " << *s << "\n";
//                }
//            } else {
//                throw std::runtime_error("PeerLink::onDataChannel::onMessage: NetworkManager expired");
//            }
//        });
//        remoteDc->onClosed([wptr = network_manager, pid = peerId, label = remoteDc->label()]() {
//            if (auto nm = wptr.lock()) {
//                // nm->onChannelClosed(pid, label);
//                std::cout << "[PeerLink] Remote DC closed: " << label << "\n";
//            } else {
//                throw std::runtime_error("PeerLink::onDataChannel::onClosed: NetworkManager expired");
//            }
//        });
//    });
//}

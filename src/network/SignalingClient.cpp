#include "SignalingClient.h"
#include <rtc/rtc.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include "NetworkManager.h" 
#include "NetworkUtilities.h"
#include "Message.h"

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

SignalingClient::SignalingClient(std::weak_ptr<NetworkManager> parent)
    : network_manager(parent) 
{

}

// SignalingClient.cpp
bool SignalingClient::connectUrl(const std::string& url) {
    rtc::WebSocketConfiguration cfg;
    cfg.pingInterval = std::chrono::milliseconds(1000);
    cfg.connectionTimeout = std::chrono::milliseconds(0);
    ws = std::make_shared<rtc::WebSocket>(cfg);

    ws->onOpen([=]() {
        if (auto nm = network_manager.lock()) {
            ws->send(msg::makeAuth(nm->getNetworkPassword(), nm->getMyUsername()).dump());
        }
        });
    ws->onClosed([=]() { std::cout << "[SignalingClient] Disconnected from GM.\n"; });
    ws->onMessage([=](std::variant<rtc::binary, rtc::string> msg) {
        if (!std::holds_alternative<rtc::string>(msg)) return;
        this->onMessage(std::get<rtc::string>(msg));
        });

    // normalize https→wss, http→ws; leave ws/wss as-is
    const std::string norm = NetworkUtilities::normalizeWsUrl(url);
    std::cout << "[SignalingClient] Opening URL: " << norm << "\n";
    try {
        if (ws->isClosed()) ws->open(norm);
        else { ws->close(); ws->open(norm); }
    }
    catch (const std::exception& e) {
        std::cout << "Exception on ws->open: " << e.what() << "\n";
        return false;
    }
    return true;
}

//
//bool SignalingClient::connect(const std::string& ip, unsigned short port) {
//    auto client_configuration = rtc::WebSocketConfiguration();
//    client_configuration.pingInterval = std::chrono::milliseconds(1000);
//    client_configuration.connectionTimeout = std::chrono::milliseconds(0);
//
//    ws = std::make_shared<rtc::WebSocket>(client_configuration);
//
//    ws->onOpen([=]() {
//        std::cout << "[SignalingClient] Connected to GM. " <<"\n";
//        auto nm = network_manager.lock();
//        if (!nm) throw std::exception("[SignalingClient] NetworkManager Inactive");
//        auto password = nm->getNetworkPassword();
//        auto username = nm->getMyUsername();
//        auto msg = msg::makeAuth(password, username);
//        ws->send(msg.dump());
//    });
//
//    ws->onClosed([=]() {
//        std::cout << "[SignalingClient] Disconnected from GM." << "\n";
//    });
//
//    ws->onMessage([=](std::variant<rtc::binary, rtc::string> msg) {
//        std::cout << "[SignalingClient] MESSAGE RECEIVED" << "\n";
//        if (!std::holds_alternative<rtc::string>(msg)) return;
//        const auto& s = std::get<rtc::string>(msg);
//        std::cout << "[SignalingClient] MESSAGE: " << s << "\n";
//        this->onMessage(s);
//    });
//
//    const std::string url = NetworkUtilities::normalizeWsUrl(ip, port);
//    std::cout << "[SignalingClient] Opening URL: " << url << "\n";
//    try{
//        if (ws->isClosed()) {
//            ws->open(url);
//        }
//        else {
//            ws->close();
//            ws->open(url);
//        }
//    }
//    catch(std::exception e){
//        std::cout << "Expection on ws->open: " << e.what() << "\n";
//        return false;
//    }
//
//    return true;
//}
bool SignalingClient::connect(const std::string& ip, unsigned short port) {
    // If caller accidentally passed a full URL, just route to connectUrl.
    auto starts = [&](const char* p) { return ip.rfind(p, 0) == 0; };
    if (starts("https://") || starts("http://") || starts("wss://") || starts("ws://")) {
        return connectUrl(ip);
    }

    rtc::WebSocketConfiguration cfg;
    cfg.pingInterval = std::chrono::milliseconds(1000);
    cfg.connectionTimeout = std::chrono::milliseconds(0);
    ws = std::make_shared<rtc::WebSocket>(cfg);

    ws->onOpen([=]() {
        std::cout << "[SignalingClient] Connected to GM.\n";
        auto nm = network_manager.lock();
        if (!nm) throw std::runtime_error("[SignalingClient] NetworkManager Inactive");
        ws->send(msg::makeAuth(nm->getNetworkPassword(), nm->getMyUsername()).dump());
        });

    ws->onClosed([=]() {
        std::cout << "[SignalingClient] Disconnected from GM.\n";
        });

    ws->onError([=](std::string err) {
        std::cout << "[SignalingClient] WebSocket error: " << err << "\n";
        });

    ws->onMessage([=](std::variant<rtc::binary, rtc::string> msg) {
        if (!std::holds_alternative<rtc::string>(msg)) return;
        this->onMessage(std::get<rtc::string>(msg));
        });

    // Build a proper ws URL from host:port using your existing helper
    const std::string url = NetworkUtilities::normalizeWsUrl(ip, port); // e.g. "ws://<ip>:<port>"
    std::cout << "[SignalingClient] Opening URL: " << url << "\n";
    try {
        if (ws->isClosed()) {
            ws->open(url);
        }
        else {
            ws->close();
            ws->open(url);
        }
    }
    catch (const std::exception& e) {  // catch by reference
        std::cout << "Exception on ws->open: " << e.what() << "\n";
        return false;
    }
    return true;
}


void SignalingClient::send(const std::string& message) {
    if (ws) {
        ws->send(message);
    }
}

void SignalingClient::onMessage(const std::string& msg) {
    using json = nlohmann::json;
    json j; try { j = json::parse(msg); }
    catch (...) { return; }
    const std::string type = j.value(msg::key::Type, ""); if (type.empty()) return;

    auto nm = network_manager.lock();
    if (!nm) throw std::runtime_error("NM expired");

    if (type == msg::signaling::AuthResponse) {
        if (j.value(msg::key::AuthOk, msg::value::False) == msg::value::True) {
            // for each existing authed client -> start offer
            const std::string myId = j.value(std::string(msg::key::ClientId), "");
            const std::string myUser = j.value(std::string(msg::key::Username), "me");

            nm->setMyIdentity(myId, myUser);

            if (j.contains(msg::key::Clients) && j[msg::key::Clients].is_array()) {
                for (auto& v : j[msg::key::Clients]) {
                    std::string peerId = v.get<std::string>();
                    auto link = nm->ensurePeerLink(peerId);
                    link->createPeerConnection();
                    link->createDataChannel(msg::dc::name::Game);
                    link->createOffer();
                }
            }
        }
        return;
    }

    if (type == msg::signaling::Offer) {
        const std::string from = j.value(msg::key::From, "");
        const std::string sdp = j.value(msg::key::Sdp, "");
        const std::string username = j.value(msg::key::Username, "guest_"+from);
        if (from.empty() || sdp.empty()) return;

        auto link = nm->ensurePeerLink(from);
        nm->upsertPeerIdentity(from, username);
        link->createPeerConnection(); // safe if already exists
        // Apply remote offer
        link->setRemoteAnswer(rtc::Description(sdp, std::string(msg::signaling::Offer)));
        // Create local answer (this will trigger NM callback to send)
        link->createAnswer();
        return;
    }

    if (type == msg::signaling::Answer) {
        const std::string from = j.value(msg::key::From, "");
        const std::string sdp = j.value(msg::key::Sdp, "");
        if (from.empty() || sdp.empty()) return;

        auto link = nm->ensurePeerLink(from);
        link->setRemoteAnswer(rtc::Description(sdp, std::string(msg::signaling::Answer)));
        return;
    }

    if (type == msg::signaling::Candidate) {
        const std::string from = j.value(msg::key::From, "");
        const std::string cand = j.value(msg::key::Candidate, "");
        const std::string mid = j.value(msg::key::SdpMid, "");
        if (from.empty() || cand.empty()) return;

        auto link = nm->ensurePeerLink(from);
        link->addIceCandidate(rtc::Candidate(cand, mid));
        return;
    }


    ////NOT USED
    //if (type == msg::signaling::Presence) {
    //    if (j.value(msg::key::Event, "") == msg::signaling::Join) {
    //        std::string peerId = j.value(msg::key::ClientId, "");
    //        if (!peerId.empty()) {
    //            auto link = nm->ensurePeerLink(peerId);
    //            link->createPeerConnection();
    //            link->createDataChannel(msg::dc::name::Game);
    //            link->createOffer(); // NM callback will send
    //        }
    //    }
    //    return;
    //}

}
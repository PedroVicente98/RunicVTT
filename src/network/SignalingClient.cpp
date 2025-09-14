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

bool SignalingClient::connect(const std::string& ip, unsigned short port) {
    auto client_configuration = rtc::WebSocketConfiguration();
    client_configuration.pingInterval = std::chrono::milliseconds(1000);
    client_configuration.connectionTimeout = std::chrono::milliseconds(0);

    ws = std::make_shared<rtc::WebSocket>(client_configuration);

    ws->onOpen([=]() {
        std::cout << "[SignalingClient] Connected to GM. " <<"\n";
        auto nm = network_manager.lock();
        if (!nm) throw std::exception("[SignalingClient] NetworkManager Inactive");
        auto password = nm->getNetworkPassword();
        auto msg = msg::makeAuth(password);
        ws->send(msg.dump());
    });

    ws->onClosed([=]() {
        std::cout << "[SignalingClient] Disconnected from GM." << "\n";
    });

    ws->onMessage([=](std::variant<rtc::binary, rtc::string> msg) {
        std::cout << "[SignalingClient] MESSAGE RECEIVED" << "\n";
        if (!std::holds_alternative<rtc::string>(msg)) return;
        const auto& s = std::get<rtc::string>(msg);
        std::cout << "[SignalingClient] MESSAGE: " << s << "\n";
        this->onMessage(s);
    });

    const std::string url = NetworkUtilities::normalizeWsUrl(ip, port);
    std::cout << "[SignalingClient] Opening URL: " << url << "\n";
    try{
        if (ws->isClosed()) {
            ws->open(url);
        }
        else {
            ws->close();
            ws->open(url);
        }
    }
    catch(std::exception e){
        std::cout << "Expection on ws->open: " << e.what() << "\n";
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
    json j;
    try { j = json::parse(msg); }
    catch (...) { return; }

    const std::string type = j.value("type", "");
    if (type.empty()) return;

    bool authenticated = false;
    if (type == msg::signaling::AuthResponse) {
        std::string ok = j.value("ok", msg::value::False);
        if (ok == msg::value::True) {
            std::cout << "CLIENT AUTHED " << "\n";
        }
    }

    //Offer
    if (type == msg::signaling::Offer)
    {

    }

    //Answer
    if (type == msg::signaling::Answer)
    {

    }


    //Candidate
    if (type == msg::signaling::Candidate)
    {

    }
}
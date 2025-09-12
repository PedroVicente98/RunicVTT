#include "SignalingClient.h"
#include <rtc/rtc.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include "NetworkManager.h" 
#include "NetworkUtilities.h"

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
    });

    ws->onClosed([=]() {
        std::cout << "[SignalingClient] Disconnected from GM." << "\n";
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


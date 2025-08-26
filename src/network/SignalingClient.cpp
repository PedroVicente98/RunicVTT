#include "SignalingClient.h"
#include <rtc/rtc.hpp>
#include <iostream>


SignalingClient::SignalingClient() {

}

bool SignalingClient::connect(const std::string& ip, unsigned short port) {
    std::string url = "ws://" + ip + ":" + std::to_string(port);
    ws = std::make_shared<rtc::WebSocket>();
    
    ws->onOpen([]() {
        std::cout << "[SignalingClient] Connected to GM.\n";
        });

    ws->onClosed([]() {
        std::cout << "[SignalingClient] Disconnected from GM.\n";
        });

    ws->onMessage([this](std::variant<rtc::binary, rtc::string> msg) {
        if (std::holds_alternative<rtc::string>(msg)) {
            onSignal(std::get<rtc::string>(msg));
        }
        });
    try{
        if (ws->isClosed()) {
            ws->open(ip);
        }
        else {
            ws->close();
            ws->open(ip);
        }
    }
    catch(std::exception e){
        return false;
    }

    return true;
}
void SignalingClient::onSignal(const std::string& msg) {

}

//
//void SignalingClient::send(const std::string& message) {
//    if (ws) {
//        ws->send(message);
//    }
//}
//

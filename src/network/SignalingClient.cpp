//#include "SignalingClient.h"
//#include <rtc/rtc.hpp>
//#include <iostream>
//
//bool SignalingClient::connect(const std::string& ip, unsigned short port) {
//    std::string url = "ws://" + ip + ":" + std::to_string(port);
//    ws = std::make_shared<rtc::WebSocket>(url);
//
//    ws->onOpen([]() {
//        std::cout << "[SignalingClient] Connected to GM.\n";
//        });
//
//    ws->onClosed([]() {
//        std::cout << "[SignalingClient] Disconnected from GM.\n";
//        });
//
//    ws->onMessage([this](std::variant<rtc::binary, rtc::string> msg) {
//        if (onSignalCb && std::holds_alternative<rtc::string>(msg)) {
//            onSignalCb(std::get<rtc::string>(msg));
//        }
//        });
//
//    return true;
//}
//
//void SignalingClient::send(const std::string& message) {
//    if (ws) {
//        ws->send(message);
//    }
//}
//
//void SignalingClient::onSignal(std::function<void(const std::string&)> cb) {
//    onSignalCb = cb;
//}

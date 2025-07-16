#include "SignalingServer.h"
#include <rtc/rtc.hpp>
#include <iostream>


SignalingServer::SignalingServer(std::function<void(const std::string&)> onConnectCb, std::function<void(const std::string&, const std::string&)> onMessageCb)
{
    this->onConnectCb = onConnectCb;
    this->onMessageCb = onMessageCb;
}

SignalingServer::SignalingServer()
{
}

SignalingServer::~SignalingServer()
{
    this->stop();
}

void SignalingServer::start(unsigned short port) {
    auto serverConfiguration = rtc::WebSocketServerConfiguration{};
    serverConfiguration.bindAddress = "0.0.0.0";
    serverConfiguration.port = port;
    server = std::make_shared<rtc::WebSocketServer>(serverConfiguration);

    server->onClient([=](std::shared_ptr<rtc::WebSocket> client) {
        auto addrOpt = client->remoteAddress();
        if (addrOpt) {
            std::string peerId = addrOpt.value();

            clients[peerId] = client;

            if (onConnectCb) onConnectCb(peerId);

            client->onMessage([=](std::variant<rtc::binary, rtc::string> msg) {
                if (onMessageCb && std::holds_alternative<rtc::string>(msg)) {
                    onMessageCb(peerId, std::get<rtc::string>(msg));
                }
                });

            client->onClosed([=]() {
                std::cout << "[SignalingServer] Peer disconnected: " << peerId << "\n";
                clients.erase(peerId);
                });
        }
    });

    std::cout << "[SignalingServer] Listening at ws://0.0.0.0" << ":" << port << "\n";
}

void SignalingServer::stop() {
    server->stop();
    clients.clear();
}

// 
//
//void SignalingServer::send(const std::string& peerId, const std::string& message) {
//    if (clients.count(peerId)) {
//        clients[peerId]->send(message);
//    }
//}
//
//void SignalingServer::onClientConnected(std::function<void(const std::string&)> cb) {
//    onConnectCb = cb;
//}
//
//void SignalingServer::onMessage(std::function<void(const std::string&, const std::string&)> cb) {
//    onMessageCb = cb;
//}

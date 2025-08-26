#include "SignalingServer.h"
#include <rtc/rtc.hpp>
#include <iostream>

SignalingServer::SignalingServer()
{
    
}

SignalingServer::~SignalingServer()
{
    this->stop();
}

void SignalingServer::start(unsigned short port) 
{
    auto serverConfiguration = rtc::WebSocketServerConfiguration{};
    serverConfiguration.bindAddress = "0.0.0.0";
    serverConfiguration.port = port;
    server = std::make_shared<rtc::WebSocketServer>(serverConfiguration);

    server->onClient([=](std::shared_ptr<rtc::WebSocket> client) {
        auto addrOpt = client->remoteAddress();
        std::cout << "[SignalingServer] Client Connected at " << addrOpt.value() << "\n";
        if (addrOpt) {
            std::string peerId = addrOpt.value();

            onConnect(peerId, client);

            client->onMessage([=](std::variant<rtc::binary, rtc::string> msg) {
                if (std::holds_alternative<rtc::string>(msg)) {
                    onMessage(peerId, std::get<rtc::string>(msg));
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
    if (server.get() != nullptr) {
        server->stop();
    }
    clients.clear();
}

void SignalingServer::onConnect(std::string peer_id, std::shared_ptr<rtc::WebSocket> client)
{
    clients[peer_id] = client;

}

void SignalingServer::onMessage(std::string peer_id, std::string msg) 
{
    for (auto client : clients) {
        send(client.first, msg);
    }
}

void SignalingServer::send(const std::string& peerId, const std::string& message) {
    if (clients.count(peerId)) {
        clients[peerId]->send(message);
    }
}


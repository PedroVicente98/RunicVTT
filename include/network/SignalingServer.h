#pragma once
#include <string>
#include <functional>
#include "rtc/rtc.hpp"


class SignalingServer {
public:
    SignalingServer(std::function<void(const std::string&)> onConnectCb, std::function<void(const std::string&, const std::string&)> onMessageCb);
    SignalingServer();
    ~SignalingServer();
    void start(unsigned short port);
    void stop();

    void send(const std::string& peerId, const std::string& message);
    void onClientConnected(std::function<void(const std::string&)>);
    void onMessage(std::function<void(const std::string& peerId, const std::string& message)>);

private:
    std::shared_ptr<rtc::WebSocketServer> server;
    std::unordered_map<std::string, std::shared_ptr<rtc::WebSocket>> clients;
    std::function<void(const std::string&)> onConnectCb;
    std::function<void(const std::string&, const std::string&)> onMessageCb;
};

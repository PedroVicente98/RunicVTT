#pragma once
#include <string>
#include <functional>
#include <memory>
#include "rtc/rtc.hpp"

class NetworkManager;
class SignalingClient {
public:

    SignalingClient(std::weak_ptr<NetworkManager> parent);
    // SignalingClient.h
    bool connectUrl(const std::string& url);
    bool connect(const std::string& ip, unsigned short port);
    void send(const std::string& message);
    void onMessage(const std::string& msg);
    void close();

private:
    std::shared_ptr<rtc::WebSocket> ws;
    std::weak_ptr<NetworkManager> network_manager;
};

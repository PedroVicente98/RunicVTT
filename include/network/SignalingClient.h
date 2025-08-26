#pragma once
#include <string>
#include <functional>
#include <memory>
#include "rtc/rtc.hpp"

class SignalingClient {
public:

    SignalingClient();

    bool connect(const std::string& ip, unsigned short port);
    void send(const std::string& message);
    void onSignal(const std::string& msg);

private:
    std::shared_ptr<rtc::WebSocket> ws;
    std::function<void(const std::string&)> onSignalCb;
};

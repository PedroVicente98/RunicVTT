#pragma once
#include <functional>
#include <string>

class SignalingClient {
public:
    bool connect(const std::string& ip, unsigned short port);
    void send(const std::string& message);
    void onSignal(std::function<void(const std::string& msg)>);
};

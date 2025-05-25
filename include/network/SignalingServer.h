#pragma once
#include <functional>
#include <string>

class SignalingServer {
public:
    void start(const std::string& host, unsigned short port);
    void stop();

    void send(const std::string& toPeerId, const std::string& message);
    void onMessage(std::function<void(const std::string& fromPeerId, const std::string& msg)>);
    void onClientConnected(std::function<void(const std::string&)>);

private:
    // Internals for WebSocket implementation
};

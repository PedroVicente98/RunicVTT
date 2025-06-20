#pragma once
#include <rtc/rtc.hpp>
#include <functional>
#include <string>

class PeerLink {
public:
    PeerLink(const std::string& id,
        const rtc::Configuration& config,
        std::function<void(const std::string&)> onSignalOut,
        std::function<void()> onReady);

    void receiveSignal(const std::string& json);
    void close();

private:
    std::string peerId;
    std::shared_ptr<rtc::PeerConnection> pc;
    std::shared_ptr<rtc::DataChannel> dc;
};

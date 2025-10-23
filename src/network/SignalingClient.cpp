#include "SignalingClient.h"
#include <rtc/rtc.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include "NetworkManager.h"
#include "NetworkUtilities.h"
#include "Message.h"

using json = nlohmann::json;
using Clock = std::chrono::steady_clock;

SignalingClient::SignalingClient(std::weak_ptr<NetworkManager> parent) :
    network_manager(parent)
{
}

// SignalingClient.cpp
bool SignalingClient::connectUrl(const std::string& url)
{
    rtc::WebSocketConfiguration cfg;
    cfg.pingInterval = std::chrono::milliseconds(1000);
    cfg.connectionTimeout = std::chrono::milliseconds(0);
    ws = std::make_shared<rtc::WebSocket>(cfg);

    ws->onOpen([=]()
               {
        if (auto nm = network_manager.lock()) {
            nm->pushStatusToast("Signaling connected", ImGuiToaster::Level::Good);
            // send AUTH with username + uniqueId from IdentityManager
            ws->send(msg::makeAuth(nm->getNetworkPassword(),
                                   nm->getMyUsername(),
                                   nm->getMyUniqueId()).dump());
        } });

    ws->onClosed([=]()
                 {
        if (auto nm = network_manager.lock())
            nm->pushStatusToast("Signaling disconnected", ImGuiToaster::Level::Error); });

    ws->onMessage([=](std::variant<rtc::binary, rtc::string> m)
                  {
        if (!std::holds_alternative<rtc::string>(m)) return;
        this->onMessage(std::get<rtc::string>(m)); });

    const std::string norm = NetworkUtilities::normalizeWsUrl(url);
    try
    {
        ws->isClosed() ? ws->open(norm) : (ws->close(), ws->open(norm));
    }
    catch (const std::exception& e)
    {
        std::cout << "ws->open: " << e.what() << "\n";
        return false;
    }
    return true;
}

bool SignalingClient::connect(const std::string& ip, unsigned short port)
{
    // (same as your version, only keep the AUTH send identical to above)
    rtc::WebSocketConfiguration cfg;
    cfg.pingInterval = std::chrono::milliseconds(1000);
    cfg.connectionTimeout = std::chrono::milliseconds(0);
    ws = std::make_shared<rtc::WebSocket>(cfg);

    ws->onOpen([=]()
               {
        auto nm = network_manager.lock();
        if (!nm) throw std::runtime_error("[SignalingClient] NetworkManager Inactive");
        ws->send(msg::makeAuth(nm->getNetworkPassword(),
                               nm->getMyUsername(),
                               nm->getMyUniqueId()).dump()); });

    ws->onMessage([=](std::variant<rtc::binary, rtc::string> m)
                  {
        if (!std::holds_alternative<rtc::string>(m)) return;
        this->onMessage(std::get<rtc::string>(m)); });

    const std::string url = NetworkUtilities::normalizeWsUrl(ip, port);
    try
    {
        ws->isClosed() ? ws->open(url) : (ws->close(), ws->open(url));
    }
    catch (const std::exception& e)
    {
        std::cout << "ws->open: " << e.what() << "\n";
        return false;
    }
    return true;
}

void SignalingClient::send(const std::string& message)
{
    if (ws && !ws->isClosed())
    {
        ws->send(message);
    }
}

void SignalingClient::onMessage(const std::string& msg)
{
    using json = nlohmann::json;
    json j;
    try
    {
        j = json::parse(msg);
    }
    catch (...)
    {
        return;
    }
    const std::string type = j.value(msg::key::Type, "");
    if (type.empty())
        return;

    auto nm = network_manager.lock();
    if (!nm)
        throw std::runtime_error("NM expired");

    if (type == msg::signaling::ServerDisconnect)
    {
        if (auto nm = network_manager.lock())
        {
            nm->disconectFromPeers();
        }
        return;
    }

    // SignalingClient::onMessage
    if (type == msg::signaling::PeerDisconnect)
    {
        const std::string target = j.value(std::string(msg::key::Target), "");
        if (target.empty())
            return;
        if (auto nm = network_manager.lock())
        {
            nm->removePeer(target); // close PC/DC and erase from map
        }
        return;
    }

    if (type == msg::signaling::AuthResponse)
    {
        if (j.value(msg::key::AuthOk, msg::value::False) == msg::value::True)
        {
            // Routing id assigned by the server
            const std::string newPeerId = j.value(std::string(msg::key::ClientId), "");
            // --- bind *self* now that peerId is known ---
            const std::string oldPeerId = nm->getMyPeerId();
            nm->setMyPeerId(newPeerId);
            // bind *self* now that peerId is known
            if (auto idm = nm->getIdentityManager())
            {
                if (!oldPeerId.empty() && oldPeerId != newPeerId)
                    idm->erasePeer(oldPeerId); // optional helper to drop stale mapping

                idm->bindPeer(newPeerId, idm->myUniqueId(), idm->myUsername());
            }

            // GM id now carries GM UNIQUE ID
            const std::string gmUniqueId = j.value(std::string(msg::key::GmId), "");
            if (!gmUniqueId.empty())
            {
                nm->setGMId(gmUniqueId);
            }

            // Start offers to already-authed peers
            if (j.contains(msg::key::Clients) && j[msg::key::Clients].is_array())
            {
                for (auto& v : j[msg::key::Clients])
                {
                    std::string peerId = v.get<std::string>();
                    auto link = nm->ensurePeerLink(peerId);
                    link->createChannels();
                    link->createOffer();
                }
            }
        }
        return;
    }

    if (type == msg::signaling::Offer)
    {
        const std::string from = j.value(msg::key::From, "");
        const std::string sdp = j.value(msg::key::Sdp, "");
        const std::string username = j.value(msg::key::Username, "guest_" + from);
        const std::string uniqueId = j.value(std::string(msg::key::UniqueId), "");
        if (from.empty() || sdp.empty() || uniqueId.empty())
        {
            Logger::instance().log("net", Logger::Level::Warn,
                                   "Signaling missing fields (from/sdp/uniqueId). Dropping.");
            return;
        }

        auto link = nm->ensurePeerLink(from);
        nm->upsertPeerIdentityWithUnique(from, uniqueId, username);
        link->setRemoteDescription(rtc::Description(sdp, std::string(msg::signaling::Offer)));
        link->createAnswer();
        return;
    }

    if (type == msg::signaling::Answer)
    {
        const std::string from = j.value(msg::key::From, "");
        const std::string sdp = j.value(msg::key::Sdp, "");
        const std::string username = j.value(msg::key::Username, "guest_" + from);
        const std::string uniqueId = j.value(std::string(msg::key::UniqueId), "");
        if (from.empty() || sdp.empty() || uniqueId.empty())
        {
            Logger::instance().log("net", Logger::Level::Warn,
                                   "Signaling missing fields (from/sdp/uniqueId). Dropping.");
            return;
        }

        auto link = nm->ensurePeerLink(from);
        nm->upsertPeerIdentityWithUnique(from, uniqueId, username);
        link->setRemoteDescription(rtc::Description(sdp, std::string(msg::signaling::Answer)));
        return;
    }

    if (type == msg::signaling::Candidate)
    {
        const std::string from = j.value(msg::key::From, "");
        const std::string cand = j.value(msg::key::Candidate, "");
        const std::string mid = j.value(msg::key::SdpMid, "");
        if (from.empty() || cand.empty())
            return;

        auto link = nm->ensurePeerLink(from);
        link->addIceCandidate(rtc::Candidate(cand, mid));
        return;
    }
}

void SignalingClient::close()
{
    NetworkUtilities::safeCloseWebSocket(ws);
    ws.reset();
}

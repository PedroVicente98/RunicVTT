#include "PeerLink.h"
#include "NetworkManager.h"
#include "Message.h"

PeerLink::PeerLink(const std::string& id, std::weak_ptr<NetworkManager> parent) :
    peerId(id), network_manager(parent)
{
    if (auto nm = network_manager.lock())
    {
        auto config = nm->getRTCConfig();
        config.iceServers.push_back({"stun:stun.l.google.com:19302"});    // Google
        config.iceServers.push_back({"stun:stun1.l.google.com:19302"});   // Google alt
        config.iceServers.push_back({"stun:stun.stunprotocol.org:3478"}); // Community server
        pc = std::make_shared<rtc::PeerConnection>(config);
        setupCallbacks();
    }
    else
    {
        throw std::runtime_error("NetworkManager expired");
    }
}

// PeerLink.cpp
void PeerLink::setDisplayName(std::string n)
{
    displayName_ = std::move(n);
}

const std::string& PeerLink::displayName() const
{
    return displayName_;
}

//void PeerLink::send(const std::string& msg) {
//    if (dc && dc->isOpen()) {
//        dc->send(msg);
//    }
//}

//void PeerLink::createDataChannel(const std::string& label) {
//    if (!pc) return;                       // ensure pc exists
//    if (dc && dc->isOpen()) return;        // idempotent-ish: keep existing dc
//
//    dc = pc->createDataChannel(label);     // negotiated=false by default
//    attachChannelHandlers(dc);
//}

void PeerLink::createChannels()
{
    if (!pc)
        return;

    rtc::DataChannelInit init; // default reliable/ordered
    // Offerer creates channels; answerer gets them in pc->onDataChannel
    auto dcGame = pc->createDataChannel(std::string(msg::dc::name::Game), init);
    dcs_[std::string(msg::dc::name::Game)] = dcGame;
    attachChannelHandlers(dcGame, std::string(msg::dc::name::Game));

    auto dcChat = pc->createDataChannel(std::string(msg::dc::name::Chat), init);
    dcs_[std::string(msg::dc::name::Chat)] = dcChat;
    attachChannelHandlers(dcChat, std::string(msg::dc::name::Chat));

    auto dcNotes = pc->createDataChannel(std::string(msg::dc::name::Notes), init);
    dcs_[std::string(msg::dc::name::Notes)] = dcNotes;
    attachChannelHandlers(dcNotes, std::string(msg::dc::name::Notes));
}

rtc::Description PeerLink::createOffer()
{
    auto offer = pc->createOffer();
    rtc::LocalDescriptionInit init;
    init.icePwd = offer.icePwd();     // string
    init.iceUfrag = offer.iceUfrag(); // string
    pc->setLocalDescription(offer.type(), init);
    return offer;
}

rtc::Description PeerLink::createAnswer()
{
    auto answer = pc->createAnswer();
    rtc::LocalDescriptionInit init;
    init.icePwd = answer.icePwd();
    init.iceUfrag = answer.iceUfrag();
    pc->setLocalDescription(answer.type(), init);
    return answer; // Send this via signaling
}

void PeerLink::setRemoteDescription(const rtc::Description& desc)
{
    pc->setRemoteDescription(desc);
    {
        std::lock_guard<std::mutex> lk(candMx_);
        remoteDescSet_.store(true, std::memory_order_release);
        for (auto& c : pendingRemoteCandidates_)
        {
            pc->addRemoteCandidate(c);
        }
        pendingRemoteCandidates_.clear();
    }
}

void PeerLink::addIceCandidate(const rtc::Candidate& candidate)
{
    std::lock_guard<std::mutex> lk(candMx_);
    if (!remoteDescSet_.load(std::memory_order_acquire))
    {
        pendingRemoteCandidates_.push_back(candidate);
    }
    else
    {
        pc->addRemoteCandidate(candidate);
    }
}

void PeerLink::setupCallbacks()
{
    pc->onStateChange([this](rtc::PeerConnection::State s)
                      {
        lastState_ = s;
        lastStateAt_ = std::chrono::seconds().count();
        std::cout << "[PeerLink] State(" << peerId << "): " << (int)s << "at " << lastStateAt_ << "\n";
       
        if (auto nm = network_manager.lock()) {
            using L = ImGuiToaster::Level;
            const char* stateStr =
                s == rtc::PeerConnection::State::Connected    ? "Connected" :
                s == rtc::PeerConnection::State::Connecting   ? "Connecting" :
                s == rtc::PeerConnection::State::Disconnected ? "Disconnected" :
                s == rtc::PeerConnection::State::Failed       ? "Failed" :
                s == rtc::PeerConnection::State::Closed       ? "Closed" : "New";
            L lvl =
                s == rtc::PeerConnection::State::Connected    ? L::Good :
                s == rtc::PeerConnection::State::Connecting   ? L::Warning :
                s == rtc::PeerConnection::State::Disconnected ? L::Error :
                s == rtc::PeerConnection::State::Failed       ? L::Error :
                s == rtc::PeerConnection::State::Closed       ? L::Warning : L::Info;
            nm->pushStatusToast(std::string("[Peer] ") + peerId + " " + stateStr, lvl);
        } });

    pc->onLocalDescription([wk = network_manager, id = peerId](rtc::Description desc)
                           {
        if (auto nm = wk.lock()) nm->onPeerLocalDescription(id, desc); });

    pc->onLocalCandidate([wk = network_manager, id = peerId](rtc::Candidate cand)
                         {
        if (auto nm = wk.lock()) nm->onPeerLocalCandidate(id, cand); });

    pc->onDataChannel([this](std::shared_ptr<rtc::DataChannel> ch)
                      {
        const std::string label = ch->label();
        dcs_[label] = ch;
        attachChannelHandlers(ch, label);
        std::cout << "[PeerLink] Received DC \"" << label << "\" from " << peerId << "\n"; });
}
//
//void PeerLink::sendOn(const std::string& label, const std::vector<std::byte>& bytes) {
//    auto it = dcs_.find(label);
//    if (it == dcs_.end() || !it->second || !it->second->isOpen()) return;
//    it->second->send(&bytes.front(), bytes.size());
//}

bool PeerLink::sendOn(const std::string& label, const std::vector<uint8_t>& bytes)
{
    auto it = dcs_.find(label);
    if (it == dcs_.end() || !it->second)
        return false;
    auto& ch = it->second;
    if (!ch->isOpen())
        return false;

    // Optional backpressure guard (avoid unbounded memory use)
    if (ch->bufferedAmount() > kMaxBufferedBytes)
    {
        // You can queue locally instead of dropping, if you want
        return false;
    }
    rtc::binary b;
    b.resize(bytes.size());
    if (!bytes.empty())
    {
        std::memcpy(b.data(), bytes.data(), bytes.size()); // std::byte is trivially copyable
    }
    ch->send(b); // libdatachannel handles SCTP fragmentation
    return true;
}
bool PeerLink::sendGame(const std::vector<uint8_t>& bytes)
{
    return sendOn(std::string(msg::dc::name::Game), bytes);
}

bool PeerLink::allRequiredOpen() const
{
    // minimal: only require the channel used for snapshots
    auto it = dcOpen_.find(msg::dc::name::Game);
    return it != dcOpen_.end() && it->second;
}

void PeerLink::attachChannelHandlers(const std::shared_ptr<rtc::DataChannel>& dc, const std::string& label)
{
    if (!dc)
        return;

    dc->onOpen([this, id = peerId, label]()
               {
        std::cout << "[PeerLink] DC open \"" << label << "\" to " << id << "\n";
        dcOpen_[label] = true;
        if (label == msg::dc::name::Game /* or "state" if you split */) {
            if (auto nm = network_manager.lock()) {
                nm->onPeerChannelOpen(peerId, label);
            }
        } });

    dc->onClosed([this, id = peerId, label]()
                 {
        std::cout << "[PeerLink] DC closed \"" << label << "\" to " << id << "\n";
        dcOpen_[label] = false;
        bootstrapSent_ = false; });

    dc->onMessage([this, label](rtc::message_variant m)
                  {
        if (std::holds_alternative<std::string>(m)) {
            const auto& s = std::get<std::string>(m);
            // Route text by label if you wish; for now, just log & (optionally) notify NM later
            if (label == msg::dc::name::Chat) {
                // TODO: add NM hook: nm->onPeerChatText(peerId, s);
                std::cout << "[PeerLink] CHAT(" << peerId << "): " << s << "\n";
            }
            else {
                std::cout << "[PeerLink] TEXT(" << label << " from " << peerId << "): " << s << "\n";
            }
            return;
        }

        // binary
        const auto& bin = std::get<rtc::binary>(m);
        std::vector<uint8_t> bytes(bin.size());
        std::memcpy(bytes.data(), bin.data(), bin.size()); 

        if (auto nm = network_manager.lock()) {
            if (label == msg::dc::name::Game) {
                // Your existing binary entrypoint:
                nm->onDcGameBinary(peerId, bytes);
            }
            else if (label == msg::dc::name::Chat) {
                // Optional: add nm->onPeerChatBinary(peerId, bytes) later
                std::cout << "[PeerLink] CHAT/BIN " << bytes.size() << "B from " << peerId << "\n";
            }
            else if (label == msg::dc::name::Notes) {
                // Optional: nm->onPeerNotesBinary(peerId, bytes)
                std::cout << "[PeerLink] NOTES/BIN " << bytes.size() << "B from " << peerId << "\n";
            }
            else {
                // Unknown label, forward to generic handler if you add one later
                std::cout << "[PeerLink] DC(" << label << ") BIN " << bytes.size() << "B from " << peerId << "\n";
            }
        } });
}

bool PeerLink::isConnected() const
{
    // â€œusableâ€ = PC connected AND at least Game channel open
    auto it = dcs_.find(std::string(msg::dc::name::Game));
    return isPcConnectedOnly() && it != dcs_.end() && it->second && it->second->isOpen();
}

bool PeerLink::isDataChannelOpen() const
{
    for (auto& [label, ch] : dcs_)
    {
        if (ch && ch->isOpen())
            return true;
    }
    return false;
}

bool PeerLink::isPcConnectedOnly() const
{
    return pc && pc->state() == rtc::PeerConnection::State::Connected;
}

void PeerLink::close()
{
    // Close datachannel first
    for (auto& [label, ch] : dcs_)
    {
        if (ch)
        {
            try
            {
                ch->close();
            }
            catch (...)
            {
            }
        }
    }
    dcs_.clear();

    if (pc)
    {
        try
        {
            pc->close();
        }
        catch (...)
        {
        }
    }
    pc.reset();
    if (auto nm = network_manager.lock())
        nm->removePeer(peerId);
}

rtc::PeerConnection::State PeerLink::pcState() const
{
    if (!pc)
        return rtc::PeerConnection::State::Closed;
    return pc->state();
}

bool PeerLink::isClosedOrFailed() const
{
    if (!pc)
        return true;
    auto s = pc->state();
    return s == rtc::PeerConnection::State::Closed ||
           s == rtc::PeerConnection::State::Failed;
}

const char* PeerLink::pcStateString() const
{
    auto s = pcState();
    using S = rtc::PeerConnection::State;
    switch (s)
    {
        case S::New:
            return "New";
        case S::Connecting:
            return "Connecting";
        case S::Connected:
            return "Connected";
        case S::Disconnected:
            return "Disconnected";
        case S::Failed:
            return "Failed";
        case S::Closed:
            return "Closed";
    }
    return "Unknown";
}

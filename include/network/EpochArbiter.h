// EpochArbiter.h
#pragma once
#include <unordered_map>
#include <string>
#include <cstdint>
#include <optional>
#include <chrono>
#include <vector>
#include "Message.h"       // your msg::ReadyMessage

struct EA_Pos { int x{0}, y{0}; };
enum class EA_Role : uint8_t { GAMEMASTER=0, PLAYER=1 };

struct EA_MoveMsg {
    uint64_t boardId{0};
    uint64_t markerId{0};
    EA_Pos   pos{};
    bool     isDragging{true};
    std::string fromPeer;
    EA_Role senderRole{EA_Role::PLAYER};
    std::optional<uint32_t> dragEpoch;
    std::optional<uint32_t> seq;
    std::optional<uint64_t> tsMs;
};

struct EA_FinalMsg {
    uint64_t boardId{0};
    uint64_t markerId{0};
    std::optional<EA_Pos> pos;
    bool     isDragging{false};
    std::string fromPeer;
    EA_Role senderRole{EA_Role::PLAYER};
    std::optional<uint32_t> dragEpoch;
    std::optional<uint32_t> seq;
    std::optional<uint64_t> tsMs;
};

struct EA_DragState {
    uint32_t epoch{0};
    bool     closed{true};
    uint32_t lastSeq{0};
    std::string ownerPeerId;
    uint64_t  lastFinalTsMs{0};
    bool     locallyDragging{false};
    uint32_t locallyProposedEpoch{0};
    uint32_t localSeq{0};
    uint64_t lastMoveRxMs{0};
    uint64_t lastMoveTxMs{0};
    uint64_t lastActivityMs{0};
    uint64_t epochOpenedMs{0};
};

struct EA_Config {
    uint32_t sendMoveMinPeriodMs     = 50;    // ~20Hz
    uint32_t dragInactivityTimeoutMs = 2000;  // report-only
    uint32_t dragMaxDurationMs       = 15000; // report-only
    uint32_t dropOlderThanMs         = 0;     // disabled
};

struct EA_WatchdogEvent {
    enum class Reason { Inactivity, MaxDuration, OwnerDisconnected };
    uint64_t markerId;
    Reason reason;
    uint32_t epoch;
    std::string ownerPeerId;
};

class EpochArbiter {
public:
    using Clock = std::chrono::steady_clock;
    explicit EpochArbiter(EA_Config cfg = {}) : cfg_(cfg) {}

    // ----- Local input lifecycle -----
    void onLocalDragStart(uint64_t markerId, const std::string& myPeerId) {
        auto& s = st_[markerId];
        s.locallyDragging = true;
        s.locallyProposedEpoch = (s.closed ? (s.epoch + 1) : s.epoch);
        s.localSeq = 0;
        s.ownerPeerId = myPeerId;
        s.epochOpenedMs = nowMs();
        s.lastActivityMs = s.epochOpenedMs;
        s.closed = false;
        if (s.locallyProposedEpoch > s.epoch) s.epoch = s.locallyProposedEpoch;
    }

    bool shouldSendMoveNow(uint64_t markerId) {
        auto& s = st_[markerId];
        uint64_t t = nowMs();
        if (t - s.lastMoveTxMs >= cfg_.sendMoveMinPeriodMs) {
            s.lastMoveTxMs = t;
            return true;
        }
        return false;
    }

    EA_MoveMsg buildOutgoingMove(uint64_t boardId, uint64_t markerId,
                                 const EA_Pos& pos,
                                 const std::string& myPeerId,
                                 EA_Role role) {
        auto& s = st_[markerId];
        EA_MoveMsg m;
        m.boardId = boardId;
        m.markerId= markerId;
        m.pos     = pos;
        m.isDragging = true;
        m.fromPeer = myPeerId;
        m.senderRole = role;
        m.dragEpoch = s.locallyProposedEpoch;
        m.seq = ++s.localSeq;
        m.tsMs = nowMs();
        s.lastActivityMs = *m.tsMs;
        return m;
    }

    EA_FinalMsg buildOutgoingFinal(uint64_t boardId, uint64_t markerId,
                                   const std::optional<EA_Pos>& finalPos,
                                   const std::string& myPeerId,
                                   EA_Role role) {
        auto& s = st_[markerId];
        EA_FinalMsg m;
        m.boardId = boardId;
        m.markerId= markerId;
        m.pos     = finalPos;
        m.isDragging = false;
        m.fromPeer = myPeerId;
        m.senderRole = role;
        m.dragEpoch = s.locallyProposedEpoch;
        m.seq = ++s.localSeq;
        m.tsMs = nowMs();
        s.locallyDragging = false; // local UX stop
        s.lastActivityMs = *m.tsMs;
        return m;
    }

    // ----- Inbound arbitration -----
    bool acceptIncomingMove(const EA_MoveMsg& m) {
        if (!m.dragEpoch) return false;
        if (cfg_.dropOlderThanMs && m.tsMs && (nowMs() - *m.tsMs) > cfg_.dropOlderThanMs) return false;

        auto& s = st_[m.markerId];
        if (*m.dragEpoch < s.epoch) return false;
        if (*m.dragEpoch > s.epoch) adoptEpoch_(s, *m.dragEpoch, m.fromPeer);
        else {
            if (s.closed) return false;
            if (s.ownerPeerId != m.fromPeer) {
                if (!ownerWins_(m.fromPeer, s.ownerPeerId)) return false;
                s.ownerPeerId = m.fromPeer;
                if (s.locallyDragging) { s.locallyDragging = false; s.localSeq=0; }
            }
        }
        if (m.seq && *m.seq <= s.lastSeq) return false;
        if (m.seq) s.lastSeq = *m.seq;

        // local echo suppression
        if (s.locallyDragging) return false;

        s.lastMoveRxMs = nowMs();
        s.lastActivityMs = s.lastMoveRxMs;
        return true;
    }

    bool acceptIncomingFinal(const EA_FinalMsg& m) {
        if (!m.dragEpoch) return false;
        auto& s = st_[m.markerId];
        if (*m.dragEpoch < s.epoch) return false;
        if (*m.dragEpoch > s.epoch) adoptEpoch_(s, *m.dragEpoch, m.fromPeer);
        else {
            if (s.closed) return false;
            if (s.ownerPeerId != m.fromPeer) {
                if (!ownerWins_(m.fromPeer, s.ownerPeerId)) return false;
                s.ownerPeerId = m.fromPeer;
                if (s.locallyDragging) { s.locallyDragging = false; s.localSeq=0; }
            }
            if (m.seq && *m.seq < s.lastSeq) return false;
            if (m.seq && *m.seq >= s.lastSeq) s.lastSeq = *m.seq;
        }
        // Accept final (hard stop)
        s.closed = true;
        s.lastFinalTsMs = m.tsMs.value_or(nowMs());
        s.lastActivityMs = s.lastFinalTsMs;
        s.locallyDragging = false;
        s.localSeq = 0;
        return true;
    }

    // ----- Watchdogs (report-only) -----
    std::vector<EA_WatchdogEvent> pollWatchdogs() const {
        std::vector<EA_WatchdogEvent> out;
        const uint64_t t = nowMs();
        for (auto& [markerId, s] : st_) {
            if (s.closed) continue;
            if ((t - s.lastMoveRxMs > cfg_.dragInactivityTimeoutMs) &&
                (t - s.lastActivityMs  > cfg_.dragInactivityTimeoutMs)) {
                out.push_back({markerId, EA_WatchdogEvent::Reason::Inactivity, s.epoch, s.ownerPeerId});
            }
            if (t - s.epochOpenedMs > cfg_.dragMaxDurationMs) {
                out.push_back({markerId, EA_WatchdogEvent::Reason::MaxDuration, s.epoch, s.ownerPeerId});
            }
        }
        return out;
    }

    void forceClose(uint64_t markerId) {
        if (auto it = st_.find(markerId); it != st_.end()) {
            auto& s = it->second;
            s.closed = true;
            s.locallyDragging = false;
            s.localSeq = 0;
            s.lastActivityMs = nowMs();
        }
    }

    std::vector<EA_WatchdogEvent> onPeerDisconnectedSuggest(const std::string& peerId) const {
        std::vector<EA_WatchdogEvent> out;
        for (auto& [markerId, s] : st_) {
            if (!s.closed && s.ownerPeerId == peerId) {
                out.push_back({markerId, EA_WatchdogEvent::Reason::OwnerDisconnected, s.epoch, s.ownerPeerId});
            }
        }
        return out;
    }

    // Queries
    bool isLocallyDragging(uint64_t markerId) const {
        if (auto it = st_.find(markerId); it != st_.end()) return it->second.locallyDragging;
        return false;
    }
    uint32_t currentEpoch(uint64_t markerId) const {
        if (auto it = st_.find(markerId); it != st_.end()) return it->second.epoch;
        return 0;
    }
    const std::string& currentOwner(uint64_t markerId) const {
        static const std::string kNone{};
        if (auto it = st_.find(markerId); it != st_.end()) return it->second.ownerPeerId;
        return kNone;
    }
  
    inline EA_Role toEA_Role(Role r) {
        return (r==Role::GAMEMASTER ? EA_Role::GAMEMASTER : EA_Role::PLAYER);
    }
    
    inline Role fromEA_Role(EA_Role r) {
        return (r==EA_Role::GAMEMASTER ? Role::GAMEMASTER : Role::PLAYER);
    }
    
    inline EA_MoveMsg toEA_Move(const msg::ReadyMessage& m) {
        EA_MoveMsg e;
        e.boardId   = m.boardId.value_or(0);
        e.markerId  = m.markerId.value_or(0);
        if (m.pos) e.pos = EA_Pos{m.pos->x, m.pos->y};
        e.isDragging= m.mov ? m.mov->isDragging : true;
        e.fromPeer  = m.fromPeer;
        e.senderRole= m.senderRole ? toEA_Role(*m.senderRole) : EA_Role::PLAYER;
        if (m.dragEpoch) e.dragEpoch = *m.dragEpoch;
        if (m.seq)       e.seq       = *m.seq;
        if (m.ts)        e.tsMs      = *m.ts;
        return e;
    }
    
    inline EA_FinalMsg toEA_Final(const msg::ReadyMessage& m) {
        EA_FinalMsg e;
        e.boardId   = m.boardId.value_or(0);
        e.markerId  = m.markerId.value_or(0);
        if (m.pos) e.pos = EA_Pos{m.pos->x, m.pos->y};
        e.isDragging= false;
        e.fromPeer  = m.fromPeer;
        e.senderRole= m.senderRole ? toEA_Role(*m.senderRole) : EA_Role::PLAYER;
        if (m.dragEpoch) e.dragEpoch = *m.dragEpoch;
        if (m.seq)       e.seq       = *m.seq;
        if (m.ts)        e.tsMs      = *m.ts;
        return e;
    }
    
    inline msg::ReadyMessage fromEA(const EA_MoveMsg& e) {
        msg::ReadyMessage m;
        m.kind     = msg::DCType::MarkerMove;
        m.fromPeer = e.fromPeer;
        m.boardId  = e.boardId;
        m.markerId = e.markerId;
        m.pos      = Position{e.pos.x, e.pos.y};
        m.mov      = Moving{true};
        m.dragEpoch= e.dragEpoch;
        m.seq      = e.seq;
        m.ts       = e.tsMs;
        m.senderRole = fromEA_Role(e.senderRole);
        return m;
    }
    
    inline msg::ReadyMessage fromEA(const EA_FinalMsg& e) {
        msg::ReadyMessage m;
        m.kind     = msg::DCType::MarkerUpdate;
        m.fromPeer = e.fromPeer;
        m.boardId  = e.boardId;
        m.markerId = e.markerId;
        if (e.pos) m.pos = Position{e.pos->x, e.pos->y};
        m.mov      = Moving{false};
        m.dragEpoch= e.dragEpoch;
        m.seq      = e.seq;
        m.ts       = e.tsMs;
        m.senderRole = fromEA_Role(e.senderRole);
        return m;
    }
private:
    EA_Config cfg_;
    mutable std::unordered_map<uint64_t, EA_DragState> st_;

    static uint64_t nowMs() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(Clock::now().time_since_epoch()).count();
    }
    static bool ownerWins_(const std::string& challenger, const std::string& current) {
        return (challenger < current); // deterministic tie-break
    }
    static void adoptEpoch_(EA_DragState& s, uint32_t newEpoch, const std::string& owner) {
        s.epoch = newEpoch;
        s.closed = false;
        s.lastSeq = 0;
        s.ownerPeerId = owner;
        s.epochOpenedMs = nowMs();
        s.lastMoveRxMs = 0;
        s.lastActivityMs = s.epochOpenedMs;
    }


};





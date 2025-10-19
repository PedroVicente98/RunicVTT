//// EpochArbiter.h  (final form for 3-op model)
//#pragma once
//#include <unordered_map>
//#include <string>
//#include <cstdint>
//#include <optional>
//#include <chrono>
//#include "Message.h"
//
//struct EA_DragState {
//    uint32_t epoch{0};
//    bool     closed{true};
//    uint32_t lastSeq{0};
//    std::string ownerPeerId;
//    bool     locallyDragging{false};
//    uint32_t locallyProposedEpoch{0};
//    uint32_t localSeq{0};
//    uint64_t lastMoveRxMs{0};
//    uint64_t lastMoveTxMs{0};
//    uint64_t lastActivityMs{0};
//    uint64_t epochOpenedMs{0};
//};
//
//struct EA_Config {
//    uint32_t sendMoveMinPeriodMs = 50; // ~20Hz
//    // report-only timers (kept for future use; not auto-closing)
//    uint32_t dragInactivityTimeoutMs = 2000;
//    uint32_t dragMaxDurationMs = 15000;
//};
//
//class EpochArbiter {
//public:
//    using Clock = std::chrono::steady_clock;
//    explicit EpochArbiter(EA_Config cfg = {}) : cfg_(cfg) {}
//
//    // Local lifecycle
//    void onLocalDragStart(uint64_t markerId, const std::string& myPeerId) {
//        auto& s = st_[markerId];
//        s.locallyDragging = true;
//        s.locallyProposedEpoch = (s.closed ? (s.epoch + 1) : s.epoch);
//        s.localSeq = 0;
//        s.ownerPeerId = myPeerId;
//        s.epochOpenedMs = nowMs();
//        s.lastActivityMs = s.epochOpenedMs;
//        s.closed = false;
//        if (s.locallyProposedEpoch > s.epoch) s.epoch = s.locallyProposedEpoch;
//    }
//    bool shouldSendMoveNow(uint64_t markerId) {
//        auto& s = st_[markerId];
//        const uint64_t t = nowMs();
//        if (t - s.lastMoveTxMs >= cfg_.sendMoveMinPeriodMs) { s.lastMoveTxMs = t; return true; }
//        return false;
//    }
//
//    // OUT: build move (unreliable)
//    msg::ReadyMessage buildMove(uint64_t boardId, uint64_t markerId,
//                                Position pos, const std::string& myPeerId, Role myRole) {
//        auto& s = st_[markerId];
//        msg::ReadyMessage m;
//        m.kind      = msg::DCType::MarkerMove;
//        m.boardId   = boardId;
//        m.markerId  = markerId;
//        m.pos       = pos;
//        m.mov       = Moving{true};
//        m.fromPeer  = myPeerId;
//        m.senderRole= myRole;
//        m.dragEpoch = s.locallyProposedEpoch;
//        m.seq       = ++s.localSeq;
//        m.ts        = nowMs();
//        s.lastActivityMs = *m.ts;
//        return m;
//    }
//
//    // OUT: build move state START (reliable)
//    msg::ReadyMessage buildMoveStateStart(uint64_t boardId, uint64_t markerId,
//                                          const std::string& myPeerId, Role myRole) {
//        auto& s = st_[markerId];
//        msg::ReadyMessage m;
//        m.kind      = msg::DCType::MarkerMoveState;
//        m.boardId   = boardId;
//        m.markerId  = markerId;
//        m.mov       = Moving{true};     // start drag
//        m.fromPeer  = myPeerId;
//        m.senderRole= myRole;
//        m.dragEpoch = s.locallyProposedEpoch;
//        m.seq       = ++s.localSeq;
//        m.ts        = nowMs();
//        s.lastActivityMs = *m.ts;
//        return m;
//    }
//
//    // OUT: build move state FINAL (reliable)
//    msg::ReadyMessage buildMoveStateFinal(uint64_t boardId, uint64_t markerId,
//                                          std::optional<Position> finalPos,
//                                          const std::string& myPeerId, Role myRole) {
//        auto& s = st_[markerId];
//        msg::ReadyMessage m;
//        m.kind      = msg::DCType::MarkerMoveState;
//        m.boardId   = boardId;
//        m.markerId  = markerId;
//        if (finalPos) m.pos = *finalPos; // carry final position if available
//        m.mov       = Moving{false};      // end drag
//        m.fromPeer  = myPeerId;
//        m.senderRole= myRole;
//        m.dragEpoch = s.locallyProposedEpoch;
//        m.seq       = ++s.localSeq;
//        m.ts        = nowMs();
//        // local UX stop
//        s.locallyDragging = false;
//        s.lastActivityMs  = *m.ts;
//        return m;
//    }
//
//    // IN: MarkerMove (in-drag)
//    bool acceptIncomingMove(const msg::ReadyMessage& m) {
//        if (!m.markerId || !m.dragEpoch) return false;
//        auto& s = st_[*m.markerId];
//
//        if (*m.dragEpoch < s.epoch) return false;
//        if (*m.dragEpoch > s.epoch) adoptEpoch_(s, *m.dragEpoch, m.fromPeer);
//        else {
//            if (s.closed) return false;
//            if (s.ownerPeerId != m.fromPeer) {
//                if (!ownerWins_(m.fromPeer, s.ownerPeerId)) return false;
//                s.ownerPeerId = m.fromPeer;
//                if (s.locallyDragging) { s.locallyDragging = false; s.localSeq = 0; }
//            }
//        }
//        if (m.seq && *m.seq <= s.lastSeq) return false;
//        if (m.seq) s.lastSeq = *m.seq;
//
//        // local echo suppression
//        if (s.locallyDragging) return false;
//
//        s.lastMoveRxMs = nowMs();
//        s.lastActivityMs = s.lastMoveRxMs;
//        return true;
//    }
//
//    // IN: MarkerMoveState START
//    bool acceptIncomingMoveStateStart(const msg::ReadyMessage& m) {
//        if (!m.markerId || !m.dragEpoch || !m.mov || !m.mov->isDragging) return false;
//        auto& s = st_[*m.markerId];
//
//        if (*m.dragEpoch < s.epoch) return false;
//        if (*m.dragEpoch > s.epoch) adoptEpoch_(s, *m.dragEpoch, m.fromPeer);
//        else {
//            if (s.closed) return false;
//            if (s.ownerPeerId != m.fromPeer) {
//                if (!ownerWins_(m.fromPeer, s.ownerPeerId)) return false;
//                s.ownerPeerId = m.fromPeer;
//                if (s.locallyDragging) { s.locallyDragging = false; s.localSeq = 0; }
//            }
//        }
//        if (m.seq && *m.seq <= s.lastSeq) return false;
//        if (m.seq) s.lastSeq = *m.seq;
//
//        // this only announces start; applying ECS Moving(true) is up to caller
//        return true;
//    }
//
//    // IN: MarkerMoveState FINAL
//    bool acceptIncomingMoveStateFinal(const msg::ReadyMessage& m) {
//        if (!m.markerId || !m.dragEpoch || !m.mov || m.mov->isDragging) return false;
//        auto& s = st_[*m.markerId];
//
//        if (*m.dragEpoch < s.epoch) return false;
//        if (*m.dragEpoch > s.epoch) adoptEpoch_(s, *m.dragEpoch, m.fromPeer);
//        else {
//            if (s.closed) return false;
//            if (s.ownerPeerId != m.fromPeer) {
//                if (!ownerWins_(m.fromPeer, s.ownerPeerId)) return false;
//                s.ownerPeerId = m.fromPeer;
//                if (s.locallyDragging) { s.locallyDragging = false; s.localSeq = 0; }
//            }
//            if (m.seq && *m.seq < s.lastSeq) return false;
//            if (m.seq && *m.seq >= s.lastSeq) s.lastSeq = *m.seq;
//        }
//        // hard stop
//        s.closed = true;
//        s.locallyDragging = false;
//        s.localSeq = 0;
//        return true;
//    }
//
//    void forceClose(uint64_t markerId) {
//        if (auto it = st_.find(markerId); it != st_.end()) {
//            auto& s = it->second;
//            s.closed = true;
//            s.locallyDragging = false;
//            s.localSeq = 0;
//            s.lastActivityMs = nowMs();
//        }
//    }
//
//private:
//    EA_Config cfg_;
//    mutable std::unordered_map<uint64_t, EA_DragState> st_;
//    static uint64_t nowMs() {
//        using namespace std::chrono;
//        return duration_cast<milliseconds>(Clock::now().time_since_epoch()).count();
//    }
//    static bool ownerWins_(const std::string& challenger, const std::string& current) {
//        return (challenger < current); // deterministic tiebreak
//    }
//    static void adoptEpoch_(EA_DragState& s, uint32_t newEpoch, const std::string& owner) {
//        s.epoch = newEpoch;
//        s.closed = false;
//        s.lastSeq = 0;
//        s.ownerPeerId = owner;
//        s.epochOpenedMs = nowMs();
//        s.lastMoveRxMs = 0;
//        s.lastActivityMs = s.epochOpenedMs;
//    }
//};

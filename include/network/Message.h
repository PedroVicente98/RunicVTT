// message.h
#pragma once
#include <string>
#include <string_view>
#include <cstdint>
#include "nlohmann/json.hpp"

// If you use nlohmann::json in this TU, include it once anywhere before using helpers.
// #include <nlohmann/json.hpp>

namespace msg {
    
    // ---------- Common JSON keys (shared) ----------
    namespace key {
        inline constexpr std::string_view Type = "type";
        inline constexpr std::string_view From = "from";
        inline constexpr std::string_view To = "to";
        inline constexpr std::string_view Broadcast = "broadcast";
        inline constexpr std::string_view Timestamp = "ts";
        inline constexpr std::string_view Text = "text";
        inline constexpr std::string_view Clients = "clients";
        inline constexpr std::string_view Event = "event";
        inline constexpr std::string_view Username = "username";
        inline constexpr std::string_view ClientId = "clientId";

        // Signaling-specific
        inline constexpr std::string_view Sdp = "sdp";
        inline constexpr std::string_view Candidate = "candidate";
        inline constexpr std::string_view SdpMid = "sdpMid";
        //inline constexpr std::string_view SdpMLineIndex = "sdpMLineIndex";

        // Auth / control
        inline constexpr std::string_view AuthOk = "ok";
        inline constexpr std::string_view AuthMsg = "msg";
        inline constexpr std::string_view AuthToken = "token";
        inline constexpr std::string_view Password = "password";

        // DataChannel (game) generic keys (if you serialize as JSON)
        inline constexpr std::string_view Label = "label";     // DC label if needed
        inline constexpr std::string_view Payload = "payload";   // generic payload
        inline constexpr std::string_view EntityId = "entityId";
        inline constexpr std::string_view X = "x";
        inline constexpr std::string_view Y = "y";
        inline constexpr std::string_view Z = "z";
        inline constexpr std::string_view Visible = "visible";
        inline constexpr std::string_view ImageId = "imageId";
        inline constexpr std::string_view BytesOffset = "offset";
        inline constexpr std::string_view BytesTotal = "total";
        inline constexpr std::string_view Chunk = "chunk";
    }

    // ---------- Signaling message types (WebSocket) ----------
    namespace signaling {
        inline constexpr std::string_view Join = "join";
        inline constexpr std::string_view Offer = "offer";
        inline constexpr std::string_view Answer = "answer";
        inline constexpr std::string_view Presence = "presence";
        inline constexpr std::string_view Candidate = "candidate";
        inline constexpr std::string_view Ping = "ping";
        inline constexpr std::string_view Pong = "pong";     // if you choose to send explicit pongs
        inline constexpr std::string_view Auth = "auth";
        inline constexpr std::string_view AuthResponse = "auth_response";
        inline constexpr std::string_view Text = "text";
        inline constexpr std::string_view ServerDisconnect = "server_disconnect";
    }

    namespace value {
        inline constexpr std::string False = "false";
        inline constexpr std::string True = "true";
    }

    // ---------- DataChannel message types (game logic) ----------
    // Prefer binary with a tiny header for performance, but define string labels too
    namespace dc {
        namespace name {
            inline constexpr std::string Game = "game";
            inline constexpr std::string Chat = "chat";
            inline constexpr std::string Notes = "notes";
        }

        inline constexpr std::string_view Chat = "CHAT";
        inline constexpr std::string_view Image = "IMAGE";              // metadata/chunk control
        inline constexpr std::string_view ToggleVisibility = "TOGGLE_VISIBILITY";
        inline constexpr std::string_view CreateEntity = "CREATE_ENTITY";
        inline constexpr std::string_view Move = "MOVE";

        // Examples from earlier discussion:
        inline constexpr std::string_view MarkerMove = "MARKER_MOVE";
        inline constexpr std::string_view FogCreate = "FOG_CREATE";
        inline constexpr std::string_view FogUpdate = "FOG_UPDATE";
    }

    // ---------- Optional: numeric IDs for DC binary mux ----------
    enum class DCType : uint8_t {
        Chat = 1,
        Image = 2,
        ToggleVisibility = 3,
        CreateEntity = 4,
        Move = 5,
        MarkerMove = 6,
        FogCreate = 7,
        FogUpdate = 8,
    };

    // ========== Optional JSON helpers (nlohmann::json) ==========
    // Enable by including <nlohmann/json.hpp> before using these.
    using Json = nlohmann::json;
    // ---- builders (signaling) ----
    inline Json makeOffer(const std::string& from, const std::string& to, const std::string& sdp, const std::string& username, const std::string& broadcast = msg::value::False) {
        return Json{
            { std::string(key::Type), std::string(signaling::Offer) },
            { std::string(key::From), from },
            { std::string(key::To), to },
            { std::string(key::Broadcast), broadcast },
            { std::string(key::Sdp), sdp },
            { std::string(key::Username), username }
        };
    }
    inline Json makeAnswer(const std::string& from, const std::string& to, const std::string& sdp, const std::string& username, const std::string& broadcast = msg::value::False) {
        return Json{
            { std::string(key::Type), std::string(signaling::Answer) },
            { std::string(key::From), from },
            { std::string(key::To), to },
            { std::string(key::Broadcast), broadcast },
            { std::string(key::Sdp), sdp },
            { std::string(key::Username), username }
        };
    }
    inline Json makeCandidate(const std::string& from, const std::string& to, const std::string& cand, const std::string& broadcast = msg::value::False) {
        return Json{
            { std::string(key::Type), std::string(signaling::Candidate) },
            { std::string(key::From), from },
            { std::string(key::To), to },
            { std::string(key::Broadcast), broadcast },
            { std::string(key::Candidate), cand }/*,
            { std::string(key::SdpMid), mid },
            { std::string(key::SdpMLineIndex), mline }*/
        };
    }

    inline Json makePresence(const std::string& event, const std::string& clientId) {
        return Json{
            { std::string(key::Type), std::string(signaling::Presence) },
            { std::string(key::Event), event },
            { std::string(key::ClientId), clientId }
        };
    }

   inline Json makeBroadcastShutdown() {
       return Json{
           { std::string(msg::key::Type),      msg::signaling::ServerDisconnect },
           { std::string(msg::key::Broadcast), std::string(msg::value::True) }
       };
   }

    inline Json makePing(const std::string& from) {
        return Json{
            { std::string(key::Type), std::string(signaling::Ping) },
            { std::string(key::From), from }
        };
    }
    inline Json makeAuth(const std::string& token, const std::string& username) {
        return Json{
            { std::string(key::Type), std::string(signaling::Auth) },
            { std::string(key::AuthToken), token },
            { std::string(key::Username), username },
        };
    }

    inline Json makeAuthResponse(const std::string ok, const std::string& msg, const std::string& clientId, const std::string& username, const std::vector<std::string>& clients = {}) {
        
         auto j = Json{
            { std::string(key::Type), std::string(signaling::AuthResponse) },
            { std::string(key::AuthOk), ok },
            { std::string(key::AuthMsg), msg },
            { std::string(key::ClientId), clientId },
            { std::string(key::Username), username }
         };

         if (!clients.empty()) {
             j[std::string(msg::key::Clients)] = clients; // array of peerId strings
         };

        return j;
    }
    inline Json makeText(const std::string& from, const std::string& to, const std::string& text, bool broadcast = false) {
        return Json{
            { std::string(key::Type), std::string(signaling::Text) },
            { std::string(key::From), from },
            { std::string(key::To), to },
            { std::string(key::Broadcast), broadcast },
            { std::string(key::Text), text }
        };
    }

    // ---- predicates ----
    inline bool isType(const Json& j, std::string t) {
        auto it = j.find(std::string(key::Type));
        return it != j.end() && it->is_string() && it->get_ref<const std::string&>() == t;
    }

    // ---- accessors (safe-ish) ----
    inline std::string getString(const Json& j, std::string k, std::string def = {}) {
        auto it = j.find(std::string(k));
        return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::move(def);
    }
    inline int getInt(const Json& j, std::string k, int def = 0) {
        auto it = j.find(std::string(k));
        return (it != j.end() && it->is_number_integer()) ? it->get<int>() : def;
    }
    inline bool getBool(const Json& j, std::string k, bool def = false) {
        auto it = j.find(std::string(k));
        return (it != j.end() && it->is_boolean()) ? it->get<bool>() : def;
    }

} // namespace msg


// namespace msg
/*
#pragma once
#include
#include <iostream>
#include "nlohmann/json.hpp"

enum MessageType {
    CHAT,
    IMAGE,
    MOVE,
    CREATE_ENTITY,
    PASSWORD,
    TOGGLE_VISIBILITY,
    SEND_NOTE
};




struct Message {
    MessageType type;
    std::string senderId;
    nlohmann::json payload;
};


*/
// message.h
#pragma once
#include <string>
#include <string_view>
#include <cstdint>
#include "nlohmann/json.hpp"
#include "Components.h"

// If you use nlohmann::json in this TU, include it once anywhere before using helpers.
// #include <nlohmann/json.hpp>

enum class Role
{
    NONE,
    GAMEMASTER,
    PLAYER
};

enum class ConnectionType
{
    EXTERNAL,
    LOCAL,
    LOCALTUNNEL,
    CUSTOM
};

namespace msg
{

    enum class DCType : uint8_t
    {
        // Snapshots and Composite Operations
        Snapshot_GameTable = 100,
        Snapshot_Board = 101,
        CommitMarker = 102,
        CommitBoard = 103,
        ImageChunk = 104,

        //Operations
        MarkerMove = 150,
        MarkerMoveState = 151,
        MarkerCreate = 1,
        MarkerUpdate = 2, //Position and/or Visibility
        MarkerDelete = 3,
        FogCreate = 4,
        FogUpdate = 5, //Size and/or Visibility
        FogDelete = 6,
        GridUpdate = 7, //Position, Size and/or Visibility
        NoteCreate = 8,
        NoteUpdate = 9, //Metadata and/or Content
        NoteDelete = 10,

        UserNameUpdate = 105, // Game channel: broadcast username changes

        // chat ops (binary)
        ChatGroupCreate = 200,
        ChatGroupUpdate = 201,
        ChatGroupDelete = 202,
        ChatMessage = 203
    };
    inline std::string DCtypeString(DCType type)
    {
        std::string type_str;
        switch (type)
        {
            case msg::DCType::Snapshot_GameTable:
                type_str = "Snapshot_GameTable";
                break;
            case msg::DCType::Snapshot_Board:
                type_str = "Snapshot_Board";
                break;
            case msg::DCType::CommitMarker:
                type_str = "CommitMarker";
                break;
            case msg::DCType::CommitBoard:
                type_str = "CommitBoard";
                break;
            case msg::DCType::ImageChunk:
                type_str = "ImageChunk";
                break;
            case msg::DCType::MarkerCreate:
                type_str = "MarkerCreate";
                break;
            case msg::DCType::MarkerUpdate:
                type_str = "MarkerUpdate";
                break;
            case msg::DCType::MarkerDelete:
                type_str = "MarkerDelete";
                break;
            case msg::DCType::FogCreate:
                type_str = "FogCreate";
                break;
            case msg::DCType::FogUpdate:
                type_str = "FogUpdate";
                break;
            case msg::DCType::FogDelete:
                type_str = "FogDelete";
                break;
            case msg::DCType::GridUpdate:
                type_str = "GridUpdate";
                break;
            case msg::DCType::NoteCreate:
                type_str = "NoteCreate";
                break;
            case msg::DCType::NoteUpdate:
                type_str = "NoteUpdate";
                break;
            case msg::DCType::NoteDelete:
                type_str = "NoteDelete";
                break;
            case msg::DCType::ChatGroupCreate:
                type_str = "ChatThreadCreate";
                break;
            case msg::DCType::ChatGroupUpdate:
                type_str = "ChatThreadUpdate";
                break;
            case msg::DCType::ChatGroupDelete:
                type_str = "ChatThreadDelete";
                break;
            case msg::DCType::ChatMessage:
                type_str = "ChatMessage";
                break;
            case msg::DCType::MarkerMove:
                type_str = "MarkerMove";
                break;
            case msg::DCType::MarkerMoveState:
                type_str = "MarkerMoveState";
                break;
            case msg::DCType::UserNameUpdate:
                type_str = "UserNameUpdate";
                break;
            default:
                type_str = "UnkownType";
                break;
        }
        return type_str;
    }

    enum class ImageOwnerKind : uint8_t
    {
        Board = 0,
        Marker = 1
    };

    struct MarkerMeta
    {
        uint64_t markerId = 0;
        uint64_t boardId = 0;
        std::string name; // filename/hint if you want
        Position pos{};
        Size size{};
        Visibility vis{};
        Moving mov{};
    };

    struct BoardMeta
    {
        uint64_t boardId = 0;
        std::string boardName;
        Panning pan{};
        Grid grid{};
        Size size{};
    };

    // Single ready container with tag + optionals (only 1 engaged)
    struct ReadyMessage
    {
        DCType kind;
        std::string fromPeerId; // optional: who sent it (fill in DC callback if you have it)

        std::optional<uint64_t> tableId;
        std::optional<uint64_t> boardId;
        std::optional<uint64_t> markerId;
        std::optional<uint64_t> fogId;

        std::optional<Position> pos;
        std::optional<Size> size;
        std::optional<Visibility> vis;

        std::optional<std::string> name;
        std::optional<std::vector<uint8_t>> bytes;
        std::optional<BoardMeta> boardMeta;
        std::optional<MarkerMeta> markerMeta;

        std::optional<uint64_t> threadId;
        std::optional<uint64_t> ts;
        std::optional<std::string> text;                   // chat text
        std::optional<std::set<std::string>> participants; // thread participants

        std::optional<Moving> mov;
        std::optional<MarkerComponent> markerComp;

        std::optional<Grid> grid;

        std::optional<uint32_t> dragEpoch;
        std::optional<uint32_t> seq;
        std::optional<Role> senderRole;

        std::optional<std::string> userUniqueId;
        std::optional<uint8_t> rebound;
    };

    struct NetEvent
    {
        enum class Type
        {
            DcOpen,
            DcClosed,
            PcOpen,
            PcClosed,
            ClientOpen,
            ClientClosed
        };
        Type type;
        std::string peerId;
        std::string label;
    };

    struct InboundRaw
    {
        std::string fromPeer;
        std::string label;
        std::vector<uint8_t> bytes;
    };

    // ---------- Common JSON keys (shared) ----------
    namespace key
    {
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
        inline constexpr std::string_view Target = "target";
        inline constexpr std::string_view GmId = "gmId";
        // Signaling-specific
        inline constexpr std::string_view Sdp = "sdp";
        inline constexpr std::string_view Candidate = "candidate";
        inline constexpr std::string_view SdpMid = "sdpMid";
        //inline constexpr std::string_view SdpMLineIndex = "sdpMLineIndex";

        // Auth / control
        inline constexpr std::string_view UniqueId = "uniqueId";
        inline constexpr std::string_view AuthOk = "ok";
        inline constexpr std::string_view AuthMsg = "msg";
        inline constexpr std::string_view AuthToken = "token";
        inline constexpr std::string_view Password = "password";

        // DataChannel (game) generic keys (if you serialize as JSON)
        inline constexpr std::string_view Label = "label";     // DC label if needed
        inline constexpr std::string_view Payload = "payload"; // generic payload
        inline constexpr std::string_view EntityId = "entityId";
        inline constexpr std::string_view X = "x";
        inline constexpr std::string_view Y = "y";
        inline constexpr std::string_view Z = "z";
        inline constexpr std::string_view Visible = "visible";
        inline constexpr std::string_view ImageId = "imageId";
        inline constexpr std::string_view BytesOffset = "offset";
        inline constexpr std::string_view BytesTotal = "total";
        inline constexpr std::string_view Chunk = "chunk";
    } // namespace key

    // ---------- Signaling message types (WebSocket) ----------
    namespace signaling
    {
        inline constexpr std::string_view Join = "join";
        inline constexpr std::string_view Offer = "offer";
        inline constexpr std::string_view Answer = "answer";
        inline constexpr std::string_view Presence = "presence";
        inline constexpr std::string_view Candidate = "candidate";
        inline constexpr std::string_view Ping = "ping";
        inline constexpr std::string_view Pong = "pong"; // if you choose to send explicit pongs
        inline constexpr std::string_view Auth = "auth";
        inline constexpr std::string_view AuthResponse = "auth_response";
        inline constexpr std::string_view Text = "text";
        inline constexpr std::string_view ServerDisconnect = "server_disconnect";
        inline constexpr std::string_view PeerDisconnect = "peer_disconnect";
    } // namespace signaling

    namespace value
    {
        inline constexpr std::string False = "false";
        inline constexpr std::string True = "true";
    } // namespace value

    // ---------- DataChannel message types (game logic) ----------
    // Prefer binary with a tiny header for performance, but define string labels too
    namespace dc
    {
        namespace name
        {
            inline constexpr std::string Game = "game";
            inline constexpr std::string Chat = "chat";
            inline constexpr std::string Notes = "notes";
            inline constexpr std::string MarkerMove = "marker_move";
        } // namespace name

    } // namespace dc

    // --- DCType <-> string (chat only) ---
    inline const char* DCTypeToJson(DCType t)
    {
        switch (t)
        {
            case DCType::ChatGroupCreate:
                return "ChatGroupCreate";
            case DCType::ChatGroupUpdate:
                return "ChatGroupUpdate";
            case DCType::ChatGroupDelete:
                return "ChatGroupDelete";
            case DCType::ChatMessage:
                return "ChatMessage";
            default:
                return "";
        }
    }
    inline bool DCTypeFromJson(std::string_view s, DCType& out)
    {
        if (s == "ChatGroupCreate")
        {
            out = DCType::ChatGroupCreate;
            return true;
        }
        if (s == "ChatGroupUpdate")
        {
            out = DCType::ChatGroupUpdate;
            return true;
        }
        if (s == "ChatGroupDelete")
        {
            out = DCType::ChatGroupDelete;
            return true;
        }
        if (s == "ChatMessage")
        {
            out = DCType::ChatMessage;
            return true;
        }
        return false;
    }

    // Common payload keys for chat JSON
    namespace chatkey
    {
        inline constexpr std::string_view Type = "type";          // string (ChatMessage, ChatGroupCreate, ...)
        inline constexpr std::string_view TableId = "tableId";    // u64
        inline constexpr std::string_view GroupId = "groupId";    // u64
        inline constexpr std::string_view Name = "name";          // string (group name)
        inline constexpr std::string_view Parts = "participants"; // array<string> (peer ids)
        inline constexpr std::string_view Ts = "ts";              // u64
        inline constexpr std::string_view Username = "username";  // string
        inline constexpr std::string_view Text = "text";          // string
    } // namespace chatkey

    // ========== JSON helpers (nlohmann::json) ==========
    // Enable by including <nlohmann/json.hpp> before using these.

    using Json = nlohmann::json;

    // --- builders (chat JSON) ---
    inline Json makeChatGroupCreate(uint64_t tableId, uint64_t groupId,
                                    const std::string& name,
                                    const std::set<std::string>& participants)
    {
        Json j = {
            {std::string(chatkey::Type), DCTypeToJson(DCType::ChatGroupCreate)},
            {std::string(chatkey::TableId), tableId},
            {std::string(chatkey::GroupId), groupId},
            {std::string(chatkey::Name), name},
            {std::string(chatkey::Parts), Json::array()}};
        for (auto& p : participants)
            j[std::string(chatkey::Parts)].push_back(p);
        return j;
    }

    inline Json makeChatGroupUpdate(uint64_t tableId, uint64_t groupId,
                                    const std::string& name,
                                    const std::set<std::string>& participants)
    {
        Json j = {
            {std::string(chatkey::Type), DCTypeToJson(DCType::ChatGroupUpdate)},
            {std::string(chatkey::TableId), tableId},
            {std::string(chatkey::GroupId), groupId},
            {std::string(chatkey::Name), name},
            {std::string(chatkey::Parts), Json::array()}};
        for (auto& p : participants)
            j[std::string(chatkey::Parts)].push_back(p);
        return j;
    }

    inline Json makeChatGroupDelete(uint64_t tableId, uint64_t groupId)
    {
        return Json{
            {std::string(chatkey::Type), DCTypeToJson(DCType::ChatGroupDelete)},
            {std::string(chatkey::TableId), tableId},
            {std::string(chatkey::GroupId), groupId}};
    }

    inline Json makeChatMessage(uint64_t tableId, uint64_t groupId,
                                uint64_t ts, const std::string& username,
                                const std::string& text)
    {
        return Json{
            {std::string(chatkey::Type), DCTypeToJson(DCType::ChatMessage)},
            {std::string(chatkey::TableId), tableId},
            {std::string(chatkey::GroupId), groupId},
            {std::string(chatkey::Ts), ts},
            {std::string(chatkey::Username), username},
            {std::string(chatkey::Text), text}};
    }

    inline Json makeOffer(const std::string& from, const std::string& to,
                          const std::string& sdp, const std::string& username,
                          const std::string& uniqueId,
                          const std::string& broadcast = msg::value::False)
    {
        return Json{
            {std::string(key::Type), std::string(signaling::Offer)},
            {std::string(key::From), from},
            {std::string(key::To), to},
            {std::string(key::Broadcast), broadcast},
            {std::string(key::Sdp), sdp},
            {std::string(key::Username), username},
            {std::string(key::UniqueId), uniqueId},
        };
    }

    inline Json makeAnswer(const std::string& from, const std::string& to,
                           const std::string& sdp, const std::string& username,
                           const std::string& uniqueId,
                           const std::string& broadcast = msg::value::False)
    {
        return Json{
            {std::string(key::Type), std::string(signaling::Answer)},
            {std::string(key::From), from},
            {std::string(key::To), to},
            {std::string(key::Broadcast), broadcast},
            {std::string(key::Sdp), sdp},
            {std::string(key::Username), username},
            {std::string(key::UniqueId), uniqueId},
        };
    }
    inline Json makeCandidate(const std::string& from, const std::string& to, const std::string& cand, const std::string& broadcast = msg::value::False)
    {
        return Json{
            {std::string(key::Type), std::string(signaling::Candidate)},
            {std::string(key::From), from},
            {std::string(key::To), to},
            {std::string(key::Broadcast), broadcast},
            {std::string(key::Candidate), cand}};
    }

    inline Json makePresence(const std::string& event, const std::string& clientId)
    {
        return Json{
            {std::string(key::Type), std::string(signaling::Presence)},
            {std::string(key::Event), event},
            {std::string(key::ClientId), clientId}};
    }

    inline nlohmann::json makePeerDisconnect(const std::string& targetPeerId, bool broadcast = true)
    {
        return nlohmann::json{
            {std::string(key::Type), std::string(signaling::PeerDisconnect)},
            {std::string(key::Broadcast), broadcast ? std::string(msg::value::True) : std::string(msg::value::False)},
            {std::string(key::Target), targetPeerId}};
    }

    inline Json makeBroadcastShutdown()
    {
        return Json{
            {std::string(msg::key::Type), msg::signaling::ServerDisconnect},
            {std::string(msg::key::Broadcast), std::string(msg::value::True)}};
    }

    inline Json makePing(const std::string& from)
    {
        return Json{
            {std::string(key::Type), std::string(signaling::Ping)},
            {std::string(key::From), from}};
    }
    /* inline Json makeAuth(const std::string& token, const std::string& username)
    {
        return Json{
            {std::string(key::Type), std::string(signaling::Auth)},
            {std::string(key::AuthToken), token},
            {std::string(key::Username), username},
        };
    }*/
    inline Json makeAuth(const std::string& token,
                         const std::string& username,
                         const std::string& uniqueId)
    {
        return Json{
            {std::string(key::Type), std::string(signaling::Auth)},
            {std::string(key::AuthToken), token},
            {std::string(key::Username), username},
            {std::string(key::UniqueId), uniqueId},
        };
    }
    inline nlohmann::json makeAuthResponse(const std::string ok, const std::string& msg,
                                           const std::string& clientId, const std::string& username,
                                           const std::vector<std::string>& clients = {},
                                           const std::string& gmPeerId = "",
                                           const std::string& uniqueId = "")
    {
        auto j = nlohmann::json{
            {std::string(key::Type), std::string(signaling::AuthResponse)},
            {std::string(key::AuthOk), ok},
            {std::string(key::AuthMsg), msg},
            {std::string(key::ClientId), clientId},
            {std::string(key::Username), username}};
        if (!clients.empty())
            j[std::string(msg::key::Clients)] = clients;
        if (!gmPeerId.empty())
            j[key::GmId] = gmPeerId;
        if (!uniqueId.empty())
            j[std::string(key::UniqueId)] = uniqueId;
        return j;
    }
    /* inline Json makeAuthResponse(const std::string ok, const std::string& msg, const std::string& clientId, const std::string& username, const std::vector<std::string>& clients = {})
    {

        auto j = Json{
            {std::string(key::Type), std::string(signaling::AuthResponse)},
            {std::string(key::AuthOk), ok},
            {std::string(key::AuthMsg), msg},
            {std::string(key::ClientId), clientId},
            {std::string(key::Username), username}};

        if (!clients.empty())
        {
            j[std::string(msg::key::Clients)] = clients; // array of peerId strings
        };

        return j;
    }*/
    //inline nlohmann::json makeAuthResponse(const std::string ok, const std::string& msg, const std::string& clientId, const std::string& username, const std::vector<std::string>& clients = {}, const std::string& gmPeerId = "")
    //{
    //    auto j = nlohmann::json{
    //        {std::string(key::Type), std::string(signaling::AuthResponse)},
    //        {std::string(key::AuthOk), ok},
    //        {std::string(key::AuthMsg), msg},
    //        {std::string(key::ClientId), clientId},
    //        {std::string(key::Username), username}};
    //    if (!clients.empty())
    //    {
    //        j[std::string(msg::key::Clients)] = clients;
    //    }
    //    if (!gmPeerId.empty())
    //    {
    //        j[key::GmId] = gmPeerId; // NEW
    //    }
    //    return j;
    //}
    inline Json makeText(const std::string& from, const std::string& to, const std::string& text, bool broadcast = false)
    {
        return Json{
            {std::string(key::Type), std::string(signaling::Text)},
            {std::string(key::From), from},
            {std::string(key::To), to},
            {std::string(key::Broadcast), broadcast},
            {std::string(key::Text), text}};
    }

    // ---- predicates ----
    inline bool isType(const Json& j, std::string t)
    {
        auto it = j.find(std::string(key::Type));
        return it != j.end() && it->is_string() && it->get_ref<const std::string&>() == t;
    }

    // ---- accessors (safe-ish) ----
    inline std::string getString(const Json& j, std::string k, std::string def = {})
    {
        auto it = j.find(std::string(k));
        return (it != j.end() && it->is_string()) ? it->get<std::string>() : std::move(def);
    }
    inline int getInt(const Json& j, std::string k, int def = 0)
    {
        auto it = j.find(std::string(k));
        return (it != j.end() && it->is_number_integer()) ? it->get<int>() : def;
    }
    inline bool getBool(const Json& j, std::string k, bool def = false)
    {
        auto it = j.find(std::string(k));
        return (it != j.end() && it->is_boolean()) ? it->get<bool>() : def;
    }

} // namespace msg

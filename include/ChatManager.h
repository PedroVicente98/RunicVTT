#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <deque>
#include <set>
#include <unordered_map>
#include <filesystem>
#include <optional>
#include <memory>
#include <array>
#include <cmath>
#include "imgui.h"
#include "IdentityManager.h"
#include "Message.h"

class NetworkManager; // fwd
class PeerLink;       // fwd

// ---- Chat model ----
struct ChatMessageModel
{
    enum class Kind
    {
        TEXT,
        IMAGE,
        LINK
    };
    Kind kind = Kind::TEXT;
    std::string senderUniqueId; // <— NEW: stable sender id
    std::string username;       // display label
    std::string content;        // text / url
    double ts = 0.0;
};

struct ChatGroupModel
{
    uint64_t id = 0;                       // 1 reserved for General
    std::string name;                      // unique per table
    std::set<std::string> participants;    // peer ids (can duplicate across groups)
    std::string ownerUniqueId;             // who can delete/rename
    std::deque<ChatMessageModel> messages; // local history
    uint32_t unread = 0;                   // UI badge
};

// Pure UI+logic+wire sync manager (groups-based)
class ChatManager : public std::enable_shared_from_this<ChatManager>
{
public:
    explicit ChatManager(std::weak_ptr<NetworkManager> nm, std::shared_ptr<IdentityManager> identity_manager);

    // Attach/replace network
    void setNetwork(std::weak_ptr<NetworkManager> nm);

    // Change active GameTable context
    void setActiveGameTable(uint64_t tableId, const std::string& gameTableName);
    bool hasCurrent() const
    {
        return currentTableId_ != 0;
    }

    // Persistence (local file per table)
    bool saveCurrent();
    bool loadCurrent();

    // Incoming (GameTableManager -> processReceivedMessages)
    void applyReady(const msg::ReadyMessage& m); // handles group create/update/delete & message
    void pushMessageLocal(uint64_t groupId,
                          const std::string& fromPeer,
                          const std::string& username,
                          const std::string& text,
                          double ts,
                          bool incoming);

    // UI render
    void render();

    // Snapshot helpers (unchanged file layout can be separate if you already use them)
    void writeGroupsToSnapshotGT(std::vector<unsigned char>& buf) const;
    void readGroupsFromSnapshotGT(const std::vector<unsigned char>& buf, size_t& off);

    //// Replace a username everywhere (rename cascade from Identity flow)
    //void replaceUsernameEverywhere(const std::string& oldUsername,
    //                               const std::string& newUsername);

    // Exposed current table data
    static constexpr uint64_t generalGroupId_ = 1;
    uint64_t currentTableId_ = 0;
    std::string currentTableName_;

    // Utility
    ChatGroupModel* getGroup(uint64_t id);

    void replaceUsernameForUnique(const std::string& uniqueId, const std::string& newUsername);
    void tryHandleSlashCommand(uint64_t threadId, const std::string& input);
    std::set<std::string> resolvePeerIdsForParticipants(const std::set<std::string>& participantUids) const;

private:
    std::shared_ptr<IdentityManager> identity_manager;

    // === runtime store ===
    std::unordered_map<uint64_t, ChatGroupModel> groups_;
    uint64_t activeGroupId_ = generalGroupId_;

    // UI state
    std::array<char, 512> input_{};
    bool focusInput_ = false;
    bool followScroll_ = true;
    bool chatWindowFocused_ = false;

    // Popups
    bool openCreatePopup_ = false;
    bool openDeletePopup_ = false;
    bool openDicePopup_ = false;

    // Create popup state
    std::array<char, 128> newGroupName_{};
    std::set<std::string> newGroupSel_; // peer ids selected

    // --- Edit popup state ---
    bool openEditPopup_ = false;
    uint64_t editGroupId_ = 0;
    std::array<char, 128> editGroupName_{};
    std::set<std::string> editGroupSel_; // peer ids selected for edit

    // Dice popup state
    int diceN_ = 1;
    int diceSides_ = 20;
    int diceMod_ = 0;
    bool diceModPerDie_ = false;

    // wiring
    std::weak_ptr<NetworkManager> network_;

    // === helpers ===
    static ChatMessageModel::Kind classifyMessage(const std::string& s);
    static double nowSec();

    mutable std::unordered_map<std::string, ImU32> nameColorCache_;
    static ImVec4 HSVtoRGB(float h, float s, float v);
    ImU32 getUsernameColor(const std::string& name) const;
    void renderColoredUsername(const std::string& name) const;
    void renderPlainMessage(const std::string& text) const;

    std::filesystem::path chatFilePathFor(uint64_t tableId, const std::string& name) const;
    void ensureGeneral();
    void markGroupRead(uint64_t groupId);

    // ---- emit frames ----
    void emitGroupCreate(const ChatGroupModel& g);
    void emitGroupUpdate(const ChatGroupModel& g);
    void emitGroupDelete(uint64_t groupId);
    void emitChatMessageFrame(uint64_t groupId, const std::string& username, const std::string& text, uint64_t ts);

    // ---- UI ----

    void renderEditGroupPopup();
    void renderLeftPanel(float width);
    void renderRightPanel(float leftPanelWidth);
    void renderCreateGroupPopup();
    void renderDeleteGroupPopup();
    void renderDicePopup();

    // ---- storage ----
    bool saveLog(std::vector<uint8_t>& buf) const;
    bool loadLog(const std::vector<uint8_t>& buf);

    // ---- id utils ----
    uint64_t makeGroupIdFromName(const std::string& name) const; // name is unique; use its hash

    //STATIC METHODS
    // HSV->RGB helper for ImGui colors
};

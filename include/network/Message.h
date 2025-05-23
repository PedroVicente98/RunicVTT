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

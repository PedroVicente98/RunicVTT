#pragma once

#include <vector>
#include <glm/glm.hpp>

struct MessageHeader {
    uint32_t message_type;  // Type of message (e.g., chat, position, image)
    uint32_t payload_size;  // Size of the message payload

    template <class Archive>
    void serialize(Archive& ar) {
        ar(message_type, payload_size);
    }
};

struct Message {
    MessageHeader header;
    std::vector<unsigned char> payload; // Actual message data

    template <class Archive>
    void serialize(Archive& ar) {
        ar(header, payload);
    }
};
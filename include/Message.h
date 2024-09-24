#pragma once
#include <vector>
#include <cstdint>  // For uint32_t

// Message header structure
struct MessageHeader {
    uint32_t message_type;  // Type of message (e.g., chat, position, image)
    uint32_t payload_size;  // Size of the message payload
};

// Full message structure
struct Message {
    MessageHeader header;
    std::vector<unsigned char> payload;  // Actual message data

    // Constructor to initialize the message with type and payload
    Message(uint32_t type, const std::vector<unsigned char>& data)
        : header{ type, static_cast<uint32_t>(data.size()) }, payload(data) {}
};

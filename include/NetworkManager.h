#pragma once

#include "NetworkManager.h"

#include <asio.hpp>
#include <thread>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <deque>
//#include "Message.h"
#include <thread>

enum class MessageType {
    MarkerUpdate,
    CreateMarker,
    FogUpdate,
    CreateFog,
    ImageChunk,
    ChatMessage,
    GamestateStart,
    GamestateEnd
};

struct Message {
    MessageType type;
    unsigned char payload;
};


class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    // Server control methods
    void startServer(unsigned short port);   // Start the server (server start)
    void stopServer();                       // Stop the server (server stop)
    bool isConnectionOpen() const;           // Check if server is running (status check)

    // Peer handling methods
    void acceptConnections();  // Accept new connections (server main loop)
    void handlePeerConnected(std::shared_ptr<asio::ip::tcp::socket> socket);  // Process peer connection (peer management)
    void disconnectPeer(const std::string& peer_id);  // Disconnect a peer (disconnect management)

    // Message processing methods (sending)
    void processSentMessages();   // Process outgoing messages, with real-time and non-real-time prioritization
    void sendMessage(const std::string& peer_id, const Message& message);  // Send a message to a specific peer
    void broadcastMessage(const Message& message);  // Send a message to all connected peers

    // Message processing methods (receiving)
    void startReceiving(std::shared_ptr<asio::ip::tcp::socket> socket);  // Start receiving messages from a peer (initialize receive)
    void handleReceivedMessage(std::shared_ptr<asio::ip::tcp::socket> socket, const unsigned char* data, std::size_t length);  // Handle incoming messages

    // Utility methods
    std::string getNetworkInfo();     // Get network info (IP and port) (server utility)
    std::string getLocalIPAddress();  // Get local IP address (server utility)

private:
    asio::io_context io_context_;  // ASIO context for handling async operations
    asio::ip::tcp::acceptor acceptor_;  // ASIO acceptor for incoming connections
    std::unordered_map<std::string, std::shared_ptr<asio::ip::tcp::socket>> connectedPeers;  // Connected peers map
    
    char network_password[124] = "";
    std::queue<Message> realTimeQueue;
    std::queue<Message> nonRealTimeQueue;
    std::mutex queueMutex;

};

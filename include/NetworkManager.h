#pragma once

#include "NetworkManager.h"

#include <asio.hpp>
#include <thread>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <queue>
//#include "Message.h"
#include <thread>
#include <regex>
#include "Serializer.h"
#include "unordered_set"
#include <fstream> 

enum class MessageType {
    MarkerUpdate,
    CreateMarker,
    FogUpdate,
    CreateFog,
    ImageChunk,
    ChatMessage,
    CreateBoard,
    GamestateStart,
    GamestateEnd
};

struct Message {
    MessageType type;
    std::vector<unsigned char> payload;  // Use std::vector to manage dynamic byte arrays
};

struct ReceivedMessage {
    MessageType type;
    std::string user;            // Chat username (for example)
    std::string chatMessage;     // Chat message (if applicable)
    flecs::entity message_entity; //entity of the message(Marker, FogOfWar, Board or GameTable)
};


/*
mover a connection para o gametable. iniciar o gametable iniciar a conection
com a rede iniciada, tentar conectar com outro peer no computador 
quando connectar começar a fazer o envio do gamestate.
quando terminar o envio fazer o recebimento
depois do recebimento fazer o envio de atualizações
depois fazer o recebimento de atualizações

em algum momento depois de conectar o peer, fazer o peer discovery

*/

enum class Role {
    NONE,
    GAMEMASTER,
    PLAYER
};


class NetworkManager {
public:
    NetworkManager(flecs::world ecs);
    ~NetworkManager();

    // Server control methods
    void startServer(unsigned short port);   // Start the server (server start)
    void stopServer();                       // Stop the server (server stop)
    bool isConnectionOpen() const;           // Check if server is running (status check)

    unsigned short getPort() const;
    bool connectToPeer(const std::string& connection_string);
    Message buildCreateMarkerMessage(flecs::entity marker_entity);
    Message buildUpdateMarkerMessage(flecs::entity marker_entity);
    Message buildCreateBoardMessage(flecs::entity board_entity);
    Message buildChatMessage(std::string user, std::string message);
    std::string getUsername();
    void sendPassword(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& password);

    // Peer handling methods
    void startReceiving(std::shared_ptr<asio::ip::tcp::socket> socket);  // Start receiving messages from a peer (initialize receive)

    std::vector<unsigned char> serializeMessage(const Message& message);
    Message deserializeMessage(const std::vector<unsigned char>& buffer);


    void acceptConnections();  // Accept new connections (server main loop)
    void handlePeerConnected(std::shared_ptr<asio::ip::tcp::socket> socket);  // Process peer connection (peer management)

    void disconnectPeer(const std::string& peer_ip);  // Disconnect a peer (disconnect management)

    void handleMessage(std::shared_ptr<asio::ip::tcp::socket> socket, const std::vector<unsigned char>& buffer, std::size_t length);

    void handleImageChunk(const std::string& imagePath, int chunkNumber, int totalChunks, const std::vector<unsigned char>& chunkData);

    void finalizeImage(const std::string& imagePath);

    void queueMessage(const Message& message);

    void startSending();
    void processSentMessages();   // Process outgoing messages, with real-time and non-real-time prioritization

    void sendMessageToPeers(const Message& message);

    void verifyPassword(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& receivedPassword);
    void sendDisconnectMessage(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& reason);




    // Utility methods
    std::string getNetworkInfo();     // Get network info (IP and port) (server utility)
    std::string getLocalIPAddress();  // Get local IP address (server utility)

    void setNetworkPassword(const char* password) {
        strncpy(network_password, password, sizeof(network_password) - 1);
        network_password[sizeof(network_password) - 1] = '\0';
    }

    Role getPeerRole() const { return peer_role; }
    // Caches for image data being received
    std::unordered_map<std::string, std::unordered_set<int>> imageChunksReceived;  // Tracks which chunks have been received for each image
    std::unordered_map<std::string, std::vector<unsigned char>> imageBufferMap;  // Stores the chunks for each image as they are received


    std::mutex receivedQueueMutex;
    std::queue<ReceivedMessage> receivedQueue;

private:
    asio::io_context io_context_;  // ASIO context for handling async operations
    asio::ip::tcp::acceptor acceptor_;  // ASIO acceptor for incoming connections
    std::unordered_map<std::string, std::shared_ptr<asio::ip::tcp::socket>> connectedPeers;  // Connected peers map
    flecs::world ecs;
    char network_password[124] = "\0";
    std::queue<Message> realTimeQueue;
    std::queue<Message> nonRealTimeQueue;
    std::mutex queueMutex;

    



    Role peer_role = Role::NONE;
};

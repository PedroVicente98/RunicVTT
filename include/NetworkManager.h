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
    NetworkManager();
    ~NetworkManager();

    // Server control methods
    void startServer(unsigned short port);   // Start the server (server start)
    void stopServer();                       // Stop the server (server stop)
    bool isConnectionOpen() const;           // Check if server is running (status check)

    unsigned short getPort() const;
    bool connectToPeer(const std::string& connection_string);
    void sendPassword(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& password);

    // Peer handling methods
    void startReceiving(std::shared_ptr<asio::ip::tcp::socket> socket);  // Start receiving messages from a peer (initialize receive)
    void acceptConnections();  // Accept new connections (server main loop)
    void handlePeerConnected(std::shared_ptr<asio::ip::tcp::socket> socket);  // Process peer connection (peer management)

    void disconnectPeer(const std::string& peer_id);  // Disconnect a peer (disconnect management)

    void handleMessage(std::shared_ptr<asio::ip::tcp::socket> socket, const unsigned char* data, std::size_t length);

    void queueMessage(const Message& message);

    // Message processing methods (sending)
    void processSentMessages();   // Process outgoing messages, with real-time and non-real-time prioritization

    void sendMessageToPeers(const Message& message);

    void sendMessage(const std::string& peer_id, const Message& message);  // Send a message to a specific peer

    void broadcastMessage(const Message& message);  // Send a message to all connected peers

    // Message processing methods (receiving)
    void verifyPassword(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& receivedPassword);
    void sendDisconnectMessage(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& reason);


    void handleReceivedMessage(std::shared_ptr<asio::ip::tcp::socket> socket, const unsigned char* data, std::size_t length);  // Handle incoming messages

    // Utility methods
    std::string getNetworkInfo();     // Get network info (IP and port) (server utility)
    std::string getLocalIPAddress();  // Get local IP address (server utility)


    void setNetworkPassword(const char* password) {
        strncpy(network_password, password, sizeof(network_password) - 1);
        network_password[sizeof(network_password) - 1] = '\0';
    }

    Role getPeerRole() const { return peer_role; }

private:
    asio::io_context io_context_;  // ASIO context for handling async operations
    asio::ip::tcp::acceptor acceptor_;  // ASIO acceptor for incoming connections
    std::unordered_map<std::string, std::shared_ptr<asio::ip::tcp::socket>> connectedPeers;  // Connected peers map
    
    char network_password[124] = "\0";
    std::queue<Message> realTimeQueue;
    std::queue<Message> nonRealTimeQueue;
    std::mutex queueMutex;
    Role peer_role = Role::NONE;
};

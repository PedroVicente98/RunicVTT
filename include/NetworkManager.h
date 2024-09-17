#pragma once

#include "NetworkManager.h"

#include <asio.hpp>
#include <thread>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include "Message.h"

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    void startServer(unsigned short port);
    void connectToPeer(const std::string& ip, unsigned short port);
    void sendMessage(const std::string& peer_id, const Message& message);
    void broadcastMessage(const Message& message);

    void onMessageReceived(const std::function<void(const std::string&, const Message&)>& handler);
    void onPeerConnected(const std::function<void(const std::string&)>& handler);
    void onPeerDisconnected(const std::function<void(const std::string&)>& handler);

private:
    void acceptConnections();
    void startReceiving(std::shared_ptr<asio::ip::tcp::socket> socket);

    asio::io_context io_context_;
    asio::ip::tcp::acceptor acceptor_;
    std::unordered_map<std::string, std::shared_ptr<asio::ip::tcp::socket>> peers_;
    std::function<void(const std::string&, const Message&)> message_handler_;
    std::function<void(const std::string&)> peer_connected_handler_;
    std::function<void(const std::string&)> peer_disconnected_handler_;
};

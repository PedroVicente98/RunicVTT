#pragma once

#include "NetworkManager.h"

#include <asio.hpp>
#include <thread>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <deque>
#include "Message.h"
#include <thread>

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();
    

   void startServer(unsigned short port);
   void stopServer();
   std::string getNetworkInfo();
   std::string getLocalIPAddress();

   bool isConnectionOpen() const;
   void acceptConnections();
   void handlePeerConnected(std::shared_ptr<asio::ip::tcp::socket> socket);
   void startReceiving(std::shared_ptr<asio::ip::tcp::socket> socket);
   void handleMessage(std::shared_ptr<asio::ip::tcp::socket> socket, const unsigned char* data, std::size_t length);

   /* void startServer(unsigned short port);
    void connectToPeer(const std::string& ip, unsigned short port);
    void sendMessage(const std::string& peer_id, const Message& message);
    void broadcastMessage(const Message& message);

    void onMessageReceived(const std::function<void(const std::string&, const Message&)>& handler);
    void onPeerConnected(const std::function<void(const std::string&)>& handler);
    void onPeerDisconnected(const std::function<void(const std::string&)>& handler);*/

private:
    //void acceptConnections();
    //void startReceiving(std::shared_ptr<asio::ip::tcp::socket> socket);

    asio::io_context io_context_;
    asio::ip::tcp::acceptor acceptor_;
    std::unordered_map<std::string, std::shared_ptr<asio::ip::tcp::socket>> peers_;
    char network_password[124] = "";

    std::deque<Message> realTimeQueue;
    std::deque<Message> nonRealTimeQueue;
    std::mutex queueMutex;

};

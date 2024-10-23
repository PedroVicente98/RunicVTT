#include "NetworkManager.h"
#include <iostream>
#include <sstream>
#include "Message.h"
#include <asio.hpp>

NetworkManager::NetworkManager()
    : acceptor_(io_context_) {}

NetworkManager::~NetworkManager() {
    io_context_.stop();
    if (acceptor_.is_open()) {
        acceptor_.close();
    }
}

// Stop the server and close all connections
void NetworkManager::stopServer() {
    io_context_.stop();  // Stop the ASIO context
    if (acceptor_.is_open()) {
        acceptor_.close();  // Close the acceptor to stop accepting connections
    }
}

bool NetworkManager::isConnectionOpen() const {
    return acceptor_.is_open();  // Return true if the acceptor is open
}


std::string NetworkManager::getNetworkInfo() {
    try {
        // Resolve the local IP address using the hostname
        asio::ip::tcp::resolver resolver(io_context_);
        asio::ip::tcp::resolver::query query(asio::ip::host_name(), "");
        asio::ip::tcp::resolver::iterator it = resolver.resolve(query);
        asio::ip::tcp::resolver::iterator end;

        // Loop through the results to find the first IPv4 address
        for (; it != end; ++it) {
            asio::ip::tcp::endpoint ep = *it;
            if (ep.address().is_v4() && !ep.address().is_loopback()) {
                // Ensure the acceptor is bound and return the IP and port
                return ep.address().to_string() + ":" + std::to_string(acceptor_.local_endpoint().port());
            }
        }

        return "No valid local IPv4 address found";
    } catch (std::exception& e) {
        return "Could not retrieve IP address or port: " + std::string(e.what());
    }
}

std::string NetworkManager::getLocalIPAddress() {
    try {
        asio::ip::tcp::resolver resolver(io_context_);
        asio::ip::tcp::resolver::query query(asio::ip::host_name(), "");
        asio::ip::tcp::resolver::iterator it = resolver.resolve(query);
        asio::ip::tcp::resolver::iterator end;

        // Loop through the results to find the first valid IPv4 address
        for (; it != end; ++it) {
            asio::ip::tcp::endpoint ep = *it;
            if (ep.address().is_v4() && !ep.address().is_loopback()) {
                return ep.address().to_string();  // Return the valid IPv4 address
            }
        }

        return "No valid local IPv4 address found";
    } catch (std::exception& e) {
        return "Unable to retrieve IP: " + std::string(e.what());
    }
}


/*
std::string NetworkManager::getNetworkInfo() {
    try {
        // Get local IP address
        asio::ip::tcp::resolver resolver(io_context_);
        asio::ip::tcp::resolver::query query(asio::ip::host_name(), "");
        asio::ip::tcp::resolver::iterator endpoints = resolver.resolve(query);
        asio::ip::tcp::endpoint ep = *endpoints;

        // Return IP address and port
        return ep.address().to_string() + ":" + std::to_string(acceptor_.local_endpoint().port());
    }
    catch (std::exception& e) {
        return "Could not retrieve IP address";
    }
}

std::string NetworkManager::getLocalIPAddress() {
    try {
        asio::ip::tcp::resolver resolver(io_context_);
        asio::ip::tcp::resolver::query query(asio::ip::host_name(), "");
        asio::ip::tcp::resolver::iterator it = resolver.resolve(query);
        asio::ip::tcp::endpoint ep = *it;
        return ep.address().to_string();  // Return the local IP address
    }
    catch (std::exception& e) {
        return "Unable to retrieve IP";
    }
}

*/
// Start the server with the given port
void NetworkManager::startServer(unsigned short port) {
    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();
    acceptConnections();  // Start accepting connections

    std::thread([this]() { io_context_.run(); }).detach();  // Run io_context in a separate thread
}

// Method to accept incoming connections
void NetworkManager::acceptConnections() {
    auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
    acceptor_.async_accept(*socket, [this, socket](asio::error_code ec) {
        if (!ec) {
            handlePeerConnected(socket);
            startReceiving(socket);  // Begin reading from the peer
        }
        acceptConnections();  // Continue accepting new connections
        });
}


// Handle when a peer connects
void  NetworkManager::handlePeerConnected(std::shared_ptr<asio::ip::tcp::socket> socket) {
    // Logic for when a peer connects, e.g., storing the socket or notifying the application
}

// Begin receiving messages from the connected peer
void  NetworkManager::startReceiving(std::shared_ptr<asio::ip::tcp::socket> socket) {
    auto buffer = std::make_shared<std::vector<unsigned char>>(1024);  // 1KB buffer size
    socket->async_read_some(asio::buffer(*buffer), [this, socket, buffer](asio::error_code ec, std::size_t length) {
        if (!ec) {
            // Process the received message (deserialize, handle data)
            handleMessage(socket, buffer->data(), length);
            startReceiving(socket);  // Continue reading from this peer
        }
        });
}

// Process received message
void  NetworkManager::handleMessage(std::shared_ptr<asio::ip::tcp::socket> socket, const unsigned char* data, std::size_t length) {
    // Deserialize and process the message here
}


void NetworkManager::queueMessage(const Message& message) {
    if (message.messageType == MessageType::REAL_TIME_UPDATE) {
        realTimeQueue.push(message);  // High-priority queue
    } else {
        nonRealTimeQueue.push(message);  // Low-priority queue
    }
}


void NetworkManager::processMessages() {
    // Process 5 real-time messages for every 1 non-real-time message
    int realTimeMessagesProcessed = 0;

    // Process all real-time messages first
    while (!realTimeQueue.empty() && realTimeMessagesProcessed < 5) {
        Message message = realTimeQueue.front();
        realTimeQueue.pop();
        handleMessage(message);
        realTimeMessagesProcessed++;
    }

    // After processing some real-time messages, process one non-real-time message
    if (!nonRealTimeQueue.empty()) {
        Message message = nonRealTimeQueue.front();
        nonRealTimeQueue.pop();
        handleMessage(message);
    }
}




























//
//void NetworkManager::acceptConnections() {
//    auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
//    acceptor_.async_accept(*socket, [this, socket](asio::error_code ec) {
//        if (!ec) {
//            std::string peer_id = socket->remote_endpoint().address().to_string() + ":" + std::to_string(socket->remote_endpoint().port());
//            peers_[peer_id] = socket;
//
//            if (peer_connected_handler_) {
//                peer_connected_handler_(peer_id);
//            }
//
//            startReceiving(socket);
//            acceptConnections();
//        }
//        else {
//            std::cerr << "Error accepting connection: " << ec.message() << std::endl;
//        }
//        });
//}
//
//void NetworkManager::connectToPeer(const std::string& ip, unsigned short port) {
//    auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
//    asio::ip::tcp::endpoint endpoint(asio::ip::make_address(ip), port);
//
//    socket->async_connect(endpoint, [this, socket, ip, port](asio::error_code ec) {
//        if (!ec) {
//            std::string peer_id = ip + ":" + std::to_string(port);
//            peers_[peer_id] = socket;
//
//            if (peer_connected_handler_) {
//                peer_connected_handler_(peer_id);
//            }
//
//            startReceiving(socket);
//        }
//        else {
//            std::cerr << "Error connecting to peer: " << ec.message() << std::endl;
//        }
//        });
//}
//
//void NetworkManager::sendMessage(const std::string& peer_id, const Message& message) {
//    if (peers_.find(peer_id) != peers_.end()) {
//        auto& socket = peers_[peer_id];
//        std::stringstream ss;
//        {
//            /*cereal::BinaryOutputArchive oarchive(ss);
//            oarchive(message);*/
//        }
//        std::string serialized_data = ss.str();
//        asio::async_write(*socket, asio::buffer(serialized_data), [](asio::error_code ec, std::size_t /*length*/) {
//            if (ec) {
//                std::cerr << "Failed to send message: " << ec.message() << std::endl;
//            }
//            });
//    }
//}
//
//void NetworkManager::broadcastMessage(const Message& message) {
//    for (auto& peer : peers_) {
//        sendMessage(peer.first, message);
//    }
//}
//
//void NetworkManager::onMessageReceived(const std::function<void(const std::string&, const Message&)>& handler) {
//    message_handler_ = handler;
//}
//
//void NetworkManager::onPeerConnected(const std::function<void(const std::string&)>& handler) {
//    peer_connected_handler_ = handler;
//}
//
//void NetworkManager::onPeerDisconnected(const std::function<void(const std::string&)>& handler) {
//    peer_disconnected_handler_ = handler;
//}
//
//void NetworkManager::startReceiving(std::shared_ptr<asio::ip::tcp::socket> socket) {
//    auto buffer = std::make_shared<std::array<char, 1024>>();
//    socket->async_read_some(asio::buffer(*buffer), [this, socket, buffer](asio::error_code ec, std::size_t length) {
//        if (!ec) {
//            std::stringstream ss(std::string(buffer->data(), length));
//            Message message;
//            {
//                /*cereal::BinaryInputArchive iarchive(ss);
//                iarchive(message);*/
//            }
//
//            std::string peer_id = socket->remote_endpoint().address().to_string() + ":" + std::to_string(socket->remote_endpoint().port());
//            if (message_handler_) {
//                message_handler_(peer_id, message);
//            }
//
//            startReceiving(socket);
//        }
//        else {
//            std::cerr << "Error receiving data: " << ec.message() << std::endl;
//            std::string peer_id = socket->remote_endpoint().address().to_string() + ":" + std::to_string(socket->remote_endpoint().port());
//            peers_.erase(peer_id);
//            if (peer_disconnected_handler_) {
//                peer_disconnected_handler_(peer_id);
//            }
//        }
//        });
//}

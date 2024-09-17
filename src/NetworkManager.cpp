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

void NetworkManager::startServer(unsigned short port) {
    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();

    acceptConnections();

    std::thread([this]() { io_context_.run(); }).detach();
}

void NetworkManager::acceptConnections() {
    auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
    acceptor_.async_accept(*socket, [this, socket](asio::error_code ec) {
        if (!ec) {
            std::string peer_id = socket->remote_endpoint().address().to_string() + ":" + std::to_string(socket->remote_endpoint().port());
            peers_[peer_id] = socket;

            if (peer_connected_handler_) {
                peer_connected_handler_(peer_id);
            }

            startReceiving(socket);
            acceptConnections();
        }
        else {
            std::cerr << "Error accepting connection: " << ec.message() << std::endl;
        }
        });
}

void NetworkManager::connectToPeer(const std::string& ip, unsigned short port) {
    auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address(ip), port);

    socket->async_connect(endpoint, [this, socket, ip, port](asio::error_code ec) {
        if (!ec) {
            std::string peer_id = ip + ":" + std::to_string(port);
            peers_[peer_id] = socket;

            if (peer_connected_handler_) {
                peer_connected_handler_(peer_id);
            }

            startReceiving(socket);
        }
        else {
            std::cerr << "Error connecting to peer: " << ec.message() << std::endl;
        }
        });
}

void NetworkManager::sendMessage(const std::string& peer_id, const Message& message) {
    if (peers_.find(peer_id) != peers_.end()) {
        auto& socket = peers_[peer_id];
        std::stringstream ss;
        {
            /*cereal::BinaryOutputArchive oarchive(ss);
            oarchive(message);*/
        }
        std::string serialized_data = ss.str();
        asio::async_write(*socket, asio::buffer(serialized_data), [](asio::error_code ec, std::size_t /*length*/) {
            if (ec) {
                std::cerr << "Failed to send message: " << ec.message() << std::endl;
            }
            });
    }
}

void NetworkManager::broadcastMessage(const Message& message) {
    for (auto& peer : peers_) {
        sendMessage(peer.first, message);
    }
}

void NetworkManager::onMessageReceived(const std::function<void(const std::string&, const Message&)>& handler) {
    message_handler_ = handler;
}

void NetworkManager::onPeerConnected(const std::function<void(const std::string&)>& handler) {
    peer_connected_handler_ = handler;
}

void NetworkManager::onPeerDisconnected(const std::function<void(const std::string&)>& handler) {
    peer_disconnected_handler_ = handler;
}

void NetworkManager::startReceiving(std::shared_ptr<asio::ip::tcp::socket> socket) {
    auto buffer = std::make_shared<std::array<char, 1024>>();
    socket->async_read_some(asio::buffer(*buffer), [this, socket, buffer](asio::error_code ec, std::size_t length) {
        if (!ec) {
            std::stringstream ss(std::string(buffer->data(), length));
            Message message;
            {
                /*cereal::BinaryInputArchive iarchive(ss);
                iarchive(message);*/
            }

            std::string peer_id = socket->remote_endpoint().address().to_string() + ":" + std::to_string(socket->remote_endpoint().port());
            if (message_handler_) {
                message_handler_(peer_id, message);
            }

            startReceiving(socket);
        }
        else {
            std::cerr << "Error receiving data: " << ec.message() << std::endl;
            std::string peer_id = socket->remote_endpoint().address().to_string() + ":" + std::to_string(socket->remote_endpoint().port());
            peers_.erase(peer_id);
            if (peer_disconnected_handler_) {
                peer_disconnected_handler_(peer_id);
            }
        }
        });
}

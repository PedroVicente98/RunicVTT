#include "NetworkManager.h"
#include <iostream>
#include <sstream>
#include "Message.h"
#include <asio.hpp>

// Start/Stop ----------------------------------------------------------------------------
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
    peer_role = Role::GAMEMASTER;
    acceptConnections();  // Start accepting connections

    std::thread([this]() { io_context_.run(); }).detach();  // Run io_context in a separate thread
}

void NetworkManager::stopServer() {
    io_context_.stop();  // Stop the ASIO context
    if (acceptor_.is_open()) {
        acceptor_.close();  // Close the acceptor to stop accepting connections
    }
}

// Start/Stop END ---------------------------------------------------------------------- END


// Auxiliar Operations -----------------------------------------------------------------

bool NetworkManager::isConnectionOpen() const {
    return acceptor_.is_open();  // Return true if the acceptor is open
}

unsigned short NetworkManager::getPort() const {
    if (acceptor_.is_open()) {
        return acceptor_.local_endpoint().port();  // Return the actual port number
    }
    else {
        return 0;  // Return 0 if the connection is not open
    }
}

std::string NetworkManager::getNetworkInfo() {
    auto ip_address = getLocalIPAddress();
    auto port = getPort();
    std::string network_info = "runic:" + ip_address + ":" + std::to_string(port) + "?" + network_password;
    return network_info;
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
// Auxiliar Operations END ---------------------------------------------------------------------- END


//Message Operations ------------------------------------------------------------------------------

void  NetworkManager::handleMessage(std::shared_ptr<asio::ip::tcp::socket> socket, const unsigned char* data, std::size_t length) {
    // Deserialize and process the message here
}

void NetworkManager::startReceiving(std::shared_ptr<asio::ip::tcp::socket> socket) {
    auto buffer = std::make_shared<std::vector<unsigned char>>(1024);  // 1KB buffer size
    socket->async_read_some(asio::buffer(*buffer), [this, socket, buffer](asio::error_code ec, std::size_t length) {
        if (!ec) {
            std::string data(buffer->begin(), buffer->begin() + length);
            if (data.rfind("PASSWORD:", 0) == 0) {  // Check if the message starts with "PASSWORD:"
                std::string receivedPassword = data.substr(9);  // Extract the password part
                verifyPassword(socket, receivedPassword);
            } else {
                handleMessage(socket, buffer->data(), length);  // Handle other messages
            }

            startReceiving(socket);  // Continue reading from this peer
        } else {
            std::cout << "Error in receiving: " << ec.message() << std::endl;
        }
    });
}
//Message Operations END ------------------------------------------------------------------------------------- END


//Connect to Peer Operations ---------------------------------------------------------------------------------
bool NetworkManager::connectToPeer(const std::string& connection_string) {
    std::regex rgx(R"(runic:([\d.]+):(\d+)\??(.*))");
    std::smatch match;

    // Parse the connection string using regex
    if (std::regex_match(connection_string, match, rgx)) {
        std::string ip = match[1];             // Extract the IP address
        unsigned short port = std::stoi(match[2]);  // Extract the port
        std::string password = match[3];       // Extract the optional password
        
        // Establish a connection using ASIO
        try {
            asio::ip::tcp::resolver resolver(io_context_);
            auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
            asio::ip::tcp::endpoint endpoint(asio::ip::make_address(ip), port);

            //asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(ip, std::to_string(port));
            //asio::connect(*socket, endpoints);

            socket->async_connect(endpoint, [this, ip, port, socket, password](asio::error_code ec) {
                if (!ec) {
                    std::cout << "Successfully connected to server at " << ip << ":" << port << std::endl;
                
                    if (socket->is_open()) {
                        std::cout << "Connection established. Socket is open." << std::endl;
                        std::cout << "Local IP: " << socket->local_endpoint().address().to_string()
                                    << " | Local Port: " << socket->local_endpoint().port() << std::endl;
                        std::cout << "Connected to: " << socket->remote_endpoint().address().to_string()
                                    << " | Remote Port: " << socket->remote_endpoint().port() << std::endl;
                    }
                    // Send the password to the server
                    sendPassword(socket, password);
                    startReceiving(socket);  // Start receiving messages from the server

                } else {
                    std::cout << "Connection failed: " << ec.message() << std::endl;
                }
            });

            //// You can now start communication with the peer
            //std::cout << "Connected to peer: " << ip << ":" << port << std::endl;
            //startReceiving(socket);  // Start receiving messages from this peer

            return true;
        }
        catch (std::exception& e) {
            std::cerr << "Failed to connect: " << e.what() << std::endl;
            return false;
        }

    }
    else {
        std::cerr << "Invalid connection string format!" << std::endl;
        return false;
    }
}

// Handle when a peer connects
void  NetworkManager::handlePeerConnected(std::shared_ptr<asio::ip::tcp::socket> socket) {
    std::cout << "Client connected from: "
        << socket->remote_endpoint().address().to_string()
        << ":" << socket->remote_endpoint().port() << std::endl;
    connectedPeers.emplace(socket->remote_endpoint().address().to_string(), socket);
    // Begin receiving messages from this peer
    startReceiving(socket);
}

// Method to accept incoming connections
void NetworkManager::acceptConnections() {
    auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
    acceptor_.async_accept(*socket, [this, socket](asio::error_code ec) {
        if (!ec) {
            handlePeerConnected(socket);
        }
        acceptConnections();  // Continue accepting new connections
        });
}

//Connect to Peer Operations END------------------------------------------------------------------------------------- END


//Password Operations ------------------------------------------------------------------------------------------------
void NetworkManager::sendPassword(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& password) {
    std::string message = "PASSWORD:" + password;
    asio::async_write(*socket, asio::buffer(message), [this, socket](asio::error_code ec, std::size_t length) {
        if (!ec) {
            std::cout << "Password sent to server.\n";
        } else {
            std::cout << "Failed to send password: " << ec.message() << std::endl;
        }
    });
}
void NetworkManager::verifyPassword(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& receivedPassword) {
    if (!strlen(network_password) == 0 && receivedPassword != network_password) {
        std::cout << "Invalid password from peer. Disconnecting.\n";
        sendDisconnectMessage(socket, "Invalid password");
    } else {
        std::cout << "Password verified. Peer is authenticated.\n";
        handlePeerConnected(socket);  // Handle further connection logic like sending game state
    }
}
void NetworkManager::sendDisconnectMessage(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& reason) {
    std::string message = "DISCONNECT:" + reason;
    asio::async_write(*socket, asio::buffer(message), [this, reason, socket](asio::error_code ec, std::size_t length) {
        if (!ec) {
            std::cout << "Disconnection message sent: " << reason << std::endl;
            socket->close();  // Close the connection after sending the disconnection message
        } else {
            std::cout << "Failed to send disconnection message: " << ec.message() << std::endl;
        }
    });
}
//Password Operations END -------------------------------------------------------------------------------------------------- END



////Network Connection Logic for Password
//
////Client-Side Implementation: Connecting to a Peer
//void NetworkManager::connectToPeer(const std::string& ip, unsigned short port, const std::string& password) {
//    auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
//    asio::ip::tcp::endpoint endpoint(asio::ip::make_address(ip), port);
//
//    socket->async_connect(endpoint, [this, ip, port, socket, password](asio::error_code ec) {
//        if (!ec) {
//            std::cout << "Successfully connected to server at " << ip << ":" << port << std::endl;
//
//            if (socket->is_open()) {
//                std::cout << "Connection established. Socket is open." << std::endl;
//                std::cout << "Local IP: " << socket->local_endpoint().address().to_string()
//                          << " | Local Port: " << socket->local_endpoint().port() << std::endl;
//                std::cout << "Connected to: " << socket->remote_endpoint().address().to_string()
//                          << " | Remote Port: " << socket->remote_endpoint().port() << std::endl;
//            }
//
//            // Send the password to the server
//            sendPassword(socket, password);
//            startReceiving(socket);  // Start receiving messages from the server
//        } else {
//            std::cout << "Connection failed: " << ec.message() << std::endl;
//        }
//    });
//}
//

//
////Server-Side: Accepting Connections
//void NetworkManager::acceptConnections() {
//    auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
//    acceptor_.async_accept(*socket, [this, socket](asio::error_code ec) {
//        if (!ec) {
//            std::cout << "New peer connected.\n";
//            startReceiving(socket);  // Begin receiving the password
//        }
//        acceptConnections();  // Continue accepting new connections
//    });
//}
//
////Server-Side: Start Receiving Messages
//void NetworkManager::startReceiving(std::shared_ptr<asio::ip::tcp::socket> socket) {
//    auto buffer = std::make_shared<std::vector<unsigned char>>(1024);  // 1KB buffer size
//    socket->async_read_some(asio::buffer(*buffer), [this, socket, buffer](asio::error_code ec, std::size_t length) {
//        if (!ec) {
//            std::string data(buffer->begin(), buffer->begin() + length);
//            if (data.rfind("PASSWORD:", 0) == 0) {  // Check if the message starts with "PASSWORD:"
//                std::string receivedPassword = data.substr(9);  // Extract the password part
//                verifyPassword(socket, receivedPassword);
//            } else {
//                handleMessage(socket, buffer->data(), length);  // Handle other messages
//            }
//            startReceiving(socket);  // Continue reading from this peer
//        } else {
//            std::cout << "Error in receiving: " << ec.message() << std::endl;
//        }
//    });
//}
//
////Server-Side: Password Verification
//void NetworkManager::verifyPassword(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& receivedPassword) {
//    if (receivedPassword != network_password) {
//        std::cout << "Invalid password from peer. Disconnecting.\n";
//        sendDisconnectMessage(socket, "Invalid password");
//    } else {
//        std::cout << "Password verified. Peer is authenticated.\n";
//        handlePeerConnected(socket);  // Handle further connection logic like sending game state
//    }
//}
//
////Server-Side: Sending a Disconnection Message
//void NetworkManager::sendDisconnectMessage(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& reason) {
//    std::string message = "DISCONNECT:" + reason;
//    asio::async_write(*socket, asio::buffer(message), [this, reason, socket](asio::error_code ec, std::size_t length) {
//        if (!ec) {
//            std::cout << "Disconnection message sent: " << reason << std::endl;
//            socket->close();  // Close the connection after sending the disconnection message
//        } else {
//            std::cout << "Failed to send disconnection message: " << ec.message() << std::endl;
//        }
//    });
//}
//
//
//void  NetworkManager::handleMessage(std::shared_ptr<asio::ip::tcp::socket> socket, const unsigned char* data, std::size_t length) {
//    // Deserialize and process the message here
//}
//
//
//void NetworkManager::queueMessage(const Message& message) {
//    if (message.type != MessageType::ImageChunk) {
//        realTimeQueue.push(message);  // High-priority queue
//    } else {
//        nonRealTimeQueue.push(message);  // Low-priority queue
//    }
//}
//
//
//void NetworkManager::processSentMessages() {
//    // Process 5 real-time messages for every 1 non-real-time message
//    int realTimeMessagesProcessed = 0;
//
//    // Process all real-time messages first
//    while (!realTimeQueue.empty() && realTimeMessagesProcessed < 5) {
//        Message message = realTimeQueue.front();
//        realTimeQueue.pop();
//        sendMessageToPeers(message);  // Function to send the message to peers
//        realTimeMessagesProcessed++;
//    }
//
//    // After processing some real-time messages, process one non-real-time message
//    if (!nonRealTimeQueue.empty()) {
//        Message message = nonRealTimeQueue.front();
//        nonRealTimeQueue.pop();
//        sendMessageToPeers(message);  // Function to send the message to peers
//        realTimeMessagesProcessed = 0;  // Reset the counter after sending the non-real-time message
//    }
//}
//
//
//void NetworkManager::sendMessageToPeers(const Message& message) {
//    for (auto& peer : connectedPeers) {  // Assume connectedPeers is a list of active connections
//        asio::async_write(peer.second, asio::buffer(std::string()), [](const asio::error_code& ec, std::size_t /*length*/) {
//            if (!ec) {
//                std::cout << "Message sent successfully" << std::endl;
//            } else {
//                std::cerr << "Error sending message: " << ec.message() << std::endl;
//            }
//        });
//    }
//}
//
//
//
//
//
//
//
//
//
//
//
//






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

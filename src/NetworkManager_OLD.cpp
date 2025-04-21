//#include "NetworkManager.h"
#include <iostream>
#include <sstream>
#include "Message.h"
#include <cstdlib>  // For system()

// Start/Stop ----------------------------------------------------------------------------
NetworkManager::NetworkManager(flecs::world ecs)
    : acceptor_(io_context_), ecs(ecs) {}

NetworkManager::~NetworkManager() {
    io_context_.stop();
    if (acceptor_.is_open()) {
        auto port = acceptor_.local_endpoint().port();
        disallowPort(port);
        acceptor_.close();
    }
}

void NetworkManager::startServer(unsigned short port, bool connected_peer) {

    auto ip_address = getLocalIPAddress();
    if (ip_address == "No valid local IPv4 address found" || ip_address.find("Unable to retrieve IP") == 0) {
        std::cerr << "Failed to start server: " << ip_address << std::endl;
        return;
    }
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address(ip_address), port);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();
    if (connected_peer) {
        peer_role = Role::PLAYER;
    }
    else {
        peer_role = Role::GAMEMASTER;
    }
    acceptConnections();  // Start accepting connections
    allowPort(port);
    std::thread([this]() { io_context_.run(); }).detach();  // Run io_context in a separate thread
}

void NetworkManager::stopServer() {
    io_context_.stop();  // Stop the ASIO context
    if (acceptor_.is_open()) {
        acceptor_.close();  // Close the acceptor to stop accepting connections
    }
    connectedPeers.clear();
}

// Start/Stop END ---------------------------------------------------------------------- END

// Auxiliar Operations -----------------------------------------------------------------



/*void NetworkManager::allowPort(int port) {
    std::string inboundCommand = "powershell.exe New-NetFirewallRule -DisplayName 'Allow TCP Inbound on Port " + std::to_string(port) + "' -Direction Inbound -Action Allow -Protocol TCP -LocalPort " + std::to_string(port);
    std::string outboundCommand = "powershell.exe New-NetFirewallRule -DisplayName 'Allow TCP Outbound on Port " + std::to_string(port) + "' -Direction Outbound -Action Allow -Protocol TCP -LocalPort " + std::to_string(port);

    system(inboundCommand.c_str());
    system(outboundCommand.c_str());
}*/

void NetworkManager::allowPort(int port) {
    std::string portStr = std::to_string(port);

    // Check if the rule already exists
    std::string checkCommand = 
        "powershell.exe -Command \"if (-not(Get-NetFirewallRule -DisplayName 'Allow TCP Inbound on Port " 
        + portStr + "' -ErrorAction SilentlyContinue)) { "
        "New-NetFirewallRule -DisplayName 'Allow TCP Inbound on Port " + portStr + "' "
        "-Direction Inbound -Action Allow -Protocol TCP -LocalPort " + portStr + " -Profile Any -RemoteAddress Any }\"";

    system(checkCommand.c_str());

    // Create outbound rule if it doesn’t exist
    checkCommand = 
        "powershell.exe -Command \"if (-not(Get-NetFirewallRule -DisplayName 'Allow TCP Outbound on Port " 
        + portStr + "' -ErrorAction SilentlyContinue)) { "
        "New-NetFirewallRule -DisplayName 'Allow TCP Outbound on Port " + portStr + "' "
        "-Direction Outbound -Action Allow -Protocol TCP -LocalPort " + portStr + " -Profile Any -RemoteAddress Any }\"";

    system(checkCommand.c_str());
}


void NetworkManager::disallowPort(unsigned short port) {
    std::string portStr = std::to_string(port);

    // Remove inbound rule for the specified port
    std::string inboundCommand = "powershell -Command \"Remove-NetFirewallRule -DisplayName 'Allow TCP Inbound on Port " + portStr + "'\"";

    // Remove outbound rule for the specified port
    std::string outboundCommand = "powershell -Command \"Remove-NetFirewallRule -DisplayName 'Allow TCP Outbound on Port " + portStr + "'\"";

    system(inboundCommand.c_str());
    system(outboundCommand.c_str());
}


bool NetworkManager::isConnectionOpen() const {
    return acceptor_.is_open();  // Return true if the acceptor is open
}

bool NetworkManager::isConnected() const {
    for (auto& peer : connectedPeers) {
        auto socket = peer.second.get();
        if (socket->is_open())
            return true;
    }
    return false;
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

        // Loop through the results to find the first valid local IPv4 address
        for (; it != end; ++it) {
            asio::ip::tcp::endpoint ep = *it;
            asio::ip::address addr = ep.address();

            // Only return addresses that are local (private) IPv4 (e.g., 192.168.x.x, 10.x.x.x)
            if (addr.is_v4() && addr.to_string().find("192.168.") == 0 || addr.to_string().find("10.") == 0) {
                return addr.to_string();  // Return the local IPv4 address
            }
        }

        return "No valid local IPv4 address found";
    }
    catch (std::exception& e) {
        return "Unable to retrieve IP: " + std::string(e.what());
    }
}
//std::string NetworkManager::getLocalIPAddress() {
//    try {
//        asio::ip::tcp::resolver resolver(io_context_);
//        asio::ip::tcp::resolver::query query(asio::ip::host_name(), "");
//        asio::ip::tcp::resolver::iterator it = resolver.resolve(query);
//        asio::ip::tcp::resolver::iterator end;
//
//        std::string fallbackIp;
//
//        // Loop through available IP addresses
//        for (; it != end; ++it) {
//            asio::ip::tcp::endpoint ep = *it;
//            std::string ipAddress = ep.address().to_string();
//
//            if (ipAddress.find("25.") == 0) {  // Check for Hamachi IP
//                return ipAddress;  // Return immediately if Hamachi IP is found
//            }
//            else if (ep.address().is_v4() && !ep.address().is_loopback()) {
//                // Save the first valid local IP as a fallback
//                fallbackIp = ipAddress;
//            }
//        }
//
//        // If no Hamachi IP was found, return the fallback IP or an error message
//        return !fallbackIp.empty() ? fallbackIp : "No valid local IPv4 address found";
//
//    }
//    catch (const std::exception& e) {
//        return "Unable to retrieve IP: " + std::string(e.what());
//    }
//}

// Auxiliar Operations END ---------------------------------------------------------------------- END


//Message Operations ------------------------------------------------------------------------------

void NetworkManager::handleMessage(std::shared_ptr<asio::ip::tcp::socket> socket, const std::vector<unsigned char>& buffer, std::size_t length) {
    // Create a temporary vector with the actual data received, up to 'length'
    std::vector<unsigned char> actualBuffer(buffer.begin(), buffer.begin() + length);
    Message message = deserializeMessage(actualBuffer);

    switch (message.type) {
    case MessageType::MarkerUpdate:
    {
        // Deserialize the marker entity from the message payload
        size_t offset = 0;
        flecs::entity marker = Serializer::deserializeMarkerEntity(message.payload, offset, ecs);
        std::lock_guard<std::mutex> lock(receivedQueueMutex);
        receivedQueue.push({ message.type, "", "", marker });
        break;
    }
    case MessageType::CreateMarker:
    {
        // Deserialize the marker entity from the message payload
        size_t offset = 0;
        flecs::entity marker = Serializer::deserializeMarkerEntity(message.payload, offset, ecs);
        std::lock_guard<std::mutex> lock(receivedQueueMutex);
        receivedQueue.push({ message.type, "", "", marker });
        break;
    }
    case MessageType::ChatMessage:
    {
        // Deserialize the marker entity from the message payload
        size_t offset = 0;
        auto user = Serializer::deserializeString(message.payload, offset);
        auto chat_message = Serializer::deserializeString(message.payload, offset);

        std::lock_guard<std::mutex> lock(receivedQueueMutex);
        receivedQueue.push({ message.type, user, chat_message, {} });
        break;
    }
    case MessageType::ImageChunk:
    {
        size_t offset = 0;
        std::string imagePath = Serializer::deserializeString(message.payload, offset);
        int chunkNumber = Serializer::deserializeInt(message.payload, offset);
        int totalChunks = Serializer::deserializeInt(message.payload, offset);
        std::vector<unsigned char> chunkData(message.payload.begin() + offset, message.payload.end());
        handleImageChunk(imagePath, chunkNumber, totalChunks, chunkData);
        break;
    }
    default:
        std::cout << "Unknown message type received.\n";
        break;
    }
}


void NetworkManager::handleImageChunk(const std::string& imagePath, int chunkNumber, int totalChunks, const std::vector<unsigned char>& chunkData) {
    // Ensure we have an entry for this image in the map
    if (imageChunksReceived.find(imagePath) == imageChunksReceived.end()) {
        imageChunksReceived[imagePath] = std::unordered_set<int>();
        imageBufferMap[imagePath] = std::vector<unsigned char>();  // Initialize an empty buffer
    }

    // If this chunk is not already received
    if (imageChunksReceived[imagePath].find(chunkNumber) == imageChunksReceived[imagePath].end()) {
        // Mark this chunk as received
        imageChunksReceived[imagePath].insert(chunkNumber);
        auto& buffer = imageBufferMap[imagePath];

        // Append the chunk to the buffer
        buffer.insert(buffer.end(), chunkData.begin(), chunkData.end());

        // Check if all chunks have been received
        if (imageChunksReceived[imagePath].size() == totalChunks) {
            std::cout << "All chunks received for image: " << imagePath << std::endl;
            finalizeImage(imagePath);  // Reassemble the image and save it
        }
    }
}

void NetworkManager::finalizeImage(const std::string& imagePath) {
    auto& buffer = imageBufferMap[imagePath];

    // Now that all chunks are received, save the image to a file or process it
    std::ofstream outFile(imagePath, std::ios::binary);
    if (outFile.is_open()) {
        outFile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        outFile.close();
        std::cout << "Image saved: " << imagePath << std::endl;
    }
    else {
        std::cerr << "Failed to open image file for writing: " << imagePath << std::endl;
    }

    // Cleanup: remove the cached data for this image
    imageBufferMap.erase(imagePath);
    imageChunksReceived.erase(imagePath);
}



void NetworkManager::startReceiving(std::shared_ptr<asio::ip::tcp::socket> socket) {
    auto buffer = std::make_shared<std::vector<unsigned char>>(1024);  // 1KB buffer size
    socket->async_read_some(asio::buffer(*buffer), [this, socket, buffer](asio::error_code ec, std::size_t length) {
        if (!ec) {
            std::cout << "Receiving Message from " << socket->remote_endpoint().address().to_string() << std::endl;
            std::string data(buffer->begin(), buffer->begin() + length);
            if (data.rfind("PASSWORD:", 0) == 0) {  // Check if the message starts with "PASSWORD:"
                std::string receivedPassword = data.substr(9);  // Extract the password part
                verifyPassword(socket, receivedPassword);
            } else {
                handleMessage(socket, *buffer, length);  // Handle other messages
            }

            startReceiving(socket);  // Continue reading from this peer
        } else {
            std::cout << "Error in receiving: " << ec.message() << std::endl;
        }
    });
}

std::vector<unsigned char> NetworkManager::serializeMessage(const Message& message) {
    // Prepare a vector to hold the serialized message
    std::vector<unsigned char> buffer;

    // Serialize the message type
    MessageType type = message.type;
    unsigned char* typePtr = reinterpret_cast<unsigned char*>(&type);
    buffer.insert(buffer.end(), typePtr, typePtr + sizeof(MessageType));

    // Serialize the payload
    buffer.insert(buffer.end(), message.payload.begin(), message.payload.end());

    return buffer;
}

Message NetworkManager::deserializeMessage(const std::vector<unsigned char>& buffer) {
    // Check if the buffer is large enough
    if (buffer.size() < sizeof(MessageType)) {
        throw std::runtime_error("Buffer is too small to contain a valid message.");
    }

    // Extract the message type
    MessageType type;
    std::memcpy(&type, buffer.data(), sizeof(MessageType));

    // Extract the payload
    std::vector<unsigned char> payload(buffer.begin() + sizeof(MessageType), buffer.end());

    // Return the reconstructed message
    return Message{ type, payload };
}


//Message Operations END ------------------------------------------------------------------------------------- END

bool NetworkManager::connectToPeer(const std::string& connection_string) {
    std::regex rgx(R"(runic:([\d.]+):(\d+)\??(.*))");
    std::smatch match;

    // Parse the connection string using regex
    if (std::regex_match(connection_string, match, rgx)) {
        std::string server_ip = match[1];             // Extract the server's Hamachi IP address
        unsigned short port = std::stoi(match[2]);    // Extract the port
        std::string password = match[3];              // Extract the optional password

        // Get the client's Hamachi IP
        std::string client_ip = getLocalIPAddress();
        if (client_ip == "No valid local IPv4 address found" || client_ip.find("Unable to retrieve IP") == 0) {
            std::cerr << "Failed to find a valid Hamachi IP for the client." << std::endl;
            return false;
        }

        try {
            asio::ip::tcp::resolver resolver(io_context_);
            auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);

            // Bind the socket to the Hamachi IP to ensure it uses this interface
            //asio::ip::tcp::endpoint client_endpoint(asio::ip::make_address(client_ip), 0); // Port 0 lets the OS choose a free port
            //socket->set_option(asio::socket_base::keep_alive(true));
            //socket->open(client_endpoint.protocol());
            //socket->bind(client_endpoint);

            // Define the server endpoint
            asio::ip::tcp::endpoint server_endpoint(asio::ip::make_address(server_ip), port);

            // Perform a synchronous connect operation
            socket->connect(server_endpoint);

            std::cout << "Successfully connected to server at " << server_ip << ":" << port << std::endl;

            if (socket->is_open()) {
                auto local_port = socket->local_endpoint().port();

                std::cout << "Connection established. Socket is open." << std::endl;
                std::cout << "Client Hamachi IP: " << client_ip
                    << " | Local Port: " << local_port << std::endl;
                std::cout << "Connected to: " << socket->remote_endpoint().address().to_string()
                    << " | Remote Port: " << socket->remote_endpoint().port() << std::endl;

            }

            if (!acceptor_.is_open()) {
                startServer(port, true);
            }
            // Send the password to the server
            sendPassword(socket, password);

            // Start receiving messages from the server asynchronously
            startReceiving(socket);

            // Add the connection to connected peers
            if (socket->is_open()) {
                connectedPeers.emplace(socket->remote_endpoint().address().to_string(), socket);
            }

            return true;

        }
        catch (const std::exception& e) {
            std::cerr << "Failed to connect to " << server_ip << ":" << port
                << " | Exception: " << e.what() << std::endl;
            return false;
        }

    }
    else {
        std::cerr << "Invalid connection string format!" << std::endl;
        return false;
    }
}


Message NetworkManager::buildCreateMarkerMessage(flecs::entity marker_entity) {
    Message create_marker_message = {};
    create_marker_message.type = MessageType::CreateMarker;
    std::vector<unsigned char> buffer;
    Serializer::serializeMarkerEntity(buffer, marker_entity, ecs);
    create_marker_message.payload = buffer;
    return create_marker_message;
}

Message NetworkManager::buildUpdateMarkerMessage(flecs::entity marker_entity) {
    Message update_marker_message = {};
    update_marker_message.type = MessageType::MarkerUpdate;
    std::vector<unsigned char> buffer;
    Serializer::serializeMarkerEntity(buffer, marker_entity, ecs);
    update_marker_message.payload = buffer;
    return update_marker_message;
}

Message NetworkManager::buildCreateBoardMessage(flecs::entity board_entity) {
    Message create_board_message = {};
    create_board_message.type = MessageType::CreateBoard;
    std::vector<unsigned char> buffer;
    Serializer::serializeBoardEntity(buffer, board_entity, ecs);
    create_board_message.payload = buffer;
    return create_board_message;
}


Message NetworkManager::buildChatMessage(std::string user, std::string message) {
    Message create_board_message = {};
    create_board_message.type = MessageType::ChatMessage;
    std::vector<unsigned char> buffer;
    Serializer::serializeString(buffer, user);
    Serializer::serializeString(buffer, message);
    create_board_message.payload = buffer;
    return create_board_message;
}


std::string NetworkManager::getUsername() {
    return getLocalIPAddress();
}

// Handle when a peer connects
void  NetworkManager::handlePeerConnected(std::shared_ptr<asio::ip::tcp::socket> socket) {
    std::cout << "Client connected from: "
        << socket->remote_endpoint().address().to_string()
        << ":" << socket->remote_endpoint().port() << std::endl;
    connectedPeers.emplace(socket->remote_endpoint().address().to_string(), socket);
    // Begin receiving messages from  this peer
    startReceiving(socket);
}

// Method to accept incoming connections
void NetworkManager::acceptConnections() {
    auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
    acceptor_.async_accept(*socket, [this, socket](asio::error_code ec) {
        if (!ec) {
            handlePeerConnected(socket);
        }
        else {
            std::cerr << "ERROR CONNECTING PEER: " << ec.message() << std::endl;
        }
        acceptConnections();  // Continue accepting new connections
        });
}

//Connect to Peer Operations END------------------------------------------------------------------------------------- END


//Password Operations ------------------------------------------------------------------------------------------------
void NetworkManager::sendPassword(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& password) {
    try {
        std::string message = "PASSWORD:" + password;
        // Write the message synchronously
        asio::write(*socket, asio::buffer(message));

        std::cout << "Password sent to server.\n";
    }
    catch (const asio::system_error& e) {
        std::cout << "Failed to send password: " << e.what() << std::endl;
    }
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
    try {
        std::string message = "DISCONNECT:" + reason;
        // Write the message synchronously
        asio::write(*socket, asio::buffer(message));

        std::cout << "Disconnection message sent: " << reason << std::endl;
        socket->close();  // Close the connection after sending the disconnection message
    }
    catch (const asio::system_error& e) {
        std::cout << "Failed to send disconnection message: " << e.what() << std::endl;
    }
}
//Password Operations END -------------------------------------------------------------------------------------------------- END


void NetworkManager::processSentMessages() {
    // Process 5 real-time messages for every 1 non-real-time message
    int realTimeMessagesProcessed = 0;
    // Process all real-time messages first
    while (!realTimeQueue.empty() && realTimeMessagesProcessed < 5) {
        Message message = realTimeQueue.front();
        realTimeQueue.pop();
        sendMessageToPeers(message);  // Function to send the message to peers
        realTimeMessagesProcessed++;
    }

    if (!nonRealTimeQueue.empty()) {
        Message message = nonRealTimeQueue.front();
        nonRealTimeQueue.pop();
        sendMessageToPeers(message);  // Function to send the message to peers
        realTimeMessagesProcessed = 0;  // Reset the counter after sending the non-real-time message
    }
}

void NetworkManager::sendMessageToPeers(const Message& message) {
    for (auto& peer : connectedPeers) {
        std::vector<unsigned char> buffer = serializeMessage(message);
        auto socket = peer.second;  // Get the socket

        std::cout << "Sending Message to " << socket->remote_endpoint().address().to_string() << std::endl;
        // Allocate a shared pointer to the buffer to keep it alive for async operation
        auto sendBuffer = std::make_shared<std::vector<unsigned char>>(std::move(buffer));

        // Use asio::async_write instead of asio::write to send the message asynchronously
        asio::async_write(*socket, asio::buffer(*sendBuffer),
            [this, sendBuffer, socket](asio::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    std::cout << "Message sent successfully to " << socket->remote_endpoint().address().to_string() << std::endl;
                }
                else {
                    std::cerr << "Error sending message: " << ec.message() << std::endl;
                }
            });
    }
}

//Sending Messages Operation END ------------------------------------------------------------------------------------------- END




//Queue Operations -----------------------------------------------------------------------------------------
void NetworkManager::queueMessage(const Message& message) {
    if (message.type != MessageType::ImageChunk) {
        realTimeQueue.push(message);  // High-priority queue
    }
    else {
        nonRealTimeQueue.push(message);  // Low-priority queue
    }
}

//Queue Operations ----------------------------------------------------------------------------------------- END

//bool NetworkManager::connectToPeer(const std::string& connection_string) {
//    std::regex rgx(R"(runic:([\d.]+):(\d+)\??(.*))");
//    std::smatch match;
//
//    // Parse the connection string using regex
//    if (std::regex_match(connection_string, match, rgx)) {
//        std::string ip = match[1];             // Extract the IP address
//        unsigned short port = std::stoi(match[2]);  // Extract the port
//        std::string password = match[3];       // Extract the optional password
//        
//        // Establish a connection using ASIO
//        try {
//            asio::ip::tcp::resolver resolver(io_context_);
//            auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
//            asio::ip::tcp::endpoint endpoint(asio::ip::make_address(ip), port);
//
//            //asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(ip, std::to_string(port));
//            //asio::connect(*socket, endpoints);
//
//            socket->async_connect(endpoint, [this, ip, port, socket, password](asio::error_code ec) {
//                if (!ec) {
//                    std::cout << "Successfully connected to server at " << ip << ":" << port << std::endl;
//                
//                    if (socket->is_open()) {
//                        std::cout << "Connection established. Socket is open." << std::endl;
//                        std::cout << "Local IP: " << socket->local_endpoint().address().to_string()
//                                    << " | Local Port: " << socket->local_endpoint().port() << std::endl;
//                        std::cout << "Connected to: " << socket->remote_endpoint().address().to_string()
//                                    << " | Remote Port: " << socket->remote_endpoint().port() << std::endl;
//                    }
//                    // Send the password to the server
//                    sendPassword(socket, password);
//                    startReceiving(socket);  // Start receiving messages from the server
//
//                } else {
//                    std::cout << "Connection failed: " << ec.message() << std::endl;
//                }
//            });
//
//            //// You can now start communication with the peer
//            //std::cout << "Connected to peer: " << ip << ":" << port << std::endl;
//            //startReceiving(socket);  // Start receiving messages from this peer
//
//            return true;
//        }
//        catch (std::exception& e) {
//            std::cerr << "Failed to connect: " << e.what() << std::endl;
//            return false;
//        }
//
//    }
//    else {
//        std::cerr << "Invalid connection string format!" << std::endl;
//        return false;
//    }
//}

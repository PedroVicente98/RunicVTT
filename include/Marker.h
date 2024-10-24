#pragma once
#include <string>
#include "Renderer.h"
#include "Shader.h"
#include "VertexBuffer.h"
#include "IndexBuffer.h"
#include "VertexArray.h"
#include "VertexBufferLayout.h"
#include "Texture.h"

/*
//Network Connection Logic for Password

//Client-Side Implementation: Connecting to a Peer
void NetworkManager::connectToPeer(const std::string& ip, unsigned short port, const std::string& password) {
    auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
    asio::ip::tcp::endpoint endpoint(asio::ip::make_address(ip), port);

    socket->async_connect(endpoint, [this, socket, password](asio::error_code ec) {
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
}

//Client-Side: Send Password to Server
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

//Server-Side: Accepting Connections
void NetworkManager::acceptConnections() {
    auto socket = std::make_shared<asio::ip::tcp::socket>(io_context_);
    acceptor_.async_accept(*socket, [this, socket](asio::error_code ec) {
        if (!ec) {
            std::cout << "New peer connected.\n";
            startReceiving(socket);  // Begin receiving the password
        }
        acceptConnections();  // Continue accepting new connections
    });
}

//Server-Side: Start Receiving Messages
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

//Server-Side: Password Verification
void NetworkManager::verifyPassword(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& receivedPassword) {
    if (receivedPassword != network_password) {
        std::cout << "Invalid password from peer. Disconnecting.\n";
        sendDisconnectMessage(socket, "Invalid password");
    } else {
        std::cout << "Password verified. Peer is authenticated.\n";
        handlePeerConnected(socket);  // Handle further connection logic like sending game state
    }
}

//Server-Side: Sending a Disconnection Message
void NetworkManager::sendDisconnectMessage(std::shared_ptr<asio::ip::tcp::socket> socket, const std::string& reason) {
    std::string message = "DISCONNECT:" + reason;
    asio::async_write(*socket, asio::buffer(message), [this, socket](asio::error_code ec, std::size_t length) {
        if (!ec) {
            std::cout << "Disconnection message sent: " << reason << std::endl;
            socket->close();  // Close the connection after sending the disconnection message
        } else {
            std::cout << "Failed to send disconnection message: " << ec.message() << std::endl;
        }
    });
}

Server-Side: Handle Peer Connected
void NetworkManager::handlePeerConnected(std::shared_ptr<asio::ip::tcp::socket> socket) {
    std::cout << "Peer authenticated and connected.\n";

    // Here you would send the initial game state (e.g., map, markers, etc.)
    sendGameState(socket);

    // Further logic can go here to manage peer communication
}


*/
class Marker {
public:
	Marker(const std::string& texturePath, const glm::vec2& pos, const glm::vec2& sz, Renderer& rend);
	~Marker();
	void draw(Shader& shader, const glm::mat4& viewProjMatrix);
	/*void renderEditWindow();
	void setMarkerImage();
	void changeSize();
	void changePosition();
	void addColorCircle();*/
	Renderer renderer;
	glm::vec2 position;
	glm::vec2 size;
	Texture texture;
	VertexArray va;
	VertexBuffer vb;
	IndexBuffer ib;
private:
};

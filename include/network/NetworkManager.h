#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include "PeerLink.h"
#include "SignalingServer.h"
#include "SignalingClient.h"
#include "flecs.h"
#include "rtc/configuration.hpp"
#include "imgui.h"
#include <regex>

#include <vector>
#include <rtc/peerconnection.hpp>
#include <nlohmann/json.hpp>
#include <future>
#include <iostream>

enum class Role {
    NONE,
    GAMEMASTER,
    PLAYER
};

class NetworkManager {
public:
    NetworkManager(flecs::world ecs);
    ~NetworkManager();

    void startServer(std::string internal_ip_address, unsigned short port);
    void closeServer();
    bool connectToPeer(const std::string& connectionString);

    bool disconectFromPeers();

    void allowPort(unsigned int port);
    void disallowPort(unsigned short port);
    // Utility methods
    std::string getNetworkInfo(bool external);     // Get network info (IP and port) (server utility)
    std::string getLocalIPAddress();  // Get local IP address (server utility)
    std::string getExternalIPAddress();  // Get external IP address (server utility)

    void setPort(unsigned int port) { this->port = port; }
private:
    flecs::world ecs;
    unsigned int port = 8080;
    char network_password[124] = "\0";
    std::string external_ip_address = "";
    std::string local_ip_address = "";

    void connectToWebRTCPeer(const std::string& peerId);
    void receiveSignal(const std::string& peerId, const std::string& message);
    void sendSignalToPeer(const std::string& peerId, const std::string& message);

    std::unique_ptr<SignalingServer> signalingServer;
    std::unique_ptr<SignalingClient> signalingClient;
    Role peer_role;
    std::unordered_map<std::string, std::shared_ptr<PeerLink>> peers;
    rtc::Configuration rtcConfig;

public:

    std::string createAndEncodeOffer(std::shared_ptr<rtc::PeerConnection> pc);
    std::string decodeOfferAndCreateAnswer(const std::string& offerString, std::shared_ptr<rtc::PeerConnection> pc);
    void decodeAnswerAndFinalize(const std::string& answerString, std::shared_ptr<rtc::PeerConnection> pc);
    void createPeerConnection();

    std::string base64_encode(const std::string& in);
    std::string base64_decode(const std::string& in);

    void renderNetworkOfferer();
    void renderNetworkAnswerer();


    void parseConnectionString(std::string connection_string, std::string &server_ip, unsigned short &port, std::string &password) {
        std::regex rgx(R"(runic:([\d.]+):(\d+)\??(.*))");
            std::smatch match;
            // Parse the connection string using regex
            if (std::regex_match(connection_string, match, rgx)) {
                server_ip = match[1];             // Extract the server's Hamachi IP address
                port = std::stoi(match[2]);    // Extract the port
                password = match[3];              // Extract the optional password
            }
    }

    unsigned short getPort() const { return port; };
    std::string getNetworkPassword() const { return std::string(network_password); };

    void setNetworkPassword(const char* password) {
        std::cout << "BEFORE NETWORK PASSWORD" << password << std::endl;
        strncpy(network_password, password, sizeof(network_password) - 1);
        network_password[sizeof(network_password) - 1] = '\0';
        std::cout << "NETWORK PASSWORD" << network_password << std::endl;
    }

    Role getPeerRole() const { return peer_role; }

    // Função para exibir o popup
    void ShowPortForwardingHelpPopup(bool* p_open) {
        if (*p_open) {
            ImGui::OpenPopup("Port Forwarding Failed");
        }

        if (ImGui::BeginPopupModal("Port Forwarding Failed", p_open, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Automatic port forwarding failed. This is likely because your router does not\n"
                        "support UPnP or it is disabled. You can still host by choosing one of the\n"
                        "following three options:");

            ImGui::Separator();

            if (ImGui::BeginTabBar("MyTabBar")) {

                // Tab 1: Enable UPnP
                if (ImGui::BeginTabItem("Enable UPnP")) {
                    ImGui::Text("This is the easiest solution and will allow automatic port forwarding to work.");
                    ImGui::Spacing();
                    ImGui::BulletText("Access your router's administration page. This is usually done by\n"
                                      "typing its IP address (e.g., 192.168.1.1) in your browser.");
                    ImGui::BulletText("Log in with your router's credentials.");
                    ImGui::BulletText("Look for a setting named 'UPnP' or 'Universal Plug and Play' and enable it.");
                    ImGui::BulletText("Save the settings and restart your router if necessary.");
                    ImGui::BulletText("Try hosting again. The application should now automatically forward the port.");
                    ImGui::EndTabItem();
                }

                // Tab 2: Manual Port Forwarding
                if (ImGui::BeginTabItem("Manual Port Forwarding")) {
                    ImGui::Text("This method always works but requires you to configure your router manually.");
                    ImGui::Spacing();
                    ImGui::BulletText("Find your local IP address. Open Command Prompt and type 'ipconfig' to find it\n"
                                      "(e.g., 192.168.1.100).");
                    ImGui::BulletText("Access your router's administration page.");
                    ImGui::BulletText("Look for a setting named 'Port Forwarding', 'Virtual Server', or 'NAT'.");
                    ImGui::BulletText("Create a new rule with the following details:");
                    ImGui::Indent();
                    ImGui::BulletText("Protocol: TCP");
                    ImGui::BulletText("Internal Port: [Your application's port, e.g., 8080]");
                    ImGui::BulletText("External Port: [The same port]");
                    ImGui::BulletText("Internal IP: [Your local IP from step 1]");
                    ImGui::Unindent();
                    ImGui::BulletText("Save the rule and try hosting again.");
                    ImGui::EndTabItem();
                }

                // Tab 3: Use a VPN (Hamachi)
                if (ImGui::BeginTabItem("Use a VPN (Hamachi)")) {
                    ImGui::Text("A VPN creates a virtual local network, bypassing the need for port forwarding.");
                    ImGui::Spacing();
                    ImGui::BulletText("Download and install a VPN client like Hamachi.");
                    ImGui::BulletText("Create a new network within the VPN client.");
                    ImGui::BulletText("Share the network name and password with your friends.");
                    ImGui::BulletText("Instruct your friends to join your network.");
                    ImGui::BulletText("On the host screen, you should be able to see your VPN IP address.\n"
                                      "Use this IP to host your session.");
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }

            if (ImGui::Button("Close")) {
                *p_open = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    };



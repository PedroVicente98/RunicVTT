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

enum class Role { GAMEMASTER, PLAYER };

class NetworkManager {
public:
    NetworkManager(flecs::world ecs);
    ~NetworkManager();

    void startServer(unsigned short port);
    void closeServer();
    bool connectToPeer(const std::string& connectionString);

private:
    flecs::world ecs;


    void connectToWebRTCPeer(const std::string& peerId);
    void receiveSignal(const std::string& peerId, const std::string& message);
    void sendSignalToPeer(const std::string& peerId, const std::string& message);

    Role peer_role;

    std::unordered_map<std::string, std::shared_ptr<PeerLink>> peers;

    std::unique_ptr<SignalingServer> signalingServer;
    std::unique_ptr<SignalingClient> signalingClient;

    rtc::Configuration rtcConfig;


public:

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



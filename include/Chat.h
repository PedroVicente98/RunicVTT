#pragma once
#include "imgui.h"
#include <iostream>
#include "GL/glew.h"
#include <vector>
#include <string>
#include <regex>
#include <cstdlib>  // For std::rand()
#include <ctime>    // For std::time>

class Chat {
public:

    //Chat(NetworkManager* network_manager) : network_manager(network_manager){};
    Chat() {};
    ~Chat() {};
    // Message structure with different types of content
    struct Message {
        enum MessageType { TEXT, IMAGE, LINK } type;
        std::string sender;
        std::string content;
        GLuint imageTexture = 0;  // Only used if type is IMAGE
        //std::function<void()> onClick;  // Action for link
    };

    void clearChat() {
        messages.clear();
    }

    std::vector<Message> messages;  

    // Adding text message
    void addTextMessage(const std::string& sender, const std::string& content) {
        //auto user_name = network_manager->getUsername();
       // auto  message = network_manager->buildChatMessage(user_name, content);
       // network_manager->queueMessage(message);
        messages.push_back({ Message::TEXT, sender, content });
    }
    void addReceivedTextMessage(const std::string& sender, const std::string& content) {
        messages.push_back({ Message::TEXT, sender, content });
    }

    // Adding image message
    void addImageMessage(const std::string& sender, GLuint textureId) {
        messages.push_back({ Message::IMAGE, sender, "", textureId });
    }

    // Adding link message
    void addLinkMessage(const std::string& sender, const std::string& text, const std::string& url) {
        messages.push_back({ Message::LINK, sender, text, 0,/* [url]() {
            // Use the system's command processor to open the URL in the default web browser
            #ifdef _WIN32
            std::string command = "start " + url;
            #elif __APPLE__
            std::string command = "open " + url;
            #else
            std::string command = "xdg-open " + url;
            #endif
            std::system(command.c_str());
        } */ });
    }

    // Function to roll dice
    int rollDice(int count, int sides) {
        int total = 0;
        for (int i = 0; i < count; ++i) {
            total += (rand() % sides) + 1;
        }
        return total;
    }

    // Process command input
    std::string processCommand(const std::string& input) {
        std::regex diceRegex(R"((\d+)d(\d+))");
        std::regex modRegex(R"(([+-]\d+)$)");
        std::smatch matches;
        std::string segment = input;
        std::vector<std::pair<int, int>> rolls;
        int modifier = 0;

        while (std::regex_search(segment, matches, diceRegex)) {
            rolls.push_back({ std::stoi(matches[1]), std::stoi(matches[2]) });
            segment = matches.suffix();
        }

        if (std::regex_search(input, matches, modRegex)) {
            modifier = std::stoi(matches[1]);
        }

        int total = 0;
        std::string result = "Rolls: ";
        for (const auto& roll : rolls) {
            int rollResult = rollDice(roll.first, roll.second);
            total += rollResult;
            result += std::to_string(roll.first) + "d" + std::to_string(roll.second) + "=" + std::to_string(rollResult) + ", ";
        }

        total += modifier;
        result += "Modifier: " + std::to_string(modifier) + ", Total: " + std::to_string(total);
        return result;
    }

    // Display messages in ImGui window
    void displayMessages() {
        for (const auto& message : messages) {
            switch (message.type) {
            case Message::TEXT:
                ImGui::Text("%s: %s", message.sender.c_str(), message.content.c_str());
                break;
            case Message::IMAGE:
                ImGui::Image((void*)(intptr_t)message.imageTexture, ImVec2(100, 100));
                break;
            case Message::LINK:
                if (ImGui::Button(message.content.c_str())) {
                    //message.onClick();
                }
                break;
            }
        }
    }

    // Process input to determine its type and handle accordingly
    void processInput(const std::string& input) {
        std::regex commandRegex(R"(/(roll\s+)?(\d+)d(\d+)([+-]\d+)?(\+\d+d\d+)*)"); // Extended to handle /roll and modifiers
        std::regex linkRegex(R"(https?://[^\s]+)");
        std::regex imageRegex(R"(https?://[^\s]+(\.png|\.jpg|\.jpeg))");
        auto user_name = "";// network_manager->getUsername();
        std::smatch matches;
        if (std::regex_match(input, matches, commandRegex)) {
            std::string command = matches[0].str();
            std::string result = processCommand(command.substr(1));  // Remove leading '/'
            addTextMessage("System", result);  // Display result as a system message
        }
        else if (std::regex_search(input, matches, imageRegex)) {
            addImageMessage(user_name, 0);  // Assuming texture ID is managed elsewhere
        }
        else if (std::regex_search(input, matches, linkRegex)) {
            addLinkMessage(user_name, matches[0].str(), matches[0].str()); // Add a link message
        }
        else {
            addTextMessage(user_name, input);  // Regular text message
        }
    }


    void renderChat() {
        ImGui::Begin("ChatWindow");
        ImGui::BeginChild("Messages", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), false, ImGuiWindowFlags_HorizontalScrollbar);
        displayMessages();
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f); // Auto-scroll to bottom
        }
        ImGui::EndChild();

        ImGui::Separator();

        if (shouldFocusInput) {
            ImGui::SetKeyboardFocusHere();
            shouldFocusInput = false;
        }

        if (ImGui::InputText("##input", textInput, sizeof(textInput), ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string input(textInput);
            processInput(input);
            strcpy_s(textInput, "");  // Clear input box
            shouldFocusInput = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Send")) {
            std::string input(textInput);
            processInput(input);
            strcpy_s(textInput, "");  // Clear input box
            shouldFocusInput = true;
        }

        ImGui::End();
    }

private:
    //NetworkManager* network_manager;
    char textInput[256] = "";
    bool shouldFocusInput = false;
};

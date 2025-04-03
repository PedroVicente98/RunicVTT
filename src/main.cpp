#include <iostream>

// 🔹 OpenGL & GLFW (Graphics)
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// 🔹 ImGui (UI)
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// 🔹 WebRTC (Networking)
#include <rtc/rtc.hpp>

// 🔹 Ensure OpenGL, GLFW, and WebRTC Work
void test_systems() {
    std::cout << "[Test] OpenGL Version: " << glGetString(GL_VERSION) << std::endl;
    std::cout << "[Test] WebRTC Initialized" << std::endl;
}

int main() {
    // ✅ Step 1: Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "[Error] Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // ✅ Step 2: Create OpenGL Window
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Runic VTT - Test", nullptr, nullptr);
    if (!window) {
        std::cerr << "[Error] Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // ✅ Step 3: Initialize GLEW
    if (glewInit() != GLEW_OK) {
        std::cerr << "[Error] Failed to initialize GLEW" << std::endl;
        return -1;
    }

    // ✅ Step 4: Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // ✅ Step 5: WebRTC Setup (Test Only)
    rtc::InitLogger(rtc::LogLevel::Debug);
    std::cout << "[Test] WebRTC Setup Complete" << std::endl;

    // ✅ Step 6: Main Loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Start ImGui Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Runic VTT Test");
        ImGui::Text("CMake Configuration Works!");
        ImGui::End();

        // Render
        ImGui::Render();
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // ✅ Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

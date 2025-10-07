#pragma once
#include <GL/glew.h>
#include "imgui.h"
#include <iostream>
#include <vector>
#include <filesystem>
#include <string>
#include <thread>
#include <shared_mutex>
#include <condition_variable>
#include "Texture.h"
//#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

class DirectoryWindow
{
public:
    DirectoryWindow(std::string directoryPath, std::string directoryName)
    {
        this->directoryPath = directoryPath;
        this->directoryName = directoryName;
    }

    struct ImageData
    {
        GLuint textureID;
        glm::vec2 size;
        std::string filename;
    };

    // Function to truncate a string if it exceeds a certain length
    std::string TruncateString(const std::string& str, size_t maxLength)
    {
        if (str.length() > maxLength)
        {
            return str.substr(0, maxLength) + "...";
        }
        return str;
    }

    // Function to generate the texture IDs
    void generateTextureIDs()
    {
        // Wait for the monitoring thread to finish one full loop
        //{
        //    std::shared_lock<std::shared_mutex> lock(imagesMutex);  // Shared lock: waits if the thread is still writing
        //    if (!first_scan_done) {
        //        lock.unlock();
        //        std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Wait and retry
        //        generateTextureIDs();  // Retry generating textures
        //        return;
        //    }
        //}

        while (true)
        {
            {
                std::shared_lock<std::shared_mutex> lock(imagesMutex);
                if (first_scan_done)
                    break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Now generate textures
        std::unique_lock<std::shared_mutex> lock(imagesMutex); // Unique lock to write (generate textures)
        for (auto& image : images)
        {
            if (image.textureID == 0)
            {
                std::string path_file = directoryPath + "\\" + image.filename;
                image = LoadTextureFromFile(path_file.c_str());
            }
        }
    }

    void renderDirectory(bool is_map_directory = false)
    {
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse;
        if (is_map_directory == true)
        {
            window_flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize;
        }

        ImGui::SetNextWindowSizeConstraints(ImVec2(ImGui::GetIO().DisplaySize.x * 0.1, ImGui::GetIO().DisplaySize.y * 0.1), ImVec2(ImGui::GetIO().DisplaySize.x - 200, ImGui::GetIO().DisplaySize.y));
        ImGui::Begin(directoryName.c_str(), NULL, window_flags);
        ImGui::Text("Path: %s", directoryPath.c_str());
        ImGui::Separator();

        const float imageSize = 128.0f;
        float window_width = ImGui::GetWindowWidth();
        ImVec2 content_region = ImGui::GetContentRegionAvail();
        int columns = (int)(content_region.x / imageSize);
        if (columns <= 0)
            columns = 1;
        int count = 0;
        {

            std::shared_lock<std::shared_mutex> lock(imagesMutex);
            for (auto& image : images)
            {
                if (count % columns != 0)
                    ImGui::SameLine();
                std::string path_file = directoryPath + "\\" + image.filename.c_str();
                if (image.textureID == 0)
                {
                    image = LoadTextureFromFile(path_file.c_str());
                }

                ImGui::BeginGroup();
                ImGui::PushID(count);

                if (is_map_directory == false)
                {
                    // Add drag-and-drop functionality for marker images
                    if (ImGui::ImageButton((void*)(intptr_t)image.textureID, ImVec2(imageSize, imageSize)))
                    {
                        selected_image = image;
                        std::cout << "Selected Image: " << image.filename << " | " << image.textureID << std::endl;
                        ImGui::OpenPopup("Image Popup");
                    }

                    // Begin the drag source for the markers
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                    {
                        ImGui::SetDragDropPayload("MARKER_IMAGE", &image, sizeof(image)); // Attach the marker image payload
                        ImGui::Text("Drag Marker: %s", image.filename.c_str());           // Tooltip while dragging
                        ImGui::EndDragDropSource();
                    }
                }
                else
                {
                    if (ImGui::ImageButton((void*)(intptr_t)image.textureID, ImVec2(imageSize, imageSize)))
                    {
                        selected_image = image;
                        std::cout << "Selected Image: " << image.filename << " | " << image.textureID << std::endl;
                        ImGui::OpenPopup("Image Popup");
                    }

                    if (ImGui::BeginPopup("Image Popup"))
                    {
                        ImGui::Text("File: %s", image.filename.c_str());
                        ImGui::EndPopup();
                    }
                }
                ImGui::TextWrapped("%s", TruncateString(image.filename.c_str(), 16).c_str());
                ImGui::PopID();
                ImGui::EndGroup();
                count++;
            }
        }
        ImGui::End();
    }

    // Método genérico para retornar a imagem selecionada
    ImageData getSelectedImage() const
    {
        return selected_image;
    }

    // Método para limpar a imagem selecionada
    void clearSelectedImage()
    {
        selected_image = {0, glm::vec2(), ""}; // Limpa a seleção
    }

    void startMonitoring()
    {
        monitorThread = std::thread(&DirectoryWindow::monitorDirectory, this, directoryPath);
    }

    void stopMonitoring()
    {
        running = false;
        if (monitorThread.joinable())
        {
            monitorThread.join();
        }
    }

    ~DirectoryWindow()
    {
        stopMonitoring();
    }

    ImageData getImageByPath(const std::string& file_name)
    {
        auto it = std::find_if(images.begin(), images.end(), [&](const ImageData& image)
                               {
            // Check if image.filename is a substring at the end of file_name
            if (file_name.size() >= image.filename.size()) {
                return file_name.compare(file_name.size() - image.filename.size(), image.filename.size(), image.filename) == 0;
            }
            return false; });

        if (it != images.end())
        {
            return *it;
        }
        else
        {
            throw std::runtime_error("Image not found: " + file_name);
        }
    }

    std::string directoryPath;
    std::string directoryName;

private:
    ImageData selected_image = {0, glm::vec2(), ""}; // Armazena a imagem selecionada
    std::vector<ImageData> images;
    std::thread monitorThread;
    bool running = true;
    std::shared_mutex imagesMutex;
    bool first_scan_done = false; // Flag to track when the first scan is complete

    // Function to find an ImageData by filename
    std::vector<ImageData>::iterator findImageByFilename(std::vector<ImageData>& images, const std::string& filename)
    {
        return std::find_if(images.begin(), images.end(), [&filename](const ImageData& image)
                            { return image.filename == filename; });
    }

    void monitorDirectory(const std::string& path)
    {
        std::vector<std::string> knownFiles;
        std::vector<ImageData> updatedImages;
        std::vector<ImageData> newImages;

        while (running)
        {
            std::vector<std::string> currentFiles;
            try
            {
                for (const auto& entry : std::filesystem::directory_iterator(path))
                {
                    if (entry.is_regular_file())
                    {
                        currentFiles.push_back(entry.path().filename().string());
                    }
                }
            }

            catch (const std::filesystem::filesystem_error& e)
            {
                std::cerr << "Filesystem error: " << e.what() << std::endl;
            }

            if (currentFiles != knownFiles)
            {

                std::unique_lock<std::shared_mutex> lock(imagesMutex); // Ensure thread-safe access to images

                for (auto it = knownFiles.begin(); it != knownFiles.end();)
                {
                    if (std::find(currentFiles.begin(), currentFiles.end(), *it) == currentFiles.end())
                    {
                        std::string removed_filename = *it;
                        auto image_it = findImageByFilename(images, removed_filename);
                        glDeleteTextures(1, &image_it->textureID);
                        image_it = images.erase(image_it);
                    }
                    it++;
                }

                // Add new images
                newImages.clear();
                for (const auto& filename : currentFiles)
                {
                    auto it = findImageByFilename(images, filename);
                    if (it == images.end())
                    {
                        // Simulate loading a new texture (replace with actual loading code)
                        std::string path_file = path + "\\" + filename.c_str();
                        newImages.push_back({0, glm::vec2(), filename});
                    }
                }
                knownFiles = std::move(currentFiles);

                images.insert(images.end(), newImages.begin(), newImages.end());
            }

            first_scan_done = true;
            std::this_thread::sleep_for(std::chrono::seconds(2)); // Check every 2 seconds
        }
    }

    ImageData LoadTextureFromFile(const char* path)
    {

        // Record the start time using std::chrono::high_resolution_clock
        //std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();

        int width, height, nrChannels;
        unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 4);
        stbi_set_flip_vertically_on_load(0); // Flip images vertically if needed
        if (!data)
        {
            std::cerr << "Failed to load texture: " << path << std::endl;
            return ImageData(0, glm::vec2(), "");
        }

        GLuint textureID[1];
        glGenTextures(1, textureID);
        glBindTexture(GL_TEXTURE_2D, textureID[0]);

        // Set the texture data
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

        GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GLCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

        stbi_image_free(data);

        // Record the end time using std::chrono::high_resolution_clock
        //std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

        // Calculate the duration in milliseconds
        //std::chrono::seconds duration = std::chrono::duration_cast<std::chrono::seconds>(end - start);

        // Output the duration in milliseconds
        //std::cout << "Operation for file "<< path <<"took " << duration.count() << " seconds" << std::endl;

        return ImageData(textureID[0], glm::vec2(width, height), path);
    }
};

//std::vector<ImageData> LoadImagesFromDirectory(const std::string& path) {
//    directoryPath = path;
//    std::vector<ImageData> newImages;
//    for (const auto& entry : std::filesystem::directory_iterator(path)) {
//        if (entry.is_regular_file()) {
//            std::string filename = entry.path().filename().string();
//            bool file_loaded = false;
//            for (const auto& image : images)
//                if (image.filename == filename) {
//                    file_loaded = true;
//                    break;
//                }
//            if (!file_loaded) {
//                GLuint textureID = LoadTextureFromFile(entry.path().string().c_str());
//                if (textureID) {
//                    newImages.push_back({ textureID, filename });
//                }
//                else {
//                    std::cerr << "Skipping image: " << entry.path().string() << " due to loading error." << std::endl;
//                }
//            }
//        }
//    }

//    // Swap the old images with the new ones
//    {
//        std::lock_guard<std::mutex> lock(imagesMutex);
//        images = std::move(newImages);
//    }

//    return images;
//}

//
// Include necessary headers
//
//// Structure to hold texture information
//
//// Function to load a texture from a file
//
//// Function to load images from a directory

//
//// Main function
//int main() {
//    // Initialize GLFW
//    if (!glfwInit()) {
//        std::cerr << "Failed to initialize GLFW" << std::endl;
//        return -1;
//    }
//
//    // Create a windowed mode window and its OpenGL context
//    GLFWwindow* window = glfwCreateWindow(1280, 720, "Image Directory", NULL, NULL);
//    if (!window) {
//        glfwTerminate();
//        return -1;
//    }
//
//    // Make the window's context current
//    glfwMakeContextCurrent(window);
//
//    // Initialize GLEW
//    if (glewInit() != GLEW_OK) {
//        std::cerr << "Failed to initialize GLEW" << std::endl;
//        return -1;
//    }
//
//    // Setup ImGui context
//    IMGUI_CHECKVERSION();
//    ImGui::CreateContext();
//    ImGuiIO& io = ImGui::GetIO(); (void)io;
//    ImGui::StyleColorsDark();
//
//    // Setup Platform/Renderer bindings
//    ImGui_ImplGlfw_InitForOpenGL(window, true);
//    ImGui_ImplOpenGL3_Init("#version 130");
//

//    // Main loop
//    while (!glfwWindowShouldClose(window)) {
//        // Poll and handle events
//        glfwPollEvents();
//
//        // Start the ImGui frame
//        ImGui_ImplOpenGL3_NewFrame();
//        ImGui_ImplGlfw_NewFrame();
//        ImGui::NewFrame();
//
//        // Left Docked Window
//        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
//        ImGui::SetNextWindowSize(ImVec2(200, ImGui::GetIO().DisplaySize.y), ImGuiCond_Always);
//        ImGui::Begin("Other Window", NULL, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
//        ImGui::Text("Other Window Content");
//        ImGui::End();
//
//        // Right Main Window
//        ImGui::SetNextWindowPos(ImVec2(200, 0), ImGuiCond_Once);
//        ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x - 200, ImGui::GetIO().DisplaySize.y), ImGuiCond_Always);
//        ImGui::Begin("Image Directory", NULL, ImGuiWindowFlags_NoCollapse);
//
//        // Display the path to the current folder
//        ImGui::Text("Path: %s", directoryPath.c_str());
//        ImGui::Separator();
//
//        // Display images in a grid
//        const float imageSize = 64.0f;
//        const int columns = (int)((ImGui::GetWindowWidth() - ImGui::GetStyle().ItemSpacing.x) / imageSize);
//        int count = 0;
//        for (const auto& image : images) {
//            if (count % columns != 0) ImGui::SameLine();
//            ImGui::BeginGroup();
//            ImGui::PushID(count);
//            if (ImGui::ImageButton((void*)(intptr_t)image.textureID, ImVec2(imageSize, imageSize))) {
//                ImGui::OpenPopup("Image Popup");
//            }
//            if (ImGui::BeginPopup("Image Popup")) {
//                ImGui::Text("File: %s", image.filename.c_str());
//                ImGui::EndPopup();
//            }
//            ImGui::TextWrapped("%s", image.filename.c_str());
//            ImGui::PopID();
//            ImGui::EndGroup();
//            count++;
//        }
//        ImGui::End();
//
//        // Rendering
//        ImGui::Render();
//        int display_w, display_h;
//        glfwGetFramebufferSize(window, &display_w, &display_h);
//        glViewport(0, 0, display_w, display_h);
//        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
//        glClear(GL_COLOR_BUFFER_BIT);
//        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
//
//        // Swap buffers
//        glfwSwapBuffers(window);
//    }
//
//    // Cleanup
//    for (auto& image : images) {
//        glDeleteTextures(1, &image.textureID);
//    }
//    ImGui_ImplOpenGL3_Shutdown();
//    ImGui_ImplGlfw_Shutdown();
//    ImGui::DestroyContext();
//
//    glfwDestroyWindow(window);
//    glfwTerminate();
//
//    return 0;
//}

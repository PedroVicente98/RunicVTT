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

enum class DirectoryKind
{
    MARKER = 1,
    MAP = 2
};

class DirectoryWindow
{
public:
    DirectoryWindow(std::string directoryPath, std::string directoryName, DirectoryKind kind_)
    {
        this->directoryPath = directoryPath;
        this->directoryName = directoryName;
        this->kind_ = kind_;
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

    void renderDirectory()
    {
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoCollapse;
        if (kind_ == DirectoryKind::MAP)
        {
            window_flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize;
        }

        ImGui::SetNextWindowSizeConstraints(ImVec2(ImGui::GetIO().DisplaySize.x * 0.1, ImGui::GetIO().DisplaySize.y * 0.1), ImVec2(ImGui::GetIO().DisplaySize.x - 200, ImGui::GetIO().DisplaySize.y));
        ImGui::Begin(directoryName.c_str(), NULL, window_flags);
        ImGui::Text("Path: %s", directoryPath.c_str());

        // --- Fixed header (no scrolling) ---
        if (kind_ == DirectoryKind::MARKER)
        {
            float minScale = 0.10f, maxScale = 10.0f;
            ImGui::Separator();
            ImGui::TextUnformatted("Default Marker Size Scale");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            if (ImGui::SliderFloat("##marker_size_slider", &global_size_slider, minScale, maxScale, "x%.2f"))
            {
                // Clamp just in case
                if (global_size_slider < minScale)
                    global_size_slider = minScale;
                if (global_size_slider > maxScale)
                    global_size_slider = maxScale;
            }
        }

        ImGui::Separator();

        float minScale = 25.0f, maxScale = 550.0f;
        ImGui::Separator();
        ImGui::TextUnformatted("Directory Thumb Pixel Size");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::SliderFloat("##image_size_slider", &imageSize, minScale, maxScale, "x%.2f"))
        {
            // Clamp just in case
            if (imageSize < minScale)
                imageSize = minScale;
            if (imageSize > maxScale)
                imageSize = maxScale;
        }

        ImGui::Separator();

        ImGui::BeginChild("DirectoryScrollRegion", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
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

                if (kind_ == DirectoryKind::MARKER)
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
        ImGui::EndChild();
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
        running.exchange(false, std::memory_order_relaxed);
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

    float getGlobalSizeSlider()
    {
        return global_size_slider;
    }

    std::string directoryPath;
    std::string directoryName;

    void applyPendingAssetChanges()
    {
        // 1) Steal queues quickly (minimize lock time)
        std::vector<std::string> removals;
        std::vector<std::pair<std::string, std::string>> adds;
        {
            std::lock_guard<std::mutex> qlk(pendingMtx);
            removals.swap(pendingRemovals);
            adds.swap(pendingAddPaths);
        }

        // 2) Process removals (GL-safe thread)
        if (!removals.empty())
        {
            std::unique_lock<std::shared_mutex> lk(imagesMutex);
            for (const auto& fname : removals)
            {
                auto it = std::find_if(images.begin(), images.end(),
                                       [&](const ImageData& im)
                                       { return im.filename == fname; });
                if (it != images.end())
                {
                    if (it->textureID != 0)
                        glDeleteTextures(1, &it->textureID);
                    images.erase(it);
                }
                // else: already gone — ignore
            }
        }

        // 3) Process additions (load textures here; GL-safe thread)
        if (!adds.empty())
        {
            std::vector<ImageData> toInsert;
            toInsert.reserve(adds.size());

            for (auto& [fname, fullpath] : adds)
            {
                // Optional: wait briefly if file is still being copied
                bool ready = false;
                for (int i = 0; i < 10; ++i)
                {
                    std::ifstream f(fullpath, std::ios::binary);
                    if (f.good())
                    {
                        ready = true;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                if (!ready)
                    continue;

                // Use your existing loader (returns ImageData with texture + size)
                ImageData img = LoadTextureFromFile(fullpath.c_str());
                if (img.textureID != 0)
                {
                    img.filename = fname; // store filename (not full path) for matching
                    toInsert.emplace_back(std::move(img));
                }
            }

            if (!toInsert.empty())
            {
                std::unique_lock<std::shared_mutex> lk(imagesMutex);
                for (auto& im : toInsert)
                {
                    // Avoid duplicates
                    auto it = std::find_if(images.begin(), images.end(),
                                           [&](const ImageData& x)
                                           { return x.filename == im.filename; });
                    if (it == images.end())
                        images.emplace_back(std::move(im));
                }
            }
        }
    }

private:
    ImageData selected_image = {0, glm::vec2(), ""}; // Armazena a imagem selecionada
    std::vector<ImageData> images;
    std::thread monitorThread;
    std::atomic<bool> running{true};
    std::atomic<bool> first_scan_done{true};
    std::shared_mutex imagesMutex;
    DirectoryKind kind_;
    float global_size_slider = 1.0f;
    float imageSize = 128.0f;
    std::mutex pendingMtx;
    std::vector<std::string> pendingRemovals; // filenames to remove
    std::vector<std::pair<std::string, std::string>> pendingAddPaths;

    // Function to find an ImageData by filename
    std::vector<ImageData>::iterator findImageByFilename(std::vector<ImageData>& images, const std::string& filename)
    {
        return std::find_if(images.begin(), images.end(), [&filename](const ImageData& image)
                            { return image.filename == filename; });
    }

    void monitorDirectory(const std::string& path)
    {
        using namespace std::chrono_literals;

        // Keep only a simple "known file list" of filenames (no GL/state here)
        std::vector<std::string> knownFiles;

        while (running.load(std::memory_order_relaxed))
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
                std::cerr << "[DirectoryWindow] Filesystem error: " << e.what() << std::endl;
            }

            // Diff: removed = known - current
            for (const auto& name : knownFiles)
            {
                if (std::find(currentFiles.begin(), currentFiles.end(), name) == currentFiles.end())
                {
                    std::lock_guard<std::mutex> qlk(pendingMtx);
                    pendingRemovals.emplace_back(name);
                }
            }

            // Diff: added = current - known
            for (const auto& name : currentFiles)
            {
                if (std::find(knownFiles.begin(), knownFiles.end(), name) == knownFiles.end())
                {
                    const std::string full = (std::filesystem::path(path) / name).string();
                    std::lock_guard<std::mutex> qlk(pendingMtx);
                    pendingAddPaths.emplace_back(name, full);
                }
            }

            knownFiles = std::move(currentFiles);

            // First pass complete → allow generateTextureIDs() to proceed
            first_scan_done = true;

            std::this_thread::sleep_for(2s);
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

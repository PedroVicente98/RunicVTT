#pragma once
#include <filesystem>
#include <string>
#include <iostream>

namespace fs = std::filesystem;

class PathManager {
private:
    fs::path baseDocumentsPath;
    fs::path executableDir;

public:
    PathManager() {
        executableDir = fs::current_path();
        baseDocumentsPath = getDocumentsFolder() / "RunicVTT";
    }

    // --- USER-ACCESSIBLE FOLDERS ---
    fs::path getRootDirectory() const {
        return baseDocumentsPath;
    }

    fs::path getMapsPath() const {
        return baseDocumentsPath / "Maps";
    }

    fs::path getMarkersPath() const {
        return baseDocumentsPath / "Markers";
    }

    fs::path getNotesPath() const {
        return baseDocumentsPath / "Notes";
    }

    fs::path getConfigPath() const {
        return baseDocumentsPath / "Config";
    }

    // --- APP/INSTALLATION-LOCAL FOLDERS ---
    fs::path getExecutableDirectory() const {
        return executableDir;
    }

    fs::path getExecutableRoot() const {
        // If inside /bin, return its parent
        return (executableDir.filename() == "bin") ? executableDir.parent_path() : executableDir;
    }

    fs::path getResPath() const {
        return getExecutableRoot() / "res";
    }

    fs::path getShaderPath() const {
        return getResPath() / "shaders";
    }

    // --- FOLDER INITIALIZATION ---
    void ensureDirectories() const {
        createIfNotExists(getRootDirectory());
        createIfNotExists(getMapsPath());
        createIfNotExists(getMarkersPath());
        createIfNotExists(getNotesPath());
        createIfNotExists(getConfigPath());
    }

private:
    static fs::path getDocumentsFolder() {
#ifdef _WIN32
        const char* userProfile = std::getenv("USERPROFILE");
        return fs::path(userProfile ? userProfile : "C:\\Users\\Default") / "Documents";
#elif __APPLE__
        const char* home = std::getenv("HOME");
        return fs::path(home ? home : "/Users/Default") / "Documents";
#else // Linux
        const char* home = std::getenv("HOME");
        return fs::path(home ? home : "/home/default") / "Documents";
#endif
    }

    static void createIfNotExists(const fs::path& path) {
        if (!fs::exists(path)) {
            fs::create_directories(path);
            std::cout << "[PathManager] Created folder: " << path << std::endl;
        }
        else {
            std::cout << "[PathManager] Found folder: " << path << std::endl;
        }
    }
};

#pragma once
#include <filesystem>
#include <string>
#include <iostream>
#include <cstdlib>

namespace fs = std::filesystem;

class PathManager {
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
       
    }


    inline static fs::path executableDir = fs::current_path();
    inline static fs::path baseDocumentsPath = PathManager::getDocumentsFolder() / "RunicVTT";

public:
    // --- USER-ACCESSIBLE FOLDERS ---
    static fs::path getRootDirectory() {
        return baseDocumentsPath;
    }

    static fs::path getMapsPath() {
        return baseDocumentsPath / "Maps";
    }

    static fs::path getMarkersPath() {
        return baseDocumentsPath / "Markers";
    }

    static fs::path getNotesPath() {
        return baseDocumentsPath / "Notes";
    }

    static fs::path getGameTablesPath() {
        return baseDocumentsPath / "GameTables";
    }

    static fs::path getConfigPath() {
        return baseDocumentsPath / "Config";
    }

    // --- APP/INSTALLATION-LOCAL FOLDERS ---
    static fs::path getUpnpcExePath() {
        return executableDir / "upnpc-static.exe";

    }static fs::path getExecutableDirectory() {
        return executableDir;
    }

    static fs::path getExecutableRoot() {
        return (executableDir.filename() == "bin") ? executableDir.parent_path() : executableDir;
    }

    static fs::path getResPath() {
        return getExecutableRoot() / "res";
    }

    static fs::path getShaderPath() {
        return getResPath() / "shaders";
    }

    // --- FOLDER INITIALIZATION ---
    static void ensureDirectories() {
        createIfNotExists(getRootDirectory());
        createIfNotExists(getMapsPath());
        createIfNotExists(getMarkersPath());
        createIfNotExists(getNotesPath());
        createIfNotExists(getConfigPath());
        createIfNotExists(getGameTablesPath());
    }


};

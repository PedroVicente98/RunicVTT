#pragma once
#include <filesystem>
#include <string>
#include <iostream>
#include <cstdlib>
//#include <shlobj.h>   // for SHGetKnownFolderPath
//#include <ShlObj_core.h>   // or <ShlObj.h>
//#include <KnownFolders.h>

namespace fs = std::filesystem;

class PathManager {
private:

    static fs::path getDocumentsFolder() {
#ifdef _WIN32
        //PWSTR path_w = nullptr;
        //HRESULT result = SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &path_w);
        //if (SUCCEEDED(result)) {
        //    std::wstring wpath(path_w);
        //    CoTaskMemFree(path_w); // free COM-allocated string
        //    return std::filesystem::path(wpath);
        //} else {
        //    throw std::runtime_error("Failed to get Documents folder");
        //}
        //
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
    //C:\Dev\RunicVTT\external\node\node.exe .\node_modules\localtunnel\bin\client -p 7778 -s runics
    static fs::path getExternalPath() {
        return getExecutableRoot() / "external";

    }
    static fs::path getNodeExePath() {
        return getExternalPath() / "node"/ "node.exe";

    }
    static fs::path getLocalTunnelClientPath() {
        return getExternalPath() / "localtunnel" / "node_modules" / "localtunnel" / "bin" / "client";

    }
    static fs::path getUpnpcExePath() {
        return executableDir / "upnpc-static.exe";

    }
    static fs::path getExecutableDirectory() {
        return executableDir;
    }

    static fs::path getExecutableRoot() {
        return (executableDir.filename() == "bin") ? executableDir.parent_path() : executableDir;
    }

    static fs::path getResPath() {
        return getExecutableRoot() / "res";
    }
    
    static fs::path getCertsPath() {
        return getResPath() / "certs";
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

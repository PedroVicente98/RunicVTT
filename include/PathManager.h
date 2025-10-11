#pragma once
#include <filesystem>
#include <string>
#include <iostream>
#include <cstdlib>
#include <cctype>

//#include <shlobj.h>   // for SHGetKnownFolderPath
//#include <ShlObj_core.h>   // or <ShlObj.h>
//#include <KnownFolders.h>

namespace fs = std::filesystem;

class PathManager
{
private:
    static fs::path getDocumentsFolder()
    {
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

    static void createIfNotExists(const fs::path& path)
    {
        if (!fs::exists(path))
        {
            fs::create_directories(path);
            std::cout << "[PathManager] Created folder: " << path << std::endl;
        }
    }

    inline static fs::path executableDir = fs::current_path();
    inline static fs::path baseDocumentsPath = PathManager::getDocumentsFolder() / "RunicVTT";

public:
    // --- USER-ACCESSIBLE FOLDERS ---
    static fs::path getRootDirectory()
    {
        return getExecutableRoot();
        //return baseDocumentsPath;
    }

    static fs::path getMapsPath()
    {
        return getExecutableRoot() / "Maps";
    }

    static fs::path getMarkersPath()
    {
        return getExecutableRoot() / "Markers";
    }

    static fs::path getNotesPath()
    {
        return getExecutableRoot() / "Notes";
    }

    static fs::path getGameTablesPath()
    {
        return getExecutableRoot() / "GameTables";
    }

    static fs::path getConfigPath()
    {
        return getExecutableRoot() / "Config";
    }

    // --- APP/INSTALLATION-LOCAL FOLDERS ---
    //C:\Dev\RunicVTT\external\node\node.exe .\node_modules\localtunnel\bin\client -p 7778 -s runics
    static fs::path getExternalPath()
    {
        return getExecutableRoot() / "external";
    }
    static fs::path getNodeExePath()
    {
        return getExternalPath() / "node" / "node.exe";
    }
    static fs::path getLocalTunnelClientPath()
    {
        return getExternalPath() / "localtunnel" / "node_modules" / "localtunnel" / "bin" / "client";
    }

    static fs::path getLocalTunnelControllerPath()
    {
        return getExternalPath() / "lt-controller.cjs";
    }
    static fs::path getUpnpcExePath()
    {
        return executableDir / "upnpc-static.exe";
    }
    static fs::path getExecutableDirectory()
    {
        return executableDir;
    }

    static fs::path getExecutableRoot()
    {
        return (executableDir.filename() == "bin") ? executableDir.parent_path() : executableDir;
    }

    static fs::path getResPath()
    {
        return getExecutableRoot() / "res";
    }

    static fs::path getCertsPath()
    {
        return getResPath() / "certs";
    }

    static fs::path getShaderPath()
    {
        return getResPath() / "shaders";
    }

    // --- FOLDER INITIALIZATION ---
    static void ensureDirectories()
    {
        createIfNotExists(getRootDirectory());
        createIfNotExists(getMapsPath());
        createIfNotExists(getMarkersPath());
        createIfNotExists(getNotesPath());
        createIfNotExists(getConfigPath());
        createIfNotExists(getGameTablesPath());
    }

    //helpers
    static bool hasDirSep(const std::string& s)
    {
        return s.find('\\') != std::string::npos || s.find('/') != std::string::npos;
    }

    static bool isUNC(const std::string& s)
    {
        // \\server\share or //server/share
        return (s.rfind("\\\\", 0) == 0) || (s.rfind("//", 0) == 0);
    }

    static bool isDriveAbsolute(const std::string& s)
    {
        // C:\foo or C:/foo
        return s.size() > 2 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':' && (s[2] == '\\' || s[2] == '/');
    }

    static bool isDriveRelative(const std::string& s)
    {
        // C:foo (relative to current dir on drive C)
        return s.size() > 1 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':' && (s.size() == 2 || (s[2] != '\\' && s[2] != '/'));
    }

    static bool isAbsolutePath(const std::string& s)
    {
        // std::filesystem covers POSIX abspaths; supplement for Windows special cases
        std::filesystem::path p(s);
        if (p.is_absolute())
            return true; // works for POSIX and many Windows cases
        if (isUNC(s) || isDriveAbsolute(s))
            return true;
        return false;
    }

    static bool isPathLike(const std::string& s)
    {
        // Treat anything with directory separators OR absolute/UNC/drive-relative prefixes as a path
        if (isAbsolutePath(s))
            return true;
        if (isDriveRelative(s))
            return true; // still a path, just not absolute
        if (hasDirSep(s))
            return true; // e.g. "markers/goblin.png"
        return false;
    }

    static bool isFilenameOnly(const std::string& s)
    {
        // No separators, no drive/UNC prefixes => just a filename, like "goblin.png"
        return !isPathLike(s);
    }
};

#pragma once
#include <iostream>
#include <string>
#include <array>
#include <memory>
#include "PathManager.h"
//#define WIN32_LEAN_AND_MEAN
//#include <winsock2.h>
//#include <ws2tcpip.h>
//#include <windows.h> // depois de winsock2.h
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <WinSock2.h> // must be BEFORE windows.h
#include <WS2tcpip.h>
#include <Windows.h>

#include <winhttp.h>

#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdlib> // for _putenv_s

class NetworkUtilities
{
public:
    static std::string runCommand(const std::string& cmd)
    {
        std::array<char, 128> buffer;
        std::string result;
        std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), _pclose);
        if (!pipe)
            throw std::runtime_error("popen() failed!");
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        {
            result += buffer.data();
        }
        return result;
    }

    static std::string httpGet(const std::wstring& host, const std::wstring& path)
    {
        std::string result;

        HINTERNET hSession = WinHttpOpen(L"RunicVTT/1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);

        if (hSession)
        {
            HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
                                                INTERNET_DEFAULT_HTTPS_PORT, 0);

            if (hConnect)
            {
                HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
                                                        NULL, WINHTTP_NO_REFERER,
                                                        WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                        WINHTTP_FLAG_SECURE);

                if (hRequest)
                {
                    BOOL bResults = WinHttpSendRequest(hRequest,
                                                       WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                                       WINHTTP_NO_REQUEST_DATA, 0,
                                                       0, 0);

                    if (bResults)
                        bResults = WinHttpReceiveResponse(hRequest, NULL);

                    if (bResults)
                    {
                        DWORD dwSize = 0;
                        do
                        {
                            DWORD dwDownloaded = 0;
                            if (!WinHttpQueryDataAvailable(hRequest, &dwSize))
                                break;

                            if (!dwSize)
                                break;

                            std::string buffer(dwSize, 0);
                            if (!WinHttpReadData(hRequest, buffer.data(), dwSize, &dwDownloaded))
                                break;

                            result.append(buffer.c_str(), dwDownloaded);

                        } while (dwSize > 0);
                    }
                    WinHttpCloseHandle(hRequest);
                }
                WinHttpCloseHandle(hConnect);
            }
            WinHttpCloseHandle(hSession);
        }
        return result;
    }

    static std::string getLocalIPv4Address()
    {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        {
            throw std::runtime_error("WSAStartup failed");
        }

        // Create a UDP socket (does not actually send packets)
        SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (sock == INVALID_SOCKET)
        {
            WSACleanup();
            throw std::runtime_error("socket creation failed");
        }

        sockaddr_in remote{};
        remote.sin_family = AF_INET;
        remote.sin_port = htons(53);                     // DNS port
        inet_pton(AF_INET, "8.8.8.8", &remote.sin_addr); // Google DNS

        // Connect sets the default route for this socket
        if (connect(sock, reinterpret_cast<sockaddr*>(&remote), sizeof(remote)) != 0)
        {
            closesocket(sock);
            WSACleanup();
            throw std::runtime_error("connect failed");
        }

        sockaddr_in local{};
        int len = sizeof(local);
        if (getsockname(sock, reinterpret_cast<sockaddr*>(&local), &len) != 0)
        {
            closesocket(sock);
            WSACleanup();
            throw std::runtime_error("getsockname failed");
        }

        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &local.sin_addr, ipStr, sizeof(ipStr));

        closesocket(sock);
        WSACleanup();

        return std::string(ipStr);
    }

    // helper: normalize URL for libdatachannel
    static std::string normalizeWsUrl(const std::string& hostOrUrl, unsigned short port)
    {
        auto starts = [](const std::string& s, const char* p)
        { return s.rfind(p, 0) == 0; };

        if (starts(hostOrUrl, "wss://") || starts(hostOrUrl, "ws://"))
            return hostOrUrl;
        if (starts(hostOrUrl, "https://"))
        {
            auto u = hostOrUrl;
            u.replace(0, 8, "wss://");
            return u;
        }
        if (starts(hostOrUrl, "http://"))
        {
            auto u = hostOrUrl;
            u.replace(0, 7, "ws://");
            return u;
        }

        // No scheme given:
        if (hostOrUrl.find(".loca.lt") != std::string::npos)
        {
            // LocalTunnel public endpoint â†’ secure, no port
            return "wss://" + hostOrUrl;
        }

        // LAN / plain host â†’ need explicit port
        return "ws://" + hostOrUrl + ":" + std::to_string(port);
    }

    static void setupTLS()
    {
        auto caPath = PathManager::getCertsPath() / "cacert.pem";
        auto caPathString = caPath.string();
        if (_putenv_s("SSL_CERT_FILE", caPathString.c_str()) != 0)
        {
            std::cerr << "[NetworkUtilities] Failed to set SSL_CERT_FILE\n";
        }
        else
        {
            std::cout << "[NetworkUtilities] SSL_CERT_FILE set to " << caPath << "\n";
        }
    }

    static std::string normalizeWsUrl(const std::string& raw)
    {
        auto starts = [&](const char* p)
        { return raw.rfind(p, 0) == 0; };
        if (starts("https://"))
            return "wss://" + raw.substr(8);
        if (starts("http://"))
            return "ws://" + raw.substr(7);
        // if already ws:// or wss:// or a bare host (leave as caller wishes)
        return raw;
    }

    // Start a local tunnel, returns the subdomain used
    //static std::string startLocalTunnel(const std::string& subdomainBase, int port)
    //{
    //    stopLocalTunnel(); // Stop any previous tunnel

    //    // Clean the subdomain (remove dots, lowercase)
    //    std::string subdomain = std::regex_replace(subdomainBase, std::regex("\\."), "");
    //    for (auto& c : subdomain)
    //        c = std::tolower(c);

    //    // Reset URL before starting
    //    {
    //        std::lock_guard<std::mutex> lk(urlMutex);
    //        localTunnelUrl.clear();
    //    }

    //    running = true;
    //    tunnelThread = std::thread([subdomain, port]()
    //                               {
    //                                   std::string nodePath = PathManager::getNodeExePath().string();
    //                                   std::string ltPath = PathManager::getLocalTunnelClientPath().string();
    //                                   std::ostringstream cmd;
    //                                   cmd << nodePath + " " + ltPath + " --port " + std::to_string(port) + " --subdomain " + subdomain;
    //                                   std::cout << "Starting LocalTunnel: " << cmd.str() << std::endl;
    //                                   std::array<char, 128> buffer;
    //                                   tunnelProcess = _popen(cmd.str().c_str(), "r");
    //                                   if (!tunnelProcess)
    //                                   {
    //                                       std::cerr << "Failed to start local tunnel process!" << std::endl;
    //                                       running = false;
    //                                       urlCv.notify_all(); // wake waiters
    //                                       return;
    //                                   }

    //                                   std::string output;
    //                                   while (fgets(buffer.data(), buffer.size(), tunnelProcess) != nullptr && running)
    //                                   {
    //                                       output += buffer.data();
    //                                       // Capture the URL once
    //                                       auto pos = output.find("https://");
    //                                       if (pos != std::string::npos)
    //                                       {
    //                                           auto end = output.find("\n", pos);
    //                                           std::string found = output.substr(pos, (end == std::string::npos) ? std::string::npos : end - pos);
    //                                           {
    //                                               std::lock_guard<std::mutex> lk(urlMutex);
    //                                               if (localTunnelUrl.empty())
    //                                               {
    //                                                   localTunnelUrl = std::move(found);
    //                                                   std::cout << "LocalTunnel URL: " << localTunnelUrl << std::endl;
    //                                               }
    //                                           }
    //                                           urlCv.notify_all(); // wake waiters
    //                                       }
    //                                   }

    //                                   _pclose(tunnelProcess);
    //                                   tunnelProcess = nullptr;
    //                                   running = false;
    //                                   urlCv.notify_all(); // wake waiters even if no URL found
    //                               });

    //    // Wait up to 15s for URL to be set (or process to end)
    //    std::unique_lock<std::mutex> lk(urlMutex);
    //    bool ok = urlCv.wait_for(lk, std::chrono::seconds(15), []
    //                             { return !localTunnelUrl.empty() || !running.load(); });

    //    // Return the URL (copy). If not available, return empty string.
    //    return ok ? localTunnelUrl : std::string{};
    //}

    //static void stopLocalTunnel()
    //{
    //    if (running)
    //    {
    //        running = false;
    //        if (tunnelProcess)
    //        {
    //            _pclose(tunnelProcess);
    //            tunnelProcess = nullptr;
    //        }

    //        if (tunnelThread.joinable())
    //        {
    //            tunnelThread.join();
    //        }

    //        localTunnelUrl.clear();
    //    }
    //}

    //static std::string getLocalTunnelUrl()
    //{
    //    return localTunnelUrl;
    //}

private:
    //inline static std::thread tunnelThread;
    //inline static FILE* tunnelProcess{nullptr};

    inline static std::atomic<bool> running{false};
    inline static std::string localTunnelUrl;
    inline static std::mutex urlMutex;
    inline static std::condition_variable urlCv;

// ================== LocalTunnel (Windows-only, header-only) ==================
private:
    // Process & pipes
    inline static HANDLE ltProc = nullptr;
    inline static HANDLE ltThread = nullptr;

    inline static HANDLE ltStdoutRd = nullptr; // parent read side (child stdout/stderr)
    inline static HANDLE ltStdoutWr = nullptr; // child write side

    inline static HANDLE ltStdinRd = nullptr; // child read side (stdin)
    inline static HANDLE ltStdinWr = nullptr; // parent write side

    inline static HANDLE ltJob = nullptr; // job to kill process tree

    // reuse your existing:
    // inline static std::atomic<bool> running{false};
    // inline static std::string localTunnelUrl;
    // inline static std::mutex urlMutex;
    // inline static std::condition_variable urlCv;

    static void closeHandleIf(HANDLE& h)
    {
        if (h)
        {
            CloseHandle(h);
            h = nullptr;
        }
    }

    static void createKillJobIfNeeded()
    {
        if (ltJob)
            return;
        ltJob = CreateJobObjectA(nullptr, nullptr);
        if (!ltJob)
            return;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(ltJob, JobObjectExtendedLimitInformation, &info, sizeof(info));
    }
    static void assignToJob(HANDLE process)
    {
        createKillJobIfNeeded();
        if (ltJob)
            AssignProcessToJobObject(ltJob, process);
    }

    static std::string lastErrorToString(DWORD err)
    {
        LPVOID msg = nullptr;
        DWORD n = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg, 0, nullptr);
        std::string s;
        if (n && msg)
        {
            s.assign((LPSTR)msg, n);
            LocalFree(msg);
        }
        return s;
    }

public:
    static std::string startLocalTunnel(const std::string& subdomainBase, int port)
    {
        stopLocalTunnel(); // ensure clean start

        // Normalize subdomain: remove dots, lowercase
        std::string subdomain = std::regex_replace(subdomainBase, std::regex("\\."), "");
        for (auto& c : subdomain)
            c = (char)std::tolower((unsigned char)c);

        {
            std::lock_guard<std::mutex> lk(urlMutex);
            localTunnelUrl.clear();
        }

        // Create stdout pipe
        SECURITY_ATTRIBUTES sa{};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        if (!CreatePipe(&ltStdoutRd, &ltStdoutWr, &sa, 0))
        {
            OutputDebugStringA("[LT] CreatePipe stdout failed\n");
            return {};
        }
        SetHandleInformation(ltStdoutRd, HANDLE_FLAG_INHERIT, 0); // parent read end: not inheritable

        // Create stdin pipe
        if (!CreatePipe(&ltStdinRd, &ltStdinWr, &sa, 0))
        {
            OutputDebugStringA("[LT] CreatePipe stdin failed\n");
            closeHandleIf(ltStdoutWr);
            closeHandleIf(ltStdoutRd);
            return {};
        }
        SetHandleInformation(ltStdinWr, HANDLE_FLAG_INHERIT, 0); // parent write end: not inheritable

        // Build command: "node.exe" "lt-controller.js" --port N --subdomain X
        std::string nodePath = PathManager::getNodeExePath().string();
        std::string ctlPath = PathManager::getLocalTunnelControllerPath().string();

        std::ostringstream cmd;
        cmd << "\"" << nodePath << "\" \"" << ctlPath << "\""
            << " --port " << port
            << " --subdomain " << subdomain;

        STARTUPINFOA si{};
        PROCESS_INFORMATION pi{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
        si.hStdInput = ltStdinRd;
        si.hStdOutput = ltStdoutWr;
        si.hStdError = ltStdoutWr;
        si.wShowWindow = SW_HIDE;

        DWORD flags = CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP;

        std::string cmdLine = cmd.str();
        std::vector<char> mutableCmd(cmdLine.begin(), cmdLine.end());
        mutableCmd.push_back('\0');

        // Optionally set working dir so require('localtunnel') resolves next to lt-controller.js
        std::string workingDir = PathManager::getLocalTunnelControllerPath().parent_path().string();

        BOOL ok = CreateProcessA(
            /*appName*/ nullptr,
            /*cmdLine*/ mutableCmd.data(),
            nullptr, nullptr,
            /*inherit handles*/ TRUE, // child inherits ltStdoutWr, ltStdinRd
            flags,
            /*env*/ nullptr,
            /*current dir*/ workingDir.empty() ? nullptr : workingDir.c_str(),
            &si, &pi);

        // Parent no longer needs these ends
        closeHandleIf(ltStdoutWr);
        closeHandleIf(ltStdinRd);

        if (!ok)
        {
            DWORD err = GetLastError();
            std::string em = "[LT] CreateProcess failed: " + lastErrorToString(err);
            OutputDebugStringA(em.c_str());
            closeHandleIf(ltStdoutRd);
            closeHandleIf(ltStdinWr);
            return {};
        }

        ltProc = pi.hProcess;
        ltThread = pi.hThread;
        running.store(true, std::memory_order_relaxed);

        assignToJob(ltProc);

        // Reader thread: read one JSON line with {"event":"ready","url":...}
        std::thread([rd = ltStdoutRd]()
                    {
            std::string buf; buf.reserve(4096);
            constexpr DWORD CHUNK = 1024;
            char tmp[CHUNK + 1];

            auto parseLine = [](const std::string& line) -> std::string {
                // very tiny JSON extraction: look for "event":"ready" + extract url
                if (line.find("\"event\":\"ready\"") == std::string::npos) return {};
                auto u = line.find("\"url\"");
                if (u == std::string::npos) return {};
                u = line.find('"', u + 4); // first quote after "url"
                if (u == std::string::npos) return {};
                auto v = line.find('"', u + 1);
                if (v == std::string::npos) return {};
                return line.substr(u + 1, v - (u + 1));
            };

            while (NetworkUtilities::running.load(std::memory_order_relaxed)) {
                DWORD got = 0;
                BOOL ok = ReadFile(rd, tmp, CHUNK, &got, nullptr);
                if (!ok || got == 0) break;
                tmp[got] = '\0';
                buf.append(tmp, got);

                // process complete lines
                size_t pos = 0;
                while (true) {
                    size_t nl = buf.find('\n', pos);
                    if (nl == std::string::npos) break;
                    std::string line = buf.substr(pos, nl - pos);
                    pos = nl + 1;

                    if (auto url = parseLine(line); !url.empty()) {
                        {
                            std::lock_guard<std::mutex> lk(NetworkUtilities::urlMutex);
                            if (NetworkUtilities::localTunnelUrl.empty())
                                NetworkUtilities::localTunnelUrl = std::move(url);
                        }
                        NetworkUtilities::urlCv.notify_all();
                    }
                }
                if (pos > 0) buf.erase(0, pos);
            }
            NetworkUtilities::urlCv.notify_all(); })
            .detach();

        // Wait up to 15s for URL or process exit
        std::unique_lock<std::mutex> lk(urlMutex);
        bool have = urlCv.wait_for(lk, std::chrono::seconds(15), []
                                   { return !localTunnelUrl.empty() || !running.load(std::memory_order_relaxed); });

        return have ? localTunnelUrl : std::string{};
    }

    static void stopLocalTunnel()
    {
        // tell reader thread to stop
        running.store(false, std::memory_order_relaxed);

        // try graceful: send "stop\n" if stdin is open
        if (ltStdinWr)
        {
            DWORD written = 0;
            const char* stopCmd = "stop\n";
            WriteFile(ltStdinWr, stopCmd, (DWORD)strlen(stopCmd), &written, nullptr);
        }

        // wait a moment for clean exit
        if (ltProc)
        {
            DWORD wait = WaitForSingleObject(ltProc, 300); // 300ms grace
            if (wait != WAIT_OBJECT_0)
            {
                // force kill now (non-blocking)
                TerminateProcess(ltProc, 0);
            }
        }

        // close all handles
        closeHandleIf(ltThread);
        closeHandleIf(ltProc);
        closeHandleIf(ltStdoutWr); // usually already closed
        closeHandleIf(ltStdoutRd);
        closeHandleIf(ltStdinRd); // already closed
        closeHandleIf(ltStdinWr);

        // close job (kills any remaining children)
        if (ltJob)
        {
            CloseHandle(ltJob);
            ltJob = nullptr;
        }

        // clear URL and wake any waiters
        {
            std::lock_guard<std::mutex> lk(urlMutex);
            localTunnelUrl.clear();
        }
        urlCv.notify_all();
    }

    static std::string getLocalTunnelUrl()
    {
        std::lock_guard<std::mutex> lk(urlMutex);
        return localTunnelUrl;
    }
    // ================== /LocalTunnel ==================


};

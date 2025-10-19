// FirewallUtils.h
#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#include <string>
#include <sstream>

namespace FirewallUtils
{

    inline std::string psEscape(const std::string& s)
    {
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s)
            out += (c == '\'') ? "''" : std::string(1, c);
        return out;
    }
    inline bool runPSElevatedWait(const std::string& psCommand)
    {
        // Build the PowerShell arguments once, keep the std::string alive
        std::ostringstream oss;
        oss << "-NoProfile -ExecutionPolicy Bypass -Command \"" << psCommand << "\"";
        const std::string argStr = oss.str(); // KEEP this alive for the whole function

        // Convert to UTF-16 safely
        auto toWide = [](const std::string& s) -> std::wstring
        {
            if (s.empty())
                return std::wstring();
            int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
            std::wstring ws(needed, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), ws.data(), needed);
            return ws;
        };

        const std::wstring wExe = L"powershell.exe";
        const std::wstring wArgs = toWide(argStr);

        SHELLEXECUTEINFOW sei{};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.hwnd = nullptr;
        sei.lpVerb = L"runas"; // UAC prompt
        sei.lpFile = wExe.c_str();
        sei.lpParameters = wArgs.c_str();
        sei.nShow = SW_HIDE;

        if (!ShellExecuteExW(&sei))
        {
            // Optional: GetLastError() logging here
            return false;
        }
        if (sei.hProcess)
        {
            WaitForSingleObject(sei.hProcess, INFINITE);
            CloseHandle(sei.hProcess);
        }
        return true;
    }

    inline void removeRuleElevated(const std::string& displayName)
    {
        std::ostringstream ps;
        ps << "$n='" << psEscape(displayName) << "'; "
           << "Get-NetFirewallRule -DisplayName $n -ErrorAction SilentlyContinue | "
           << "Remove-NetFirewallRule -Confirm:$false";
        runPSElevatedWait(ps.str());
    }

    // Inbound ANY TCP for a specific program (exe), on Private by default
    inline void addInboundAnyTcpForExe(const std::string& displayName,
                                       const std::string& programPath,
                                       bool privateOnly = true)
    {
        std::ostringstream ps;
        ps << "New-NetFirewallRule "
           << "-DisplayName '" << psEscape(displayName) << "' "
           << "-Direction Inbound -Action Allow -Protocol TCP -LocalPort Any "
           << "-Program '" << psEscape(programPath) << "' "
           << "-Profile " << (privateOnly ? "Private" : "Any");
        runPSElevatedWait(ps.str());
    }

    // Inbound ANY UDP for a specific program (optional, for WebRTC/data)
    inline void addInboundAnyUdpForExe(const std::string& displayName,
                                       const std::string& programPath,
                                       bool privateOnly = true)
    {
        std::ostringstream ps;
        ps << "New-NetFirewallRule "
           << "-DisplayName '" << psEscape(displayName) << "' "
           << "-Direction Inbound -Action Allow -Protocol UDP "
           << "-Program '" << psEscape(programPath) << "' "
           << "-Profile " << (privateOnly ? "Private" : "Any");
        runPSElevatedWait(ps.str());
    }

} // namespace FirewallUtils

#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <cstdio>    // For _wsystem
#include <sstream>   // For std::ostringstream
#include <windows.h> // For WideCharToMultiByte, MultiByteToWideChar if needed, and for ERROR_SUCCESS
#include <algorithm> // For std::transform
#include <cctype>    // For ::tolower
#include <iostream>  // For error logging
#include "PathManager.h"
#include <array>    // For std::array
#include <regex>    // For std::regex, std::smatch, std::regex_search


class UPnPManager {
public:

    static std::string getExternalIPv4Address() {
        return std::string("NOT IMPLEMENTED YET!!");
    }
    // Static public method to get the primary local IPv4 address using PowerShell
    static std::string getLocalIPv4Address() {
        std::string ipAddress = "";
        std::filesystem::path scriptPath = PathManager::getResPath() / "GetCorrectIPv4.ps1";
        std::string command = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" + scriptPath.string() + "\"";
        
        FILE* pipe = _popen(command.c_str(), "r");
        if (!pipe) {
            std::cerr << "Erro: Falha ao executar o script PowerShell. Comando: " << command << std::endl;
            return ipAddress; // Retorna string vazia em caso de falha ao iniciar o processo
        }

        std::array<char, 256> buffer; // Buffer para ler a linha
        std::string line;

        if (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
            line = buffer.data();
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            ipAddress = line;
        }

        _pclose(pipe);
        std::regex ipv4_format_regex("^\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}$");
        if (!std::regex_match(ipAddress, ipv4_format_regex)) {
            std::cerr << "Aviso: O script PowerShell retornou algo que não parece um IPv4 válido ou nada: '" << ipAddress << "'" << std::endl;
            return ""; // Retorna string vazia se o formato não for válido ou se nenhum IP foi encontrado
        }

        return ipAddress;
    }


    // Add a port mapping
    static bool addPortMapping(
        const std::string& internalIp,
        unsigned short internalPort,
        unsigned short externalPort,
        const std::string& protocol, // "TCP" or "UDP"
        const std::string& description = "",
        unsigned int duration = 0 // 0 for indefinite
    ) {
        std::wostringstream cmd;
        // Use your PathManager to get the path
        cmd << L"\"" << PathManager::getUpnpcExePath().wstring() << L"\" -a " // .wstring() for std::wostringstream
            << stringToWString(internalIp) << L" "
            << internalPort << L" "
            << externalPort << L" "
            << stringToWString(toUpper(protocol)); // Ensure protocol is uppercase

        if (!description.empty()) {
            cmd << L" \"" << stringToWString(description) << L"\"";
        }
        else {
            cmd << L" \"\""; // Pass empty string if no description for consistency
        }
        cmd << L" " << duration;

        std::wcout << L"Executing UPNP command: " << cmd.str() << std::endl; // For debugging
        int result = _wsystem(cmd.str().c_str());

        if (result != 0) {
            std::wcerr << L"UPNP addPortMapping command failed. _wsystem returned: " << result << std::endl;
        }
        return result == 0;
    }

    // Remove a port mapping
    static bool removePortMapping(
        unsigned short externalPort,
        const std::string& protocol // "TCP" or "UDP"
    ) {
        std::wostringstream cmd;
        // Use your PathManager to get the path
        cmd << L"\"" << PathManager::getUpnpcExePath().wstring() << L"\" -d "
            << externalPort << L" "
            << stringToWString(toUpper(protocol)); // Ensure protocol is uppercase

        std::wcout << L"Executing UPNP command: " << cmd.str() << std::endl; // For debugging
        int result = _wsystem(cmd.str().c_str());

        if (result != 0) {
            std::wcerr << L"UPNP removePortMapping command failed. _wsystem returned: " << result << std::endl;
        }
        return result == 0;
    }

private:
    // Helper to convert std::string to std::wstring for _wsystem
    static std::wstring stringToWString(const std::string& s) {
        int len;
        // +1 for null terminator, assuming s.length() is byte length.
        // For UTF-8, this is safer, but if you're dealing with raw bytes, length might be different.
        // For general usage with ASCII/Latin-1 strings in arguments, this is fine.
        int slength = (int)s.length() + 1;
        len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
        std::vector<wchar_t> buf(len);
        MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, &buf[0], len);
        return std::wstring(&buf[0]);
    }

    // Helper to convert string to uppercase
    static std::string toUpper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return static_cast<unsigned char>(std::toupper(c)); });
        return s;
    }
};
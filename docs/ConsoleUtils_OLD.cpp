#include "ConsoleUtils.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <cstdio>
#include <iostream>

static bool g_consoleOpen = false;

static void BindStdToConsole()
{
    FILE* f = nullptr;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
    freopen_s(&f, "CONIN$", "r", stdin);
    std::ios::sync_with_stdio(true);
    std::wcout.clear();
    std::cout.clear();
    std::wcerr.clear();
    std::cerr.clear();
    std::wcin.clear();
    std::cin.clear();
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

static void BindStdToNull()
{
    FILE* f = nullptr;
    freopen_s(&f, "NUL", "w", stdout);
    freopen_s(&f, "NUL", "w", stderr);
    freopen_s(&f, "NUL", "r", stdin);
    std::ios::sync_with_stdio(true);
    std::wcout.clear();
    std::cout.clear();
    std::wcerr.clear();
    std::cerr.clear();
    std::wcin.clear();
    std::cin.clear();
}

static void DisableConsoleCloseButton()
{
    if (HWND hwnd = GetConsoleWindow())
    {
        if (HMENU hMenu = GetSystemMenu(hwnd, FALSE))
        {
            // Gray + remove Close; prevents CTRL_CLOSE_EVENT via titlebar X
            EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
            DeleteMenu(hMenu, SC_CLOSE, MF_BYCOMMAND);
        }
    }
}

bool ConsoleUtils::OpenConsole()
{
    if (g_consoleOpen && GetConsoleWindow())
        return true;
    if (!AllocConsole())
        return false;
    BindStdToConsole();
    DisableConsoleCloseButton();
    g_consoleOpen = true;
    std::cout << "Console opened.\n";
    return true;
}

void ConsoleUtils::CloseConsole()
{
    if (!g_consoleOpen)
        return;
    std::cout << "Console closing...\n";
    std::cout.flush();

    // Redirect stdio away from the soon-to-be-gone console
    BindStdToNull();

    // Detach console
    FreeConsole();
    g_consoleOpen = false;
}

bool ConsoleUtils::IsConsoleOpen()
{
    // Trust the real window handle (robust across odd states)
    return GetConsoleWindow() != nullptr;
}

void ConsoleUtils::DebugPrint(const std::string& msg)
{
    OutputDebugStringA(msg.c_str());
}

#endif

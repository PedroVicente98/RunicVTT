#pragma once

#include <string>

namespace ConsoleUtils
{
    // Open a console for the current process. If launched from a terminal,
    // we attach to the parent console; otherwise we allocate a new one.
    // Returns true if a console is available after this call.
    bool OpenConsole();

    // Close/detach the console (if we allocated/attached one).
    void CloseConsole();

    // Is a console currently attached?
    bool IsConsoleOpen();

    // Optional: write a line to debugger output (shows in VS Output window)
    void DebugPrint(const std::string& msg);
} // namespace ConsoleUtils

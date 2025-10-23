// UiTypingGuard.h
#pragma once
#include <imgui.h>

namespace UiTypingGuard
{
    // single flag for your whole app
    inline bool g_isTyping = false;

    // call this *right after* each InputText you care about
    inline void TrackThisInput()
    {
        // true while the last item (the input) is active
        if (ImGui::IsItemActive())
            g_isTyping = true;
        else if (!ImGui::IsAnyItemActive())
            g_isTyping = false; // automatic clear when nothing is active
    }

    inline bool IsTyping()
    {
        return g_isTyping;
    }

    // optional: hard reset per-frame before your UI begins
    inline void FrameReset()
    {
        if (!ImGui::IsAnyItemActive())
            g_isTyping = false;
    }
} // namespace UiTypingGuard

#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>

#include "imgui.h"
#include "DebugConsole.h"
#include "Logger.h"
#include "ImGuiToaster.h"

namespace DebugActions
{

    // ---------- Small helpers ----------------------------------------------------
    using Clock = std::chrono::steady_clock;

    inline bool timeElapsedMs(Clock::time_point& last, int ms)
    {
        auto now = Clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() >= ms)
        {
            last = now;
            return true;
        }
        return false;
    }

    inline bool keyDownImGui(ImGuiKey imguiKey)
    {
        // If you use ImGui backend keys:
        return ImGui::IsKeyDown(imguiKey);
    }

    // ---------- Global master switch (optional mirror of DebugConsole master) ----
    inline bool gMasterEnabled = false;

    // ---------- Debug: Toaster Notification Demo ---------------------------------
    inline bool gEnableToasterTest = false;
    inline Clock::time_point gToasterLast = Clock::now();

    inline void ToasterChanged(std::weak_ptr<ImGuiToaster> toaster_, bool on)
    {
        Logger::instance().log("main", std::string("Toaster Notification Test ") + (on ? "ENABLED" : "DISABLED"));
        if (auto t = toaster_.lock())
        {
            if (on)
            {
                t->Push(ImGuiToaster::Level::Info, "Toaster Debug ON!!", 1.0f);
            }
            else
            {
                t->Push(ImGuiToaster::Level::Info, "Toaster Debug OFF!!", 1.0f);
            }
        }
    }

    inline void ToasterTick(std::weak_ptr<ImGuiToaster> toaster_)
    {
        if (auto t = toaster_.lock())
        {
            if (ImGui::IsKeyPressed(ImGuiKey_1) && timeElapsedMs(gToasterLast, 200))
            {
                t->Push(ImGuiToaster::Level::Info, "Info: Hello!", 5.0f);
                Logger::instance().log("main", Logger::Level::Info, "[toast] Info Demo");
            }
            if (ImGui::IsKeyPressed(ImGuiKey_2) && timeElapsedMs(gToasterLast, 200))
            {
                t->Push(ImGuiToaster::Level::Good, "Good: Saved", 4.0f);
                Logger::instance().log("main", Logger::Level::Success, "[toast] Good Demo");
            }
            if (ImGui::IsKeyPressed(ImGuiKey_3) && timeElapsedMs(gToasterLast, 200))
            {
                t->Push(ImGuiToaster::Level::Warning, "Warning: Ping high", 6.0f);
                Logger::instance().log("main", Logger::Level::Warn, "[toast] Warning Demo");
            }
            if (ImGui::IsKeyPressed(ImGuiKey_4) && timeElapsedMs(gToasterLast, 200))
            {
                t->Push(ImGuiToaster::Level::Error, "Error: Failed op");
                Logger::instance().log("main", Logger::Level::Error, "[toast] Error Demo");
            }
        }
    }

    // ---------- Debug: Mouse Circle Overlay --------------------------------------
    inline bool gEnableMouseCircle = false;
    inline void MouseCircleChanged(bool on)
    {
        Logger::instance().log("main", std::string("Mouse Circle ") + (on ? "ENABLED" : "DISABLED"));
    }
    inline void MouseCircleTick()
    {
        // Draw a simple circle at mouse (using ImGui draw list for example)
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 p = ImGui::GetMousePos();
        float radius = 14.f;
        ImU32 colFill = IM_COL32(255, 255, 0, 48);
        ImU32 colLine = IM_COL32(255, 255, 0, 200);
        dl->AddCircleFilled(p, radius, colFill, 24);
        dl->AddCircle(p, radius, colLine, 24, 2.0f);
    }

    // ---------- Debug: FPS Overlay (example) -------------------------------------
    inline bool gEnableFpsOverlay = false;
    inline void FpsOverlayChanged(bool on)
    {
        Logger::instance().log("main", std::string("FPS Overlay ") + (on ? "ENABLED" : "DISABLED"));
    }
    inline void FpsOverlayTick()
    {
        // Small FPS text in top-left corner
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 pos = ImVec2(12, 12);
        char buf[64];
        // Use ImGui::GetIO().Framerate or your timing system
        snprintf(buf, sizeof(buf), "FPS: %.1f", ImGui::GetIO().Framerate);
        dl->AddText(pos, IM_COL32(255, 255, 255, 220), buf);
    }

    // ---------- Debug: Network Ping Log (example) --------------------------------
    inline bool gEnablePingLog = false;
    inline Clock::time_point gPingLast = Clock::now();
    inline void PingLogChanged(bool on)
    {
        Logger::instance().log("main", std::string("Ping Log ") + (on ? "ENABLED" : "DISABLED"));
    }
    inline void PingLogTick()
    {
        // Throttled periodic log
        if (timeElapsedMs(gPingLast, 1000))
        {
            // Replace with a real metric if you have one
            Logger::instance().log("main", "[net] heartbeat");
        }
    }

    // ---------- Registration helpers ----------------------------------------------
    inline void RegisterToasterToggles(std::weak_ptr<ImGuiToaster> toaster_)
    {
        DebugConsole::addToggle({
            "Toaster Notifications (1–4)",
            &gEnableToasterTest,
            /* onChanged */ [t = toaster_](bool on) { // capture the shared_ptr by value
                ToasterChanged(t, on);                // call your function WITH the arg
            },
            /* onTick */ [t = toaster_]() { // capture the shared_ptr by value
                ToasterTick(t);             // call your function WITH the arg
            },
        });
    }

    inline void RegisterAllDefaultToggles()
    {
        // Helper shim to avoid direct include dependency in this header:
        auto add = [](const char* label, bool* f, std::function<void(bool)> oc, std::function<void()> ot)
        {
            auto toggle = DebugToggle{
                label,
                f,
                oc,
                ot};

            DebugConsole::addToggle(toggle);
        };

        add("Debug Mouse Circle", &gEnableMouseCircle, MouseCircleChanged, MouseCircleTick);

        add("FPS Overlay", &gEnableFpsOverlay, FpsOverlayChanged, FpsOverlayTick);

        add("Network Ping Log", &gEnablePingLog, PingLogChanged, PingLogTick);
    }

} // namespace DebugActions

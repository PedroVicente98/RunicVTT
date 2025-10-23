#pragma once
#include "imgui.h"
#include <string>
#include <vector>
#include <functional>
#include "Logger.h"

// A simple struct to register toggleable debug features.
// DebugConsole.h (top)
struct DebugToggle
{
    std::string label;
    bool* flagPtr = nullptr;                     // the checkbox controls this flag
    std::function<void(bool enabled)> onChanged; // optional: run once on toggle change
    std::function<void()> onTick;                // optional: run every frame while enabled
};

class DebugConsole
{
public:
    // Provide LT start/stop handlers (button actions on the left panel).
    // Start returns the LT URL (or ""). Stop is void.
    static void setLocalTunnelHandlers(std::function<std::string()> startHandler, std::function<void()> stopHandler)
    {
        ltStart_ = std::move(startHandler);
        ltStop_ = std::move(stopHandler);
        Logger::instance().log("localtunnel", Logger::Level::Success, "Start and Stop Handlers Set!!!");
    }

    static void setIdentityLogger(std::function<std::string()> identityLogger)
    {
        identityLogger_ = std::move(identityLogger);
    }

    // Add/remove toggle entries for the left panel.
    static void addToggle(const DebugToggle& t)
    {
        toggles_.push_back(t);
    }
    static void clearToggles()
    {
        toggles_.clear();
    }

    static void RunActiveDebugToggles()
    {
        if (!debugExecEnabled_)
            return;
        for (auto& t : toggles_)
        {
            if (t.flagPtr && *t.flagPtr && t.onTick)
            {
                t.onTick(); // execute the active debug behavior for this frame
            }
        }
    }

    static void setDebugExecEnabled(bool v)
    {
        debugExecEnabled_ = v;
    }
    static bool isDebugExecEnabled()
    {
        return debugExecEnabled_;
    }

    // Console window visibility
    static void setVisible(bool v)
    {
        visible_ = v;
    }
    static bool isVisible()
    {
        return visible_;
    }

    // Active channel selection
    static void setActiveChannel(const std::string& ch)
    {
        activeChannel_ = ch;
    }
    static const std::string& activeChannel()
    {
        return activeChannel_;
    }

    // Call once during app init to capture std::cout/cerr into "main".
    static void bootstrapStdCapture()
    {
        Logger::instance().installStdCapture();
        Logger::instance().log("main", "DebugConsole ready");
        Logger::instance().log("localtunnel", "(lt ready)");
    }

    // Render the window (call each frame if visible)
    static void Render()
    {
        if (!visible_)
            return;

        ImGui::SetNextWindowSize(ImVec2(980, 560), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(1.0f); // ensure opaque window
        if (ImGui::Begin("Console", &visible_, ImGuiWindowFlags_NoCollapse))
        {
            float leftWidth = 320.0f;

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
            ImGui::BeginChild("LeftPanel", ImVec2(leftWidth, 0), true);
            renderLeftPanel_();
            ImGui::EndChild();
            ImGui::PopStyleColor();

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.12f, 1.0f));
            ImGui::BeginChild("RightPanel", ImVec2(0, 0), true);
            renderRightPanel_();
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
        ImGui::End();
    }

private:
    // Build display names and channel order with "Main" and "LocalTunnel" first
    static void buildChannelLists_(std::vector<std::string>& display,
                                   std::vector<std::string>& order)
    {
        auto chans = Logger::instance().channels();

        auto has = [&](const std::string& name)
        {
            return std::find(chans.begin(), chans.end(), name) != chans.end();
        };
        display.clear();
        order.clear();

        if (has("main"))
        {
            display.push_back("Main");
            order.push_back("main");
        }
        if (has("localtunnel"))
        {
            display.push_back("LocalTunnel");
            order.push_back("localtunnel");
        }

        for (auto& c : chans)
        {
            if (c != "main" && c != "localtunnel")
            {
                display.push_back(c);
                order.push_back(c);
            }
        }
        if (order.empty())
        { // ensure something is present
            display.push_back("Main");
            order.push_back("main");
        }
    }

    static void renderLeftPanel_()
    {
        ImGui::TextUnformatted("Console Select");
        ImGui::Separator();

        buildChannelLists_(displayNames_, channelOrder_);
        int sel = 0;
        for (int i = 0; i < (int)channelOrder_.size(); ++i)
        {
            if (channelOrder_[i] == activeChannel_)
            {
                sel = i;
                break;
            }
        }

        if (ImGui::BeginCombo("##ChannelCombo", displayNames_.empty() ? "<none>" : displayNames_[sel].c_str()))
        {
            for (int i = 0; i < (int)displayNames_.size(); ++i)
            {
                bool selected = (i == sel);
                if (ImGui::Selectable(displayNames_[i].c_str(), selected))
                {
                    sel = i;
                    activeChannel_ = channelOrder_[i];
                }

                if (selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Clear Console"))
        {
            Logger::instance().clearChannel(activeChannel_);
        }

        ImGui::Dummy(ImVec2(0, 6));
        ImGui::Separator();
        ImGui::TextUnformatted("LocalTunnel");
        if (ImGui::Button("Start"))
        {
            if (ltStart_)
            {
                auto url = ltStart_();
                if (!url.empty())
                    Logger::instance().log("localtunnel", Logger::Level::Success, "Started: " + url);
                else
                    Logger::instance().log("localtunnel", Logger::Level::Error, "Failed to start LT");
            }
            else
            {
                Logger::instance().log("localtunnel", Logger::Level::Error, "Start handler not set");
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop"))
        {
            if (ltStop_)
            {
                ltStop_();
                Logger::instance().log("localtunnel", "Stopped");
            }
            else
            {
                Logger::instance().log("localtunnel", "Stop handler not set");
            }
        }

        ImGui::Dummy(ImVec2(0, 6));
        ImGui::Separator();

        if (ImGui::Button("Print Identity"))
        {
            if (identityLogger_)
            {
                auto log = identityLogger_();
                Logger::instance().log("identity", log);
            }
            else
            {
                Logger::instance().log("localtunnel", "Identity handler not set");
            }
        }

        ImGui::Dummy(ImVec2(0, 6));
        ImGui::Separator();
        // Master switch for executing debug actions per-frame
        ImGui::Checkbox("Run Debug Actions", &debugExecEnabled_);
        ImGui::SetItemTooltip("When ON, the per-frame debug callbacks of active toggles will run.");

        ImGui::TextUnformatted("Debug Toggles");
        ImGui::BeginChild("TogglesScroll", ImVec2(0, -30), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (auto& t : toggles_)
        {
            bool val = t.flagPtr ? *t.flagPtr : false;
            if (ImGui::Checkbox(t.label.c_str(), &val))
            {
                if (t.flagPtr)
                    *t.flagPtr = val;
                if (t.onChanged)
                    t.onChanged(val); // run once on change
            }
        }
        ImGui::EndChild();
    }

    static void renderRightPanel_()
    {
        ImGui::Checkbox("Auto-scroll", &autoScroll_);
        ImGui::SameLine();
        static bool showTimestamps = true; // default ON
        ImGui::Checkbox("Timestamps", &showTimestamps);

        ImGui::Separator();

        auto entries = Logger::instance().getChannel(activeChannel_);

        ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        for (auto& e : entries)
        {
            ImVec4 col = colorForLevel_(e.level); // see helper below

            if (showTimestamps)
            {
                if (e.tsStr.empty())
                    e.tsStr = Logger::formatTs(e.tsMs);
                ImGui::PushStyleColor(ImGuiCol_Text, colorForLevel_(e.level)); // if you color by level
                ImGui::TextWrapped("%s", e.text.c_str());                      // wraps at window edge
                ImGui::PopStyleColor();
            }
            else
            {
                ImGui::TextColored(col, "%s", e.text.c_str());
            }
        }
        if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 5.0f)
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
    }

    static ImVec4 colorForLevel_(Logger::Level lvl)
    {
        switch (lvl)
        {
            case Logger::Level::Trace:
                return ImVec4(0.65f, 0.65f, 0.65f, 1.0f);
            case Logger::Level::Debug:
                return ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
            case Logger::Level::Info:
                return ImVec4(0.60f, 0.80f, 1.00f, 1.0f);
            case Logger::Level::Warn:
                return ImVec4(1.00f, 0.85f, 0.40f, 1.0f);
            case Logger::Level::Success:
                return ImVec4(0.45f, 0.95f, 0.55f, 1.0f);
            case Logger::Level::Error:
                return ImVec4(1.00f, 0.45f, 0.45f, 1.0f);
        }
        return ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
    }

private:
    inline static bool visible_ = false;
    inline static std::string activeChannel_ = "main";
    inline static bool autoScroll_ = true;

    inline static std::function<std::string()> ltStart_;
    inline static std::function<std::string()> identityLogger_;
    inline static std::function<void()> ltStop_;
    inline static std::vector<DebugToggle> toggles_;
    inline static bool debugExecEnabled_ = false;

    inline static std::vector<std::string> displayNames_;
    inline static std::vector<std::string> channelOrder_;
};

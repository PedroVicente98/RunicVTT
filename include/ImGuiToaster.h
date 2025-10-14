#pragma once
#include <imgui.h>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <memory>
#include <atomic>

class ImGuiToaster
{
public:
    enum class Level
    {
        Info,
        Good,
        Warning,
        Error
    };

    struct Config
    {
        // Visuals
        float bgOpacity = 0.80f; // final window opacity (0..1). We'll apply this to level color.
        float rounding = 6.0f;
        ImVec2 windowPadding = ImVec2(10.f, 8.f);
        float verticalSpacing = 36.f;
        float edgePadding = 10.f;

        // Sizing
        bool autoResize = true;              // auto-fit to content (clamped by constraints)
        ImVec2 minSize = ImVec2(360.f, 0.f); // good base width for long phrases
        ImVec2 maxSize = ImVec2(0.f, 0.f);   // 0,0 => will auto-cap to 90% of work area
        ImVec2 fixedSize = ImVec2(0.f, 0.f); // if >0 for x/y, forces that dimension
        float maxWidth = 480.f;              // text wrap width (0 = use content region avail)
        bool wrapText = true;

        // Positioning
        ImVec2 anchorPivot = ImVec2(1.f, 0.f); // top-right
        enum class Corner
        {
            TopLeft,
            TopRight,
            BottomLeft,
            BottomRight
        };
        Corner corner = Corner::TopRight;

        // Behavior
        size_t maxToasts = 8;
        bool clickThrough = false; // let clicks pass through
        bool focusOnAppear = false;
        bool killOnClickAnywhere = true;

        // Colors per level (used as WINDOW background color; text is always white)
        ImVec4 colorInfo = ImVec4(0.20f, 0.45f, 0.85f, 1.0f);    // blue-ish
        ImVec4 colorGood = ImVec4(0.16f, 0.65f, 0.22f, 1.0f);    // green
        ImVec4 colorWarning = ImVec4(0.90f, 0.70f, 0.10f, 1.0f); // yellow
        ImVec4 colorError = ImVec4(0.85f, 0.25f, 0.25f, 1.0f);   // red
        ImVec4 textColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);       // always white for contrast
        bool showBorder = false;
        ImVec4 borderColor = ImVec4(0, 0, 0, 0.35f); // optional subtle border
        float borderSize = 1.0f;
    };

    ImGuiToaster() = default;
    explicit ImGuiToaster(const Config& cfg) :
        cfg_(cfg) {}

    void Push(Level lvl, const std::string& msg, float durationSec = 4.0f)
    {
        using clock = std::chrono::steady_clock;
        Toast t;
        t.id = next_id_.fetch_add(1, std::memory_order_relaxed);
        t.message = msg;
        t.level = lvl;
        t.expiresAt = clock::now() + std::chrono::duration_cast<clock::duration>(std::chrono::duration<float>(durationSec));
        {
            std::scoped_lock lk(mtx_);
            toasts_.push_back(std::move(t));
            if (toasts_.size() > cfg_.maxToasts)
                toasts_.pop_front();
        }
    }
    void Info(const std::string& msg, float sec = 5.f)
    {
        Push(Level::Info, msg, sec);
    }
    void Good(const std::string& msg, float sec = 5.f)
    {
        Push(Level::Good, msg, sec);
    }
    void Warn(const std::string& msg, float sec = 5.f)
    {
        Push(Level::Warning, msg, sec);
    }
    void Error(const std::string& msg, float sec = 5.f)
    {
        Push(Level::Error, msg, sec);
    }

    void Clear()
    {
        std::scoped_lock lk(mtx_);
        toasts_.clear();
    }

    // Call once per frame (after ImGui::NewFrame, before Render)
    void Render()
    {
        using clock = std::chrono::steady_clock;

        // Copy & prune under lock
        std::vector<Toast> local;
        {
            std::scoped_lock lk(mtx_);
            const auto now = clock::now();
            while (!toasts_.empty() && toasts_.front().expiresAt <= now)
                toasts_.pop_front();
            local.assign(toasts_.begin(), toasts_.end());
        }
        if (local.empty())
            return;

        ImGuiViewport* vp = ImGui::GetMainViewport();
        const float PAD = cfg_.edgePadding;

        ImVec2 basePos;
        switch (cfg_.corner)
        {
            case Config::Corner::TopLeft:
                basePos = ImVec2(vp->WorkPos.x + PAD, vp->WorkPos.y + PAD);
                break;
            case Config::Corner::TopRight:
                basePos = ImVec2(vp->WorkPos.x + vp->WorkSize.x - PAD, vp->WorkPos.y + PAD);
                break;
            case Config::Corner::BottomLeft:
                basePos = ImVec2(vp->WorkPos.x + PAD, vp->WorkPos.y + vp->WorkSize.y - PAD);
                break;
            case Config::Corner::BottomRight:
                basePos = ImVec2(vp->WorkPos.x + vp->WorkSize.x - PAD, vp->WorkPos.y + vp->WorkSize.y - PAD);
                break;
        }

        ImVec2 anchor = cfg_.anchorPivot;
        float y_step = (cfg_.corner == Config::Corner::BottomLeft || cfg_.corner == Config::Corner::BottomRight)
                           ? -cfg_.verticalSpacing
                           : cfg_.verticalSpacing;

        ImVec2 pos = basePos;
        int idx = 0;

        for (const auto& t : local)
        {
            const ImVec4 lvlCol = ColorForLevel_(t.level);
            ImVec4 bg = lvlCol;
            bg.w = cfg_.bgOpacity; // apply opacity to window bg
            bool delete_this_toast = false;
            
            ImGui::SetNextWindowViewport(vp->ID);
            ImGui::SetNextWindowPos(pos, ImGuiCond_Always, anchor);

            // Sizing setup
            if (cfg_.fixedSize.x > 0.f || cfg_.fixedSize.y > 0.f)
            {
                ImGui::SetNextWindowSize(ImVec2(
                                             cfg_.fixedSize.x > 0.f ? cfg_.fixedSize.x : 0.f,
                                             cfg_.fixedSize.y > 0.f ? cfg_.fixedSize.y : 0.f),
                                         ImGuiCond_Always);
            }
            else
            {
                ImVec2 minC = cfg_.minSize;
                ImVec2 maxC = cfg_.maxSize;
                if (maxC.x <= 0.f || maxC.y <= 0.f)
                {
                    maxC.x = (maxC.x <= 0.f) ? (vp->WorkSize.x * 0.9f) : maxC.x;
                    maxC.y = (maxC.y <= 0.f) ? (vp->WorkSize.y * 0.9f) : maxC.y;
                }
                ImGui::SetNextWindowSizeConstraints(minC, maxC);
                if (!cfg_.autoResize)
                {
                    ImGui::SetNextWindowSize(minC, ImGuiCond_Always);
                }
            }

            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                     ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoNav;
            if (!cfg_.focusOnAppear)
                flags |= ImGuiWindowFlags_NoFocusOnAppearing;
            if (cfg_.clickThrough && !cfg_.killOnClickAnywhere)
                flags |= ImGuiWindowFlags_NoInputs;
            if (cfg_.autoResize && !(cfg_.fixedSize.x > 0.f || cfg_.fixedSize.y > 0.f))
                flags |= ImGuiWindowFlags_AlwaysAutoResize;

            // Style: background colored by level, text always white, optional border
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, cfg_.rounding);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, cfg_.windowPadding);

            ImGui::PushStyleColor(ImGuiCol_WindowBg, bg);
            if (cfg_.showBorder)
            {
                ImGui::PushStyleColor(ImGuiCol_Border, cfg_.borderColor);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, cfg_.borderSize);
            }

            std::string name = "##toast-" + std::to_string(idx++);
            if (ImGui::Begin(name.c_str(), nullptr, flags))
            {
                if (cfg_.killOnClickAnywhere) {
                    // Cover the content region with an invisible button.
                    // It won't change layout because we restore the cursor after.
                    ImVec2 crMin = ImGui::GetWindowContentRegionMin();
                    ImVec2 crMax = ImGui::GetWindowContentRegionMax();
                    ImVec2 old   = ImGui::GetCursorPos();
                
                    ImGui::SetCursorPos(crMin);
                    ImVec2 size(crMax.x - crMin.x, crMax.y - crMin.y);
                    ImGui::InvisibleButton("##toast_kill", size);
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                        delete_this_toast = true;
                    }
                    ImGui::SetCursorPos(old);
                }
                // Text wrapping
                float wrapAt = 0.f;
                if (cfg_.wrapText)
                {
                    wrapAt = (cfg_.maxWidth > 0.f) ? cfg_.maxWidth : ImGui::GetContentRegionAvail().x;
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapAt);
                }

                ImGui::PushStyleColor(ImGuiCol_Text, cfg_.textColor);
                ImGui::TextUnformatted(t.message.c_str());
                ImGui::PopStyleColor();

                if (cfg_.wrapText)
                {
                    ImGui::PopTextWrapPos();
                }
            }
            ImGui::End();

            if (cfg_.showBorder)
            {
                ImGui::PopStyleVar();   // WindowBorderSize
                ImGui::PopStyleColor(); // Border
            }
            ImGui::PopStyleColor(); // WindowBg
            ImGui::PopStyleVar(2);  // rounding, padding
            if (cfg_.killOnClickAnywhere && delete_this_toast) {
                std::scoped_lock lk(mtx_);
                auto it_real = std::find_if(toasts_.begin(), toasts_.end(),
                    [&](const Toast& tt){ return tt.id == t.id; });
                if (it_real != toasts_.end()) toasts_.erase(it_real);
            }
            pos.y += y_step;
        }
    }

    void SetConfig(const Config& cfg)
    {
        cfg_ = cfg;
    }
    const Config& GetConfig() const
    {
        return cfg_;
    }

private:
    std::atomic<uint64_t> next_id_{1};
    struct Toast
    {
        uint64_t id = 0;
        std::string message;
        Level level;
        std::chrono::steady_clock::time_point expiresAt;
    };

    ImVec4 ColorForLevel_(Level lvl) const
    {
        switch (lvl)
        {
            case Level::Good:
                return cfg_.colorGood;
            case Level::Warning:
                return cfg_.colorWarning;
            case Level::Error:
                return cfg_.colorError;
            case Level::Info:
            default:
                return cfg_.colorInfo;
        }
    }

    mutable std::mutex mtx_;
    std::deque<Toast> toasts_;
    Config cfg_;
};

/*
USAGE
--------------------------------------------------------
// Somewhere in your app bootstrap:
auto g_toaster = std::make_shared<ImGuiToaster>();

// Optional: tweak look/feel
ImGuiToaster::Config cfg;
cfg.corner = ImGuiToaster::Config::Corner::TopRight;
cfg.maxToasts = 8;
g_toaster->SetConfig(cfg);

// Give to your managers (NetworkManager, GameTableManager, AutosaveManager, etc.)
networkManager->setToaster(g_toaster);
gameTableManager->setToaster(g_toaster);
// autosaveManager->setToaster(g_toaster); // if you want autosave messages here

--------------------------------------------------------
// In NetworkManager:
class NetworkManager {
public:
    void setToaster(std::shared_ptr<ImGuiToaster> t) { toaster_ = std::move(t); }

    void pushStatusToast(const std::string& msg, ImGuiToaster::Level lvl, float durationSec) {
        if (toaster_) toaster_->Push(lvl, msg, durationSec);
    }

private:
    std::shared_ptr<ImGuiToaster> toaster_;
};
----------------------------------------------------------
// In your main GUI frame (once per frame, after NewFrame, before Render):
if (g_toaster) g_toaster->Render();
-----------------------------------------------------------
bool ok = //your save routine /;
if (ok) {
    g_toaster->Good("GameTable Saved!!", 5.0f);
} else {
    g_toaster->Error("Save Failed: <reason>", 5.0f);
}
--------------------------------------------------------------------
// NetworkManager.hpp
#include <memory>
#include "ImGuiToaster.hpp"

class NetworkManager {
public:
    void setToaster(std::shared_ptr<ImGuiToaster> t) { toaster_ = std::move(t); }

    // Unified push (replaces your old pushStatusToast)
    void pushStatusToast(const std::string& msg, ImGuiToaster::Level lvl, float durationSec = 5.0f) {
        if (toaster_) toaster_->Push(lvl, msg, durationSec);
    }

    // Example: call on events
    void onConnected()  { pushStatusToast("Connected",  ImGuiToaster::Level::Good,    4.0f); }
    void onDisconnected(){ pushStatusToast("Disconnected", ImGuiToaster::Level::Warning, 5.0f); }
    void onError(const std::string& e){ pushStatusToast("Network error: " + e, ImGuiToaster::Level::Error, 6.0f); }

private:
    std::shared_ptr<ImGuiToaster> toaster_;
};
--------------------------------------------------------------------
void GameTableManager::renderToasts(std::shared_ptr<ImGuiToaster> toaster) {
    if (toaster) toaster->Render();
}
--------------------------------------------------------------------
// During initialization
auto toaster = std::make_shared<ImGuiToaster>();
ImGuiToaster::Config cfg;
cfg.corner = ImGuiToaster::Config::Corner::TopRight;
cfg.maxToasts = 8;
toaster->SetConfig(cfg);

// Pass to your managers
networkManager->setToaster(toaster);
gameTableManager->setToaster(toaster); // if you keep a ref there
// autosaveManager->setToaster(toaster); // optional

// In your frame loop, after ImGui::NewFrame() and before ImGui::Render():
// ... your UI
// Render toasts ONCE, globally
toaster->Render();
--------------------------------------------------------------------
// Somewhere in your input handling (per-frame)
if (ImGui::IsKeyPressed(ImGuiKey_1)) toaster->Info ("Info: Hello!", 5.0f);
if (ImGui::IsKeyPressed(ImGuiKey_2)) toaster->Good ("Good: Saved",  5.0f);
if (ImGui::IsKeyPressed(ImGuiKey_3)) toaster->Warn ("Warning: Ping high", 5.0f);
if (ImGui::IsKeyPressed(ImGuiKey_4)) toaster->Error("Error: Failed op",   5.0f);
---------------------------------------------------------------------------

*/

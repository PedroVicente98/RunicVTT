#pragma once
#include <imgui.h>
#include <deque>
#include <mutex>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <memory>

class ImGuiToaster {
public:
    enum class Level { Info, Good, Warning, Error };

    struct Config {
        // Visuals
        float bgAlpha         = 0.90f;
        float rounding        = 6.0f;
        ImVec2 windowPadding  = ImVec2(10.f, 8.f);
        float verticalSpacing = 36.f;
        float edgePadding     = 10.f;

        // Sizing
        bool   autoResize     = true;               // if true, window fits content but respects minSize/maxSize
        ImVec2 minSize        = ImVec2(360.f, 0.f); // ensure a decent width for long phrases
        ImVec2 maxSize        = ImVec2(0.f, 0.f);   // 0,0 = no max constraint
        ImVec2 fixedSize      = ImVec2(0.f, 0.f);   // if x>0 or y>0, forces exact window size (overrides autoResize)
        float  maxWidth       = 480.f;              // wrap at this width (content), 0 = no wrap
        bool   wrapText       = true;

        // Positioning
        ImVec2 anchorPivot    = ImVec2(1.f, 0.f);   // top-right by default
        enum class Corner { TopLeft, TopRight, BottomLeft, BottomRight };
        Corner corner         = Corner::TopRight;

        // Behavior
        size_t maxToasts      = 8;
        bool   clickThrough   = true;               // let clicks pass through
        bool   focusOnAppear  = false;              // typically false for toasts

        // Colors
        ImVec4 colorInfo      = ImVec4(0.60f, 0.80f, 1.00f, 1.0f);
        ImVec4 colorGood      = ImVec4(0.20f, 1.00f, 0.20f, 1.0f);
        ImVec4 colorWarning   = ImVec4(1.00f, 0.90f, 0.20f, 1.0f);
        ImVec4 colorError     = ImVec4(1.00f, 0.20f, 0.20f, 1.0f);
    };

    ImGuiToaster() = default;
    explicit ImGuiToaster(const Config& cfg) : cfg_(cfg) {}

    void Push(Level lvl, const std::string& msg, float durationSec = 5.0f) {
        using clock = std::chrono::steady_clock;
        Toast t;
        t.message   = msg;
        t.level     = lvl;
        t.expiresAt = clock::now() + std::chrono::duration_cast<clock::duration>(std::chrono::duration<float>(durationSec));
        {
            std::scoped_lock lk(mtx_);
            toasts_.push_back(std::move(t));
            if (toasts_.size() > cfg_.maxToasts)
                toasts_.pop_front();
        }
    }
    void Info (const std::string& msg, float sec=5.f){ Push(Level::Info,    msg, sec); }
    void Good (const std::string& msg, float sec=5.f){ Push(Level::Good,    msg, sec); }
    void Warn (const std::string& msg, float sec=5.f){ Push(Level::Warning, msg, sec); }
    void Error(const std::string& msg, float sec=5.f){ Push(Level::Error,   msg, sec); }

    void Clear() {
        std::scoped_lock lk(mtx_);
        toasts_.clear();
    }

    // Call once per frame (after NewFrame, before Render)
    void Render() {
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
        if (local.empty()) return;

        ImGuiViewport* vp = ImGui::GetMainViewport();
        const float PAD = cfg_.edgePadding;

        ImVec2 basePos;
        switch (cfg_.corner) {
            case Config::Corner::TopLeft:     basePos = ImVec2(vp->WorkPos.x + PAD,                         vp->WorkPos.y + PAD); break;
            case Config::Corner::TopRight:    basePos = ImVec2(vp->WorkPos.x + vp->WorkSize.x - PAD,        vp->WorkPos.y + PAD); break;
            case Config::Corner::BottomLeft:  basePos = ImVec2(vp->WorkPos.x + PAD,                         vp->WorkPos.y + vp->WorkSize.y - PAD); break;
            case Config::Corner::BottomRight: basePos = ImVec2(vp->WorkPos.x + vp->WorkSize.x - PAD,        vp->WorkPos.y + vp->WorkSize.y - PAD); break;
        }

        ImVec2 anchor = cfg_.anchorPivot;
        float y_step = (cfg_.corner == Config::Corner::BottomLeft || cfg_.corner == Config::Corner::BottomRight)
                       ? -cfg_.verticalSpacing : cfg_.verticalSpacing;

        ImVec2 pos = basePos;
        int idx = 0;

        for (const auto& t : local) {
            ImGui::SetNextWindowBgAlpha(cfg_.bgAlpha);
            ImGui::SetNextWindowViewport(vp->ID);
            ImGui::SetNextWindowPos(pos, ImGuiCond_Always, anchor);

            // Sizing behavior:
            // - If fixedSize.x/y > 0: force exact size
            // - Else if autoResize: let it auto-resize but clamp to min/max with constraints
            // - Else: use minSize as a default base size (still can be resized by the user if you enable it)
            if (cfg_.fixedSize.x > 0.f || cfg_.fixedSize.y > 0.f) {
                ImGui::SetNextWindowSize(ImVec2(
                    cfg_.fixedSize.x > 0.f ? cfg_.fixedSize.x : 0.f,
                    cfg_.fixedSize.y > 0.f ? cfg_.fixedSize.y : 0.f
                ), ImGuiCond_Always);
            } else {
                // Constraints apply either way; with autoResize, it limits the auto size; without, it enforces min.
                ImVec2 minC = cfg_.minSize;
                ImVec2 maxC = cfg_.maxSize; // (0,0) means no max
                if (maxC.x <= 0.f || maxC.y <= 0.f) {
                    // If not provided, you can safely allow giant height, and width up to work area
                    maxC.x = (maxC.x <= 0.f) ? (vp->WorkSize.x * 0.9f) : maxC.x;
                    maxC.y = (maxC.y <= 0.f) ? (vp->WorkSize.y * 0.9f) : maxC.y;
                }
                ImGui::SetNextWindowSizeConstraints(minC, maxC);
                if (!cfg_.autoResize) {
                    // Give it a starting size = minSize (so it doesn't collapse smaller than your desired default)
                    ImGui::SetNextWindowSize(minC, ImGuiCond_Always);
                }
            }

            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                     ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoNav;
            if (!cfg_.focusOnAppear) flags |= ImGuiWindowFlags_NoFocusOnAppearing;
            if (cfg_.clickThrough)   flags |= ImGuiWindowFlags_NoInputs;
            if (cfg_.autoResize && !(cfg_.fixedSize.x > 0.f || cfg_.fixedSize.y > 0.f))
                flags |= ImGuiWindowFlags_AlwaysAutoResize;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, cfg_.rounding);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, cfg_.windowPadding);

            std::string name = "##toast-" + std::to_string(idx++);
            if (ImGui::Begin(name.c_str(), nullptr, flags)) {
                ImVec4 col = ColorForLevel_(t.level);

                // Optional wrap for long lines (uses content region width minus padding)
                if (cfg_.wrapText) {
                    float wrapAt = cfg_.maxWidth > 0.f ? cfg_.maxWidth : 0.f;
                    if (wrapAt <= 0.f) {
                        // Use available content width as a fallback
                        wrapAt = ImGui::GetContentRegionAvail().x;
                    }
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + wrapAt);
                    ImGui::TextColored(col, "%s", t.message.c_str());
                    ImGui::PopTextWrapPos();
                } else {
                    ImGui::TextColored(col, "%s", t.message.c_str());
                }
            }
            ImGui::End();
            ImGui::PopStyleVar(2);

            pos.y += y_step;
        }
    }

    void SetConfig(const Config& cfg) { cfg_ = cfg; }
    const Config& GetConfig() const { return cfg_; }

private:
    struct Toast {
        std::string message;
        Level level;
        std::chrono::steady_clock::time_point expiresAt;
    };

    ImVec4 ColorForLevel_(Level lvl) const {
        switch (lvl) {
            case Level::Good:    return cfg_.colorGood;
            case Level::Warning: return cfg_.colorWarning;
            case Level::Error:   return cfg_.colorError;
            case Level::Info:
            default:             return cfg_.colorInfo;
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


*/

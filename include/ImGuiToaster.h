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
        float verticalSpacing = 36.f;           // space between stacked toasts
        float edgePadding     = 10.f;           // distance from viewport edge

        // Positioning (top-right default)
        ImVec2 anchorPivot    = ImVec2(1.f, 0.f); // (1,0) == top-right, (0,0) == top-left, (1,1) == bottom-right, etc.
        enum class Corner { TopLeft, TopRight, BottomLeft, BottomRight };
        Corner corner         = Corner::TopRight;

        // Behavior
        size_t maxToasts      = 8;
        bool   clickThrough   = true;           // NoInputs so clicks pass through
        bool   autoResize     = true;           // AlwaysAutoResize

        // Colors per level
        ImVec4 colorInfo      = ImVec4(0.60f, 0.80f, 1.00f, 1.0f);
        ImVec4 colorGood      = ImVec4(0.20f, 1.00f, 0.20f, 1.0f);
        ImVec4 colorWarning   = ImVec4(1.00f, 0.90f, 0.20f, 1.0f);
        ImVec4 colorError     = ImVec4(1.00f, 0.20f, 0.20f, 1.0f);
    };

    ImGuiToaster() = default;
    explicit ImGuiToaster(const Config& cfg) : cfg_(cfg) {}

    // Push helpers
    void Push(Level lvl, const std::string& msg, float durationSec = 5.0f) {
        using clock = std::chrono::steady_clock;
        auto now = clock::now();
        Toast t;
        t.message   = msg;
        t.level     = lvl;
        t.expiresAt = now + std::chrono::duration_cast<clock::duration>(std::chrono::duration<float>(durationSec));
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

    // Call this once per frame (after BeginFrame, before EndFrame)
    void Render() {
        using clock = std::chrono::steady_clock;

        // Copy & prune under lock to avoid holding the lock during ImGui calls
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

            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                     ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoFocusOnAppearing |
                                     ImGuiWindowFlags_NoNav;

            if (cfg_.autoResize) flags |= ImGuiWindowFlags_AlwaysAutoResize;
            if (cfg_.clickThrough) flags |= ImGuiWindowFlags_NoInputs;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, cfg_.rounding);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, cfg_.windowPadding);

            std::string name = "##toast-" + std::to_string(idx++);
            if (ImGui::Begin(name.c_str(), nullptr, flags)) {
                ImVec4 col = ColorForLevel_(t.level);
                ImGui::TextColored(col, "%s", t.message.c_str());
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

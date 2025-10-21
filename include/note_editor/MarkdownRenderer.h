#pragma once
#include "imgui_md.h"
#include <functional>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstring> // for std::strlen

// Links-only markdown renderer:
// - overrides imgui_md::open_url()
// - supports: http(s), roll:, note:/ and note://open/ refs
class MarkdownRenderer : public imgui_md
{
public:
    enum class Action
    {
        OpenExternal, // http/https
        RollExpr,     // roll:1d20+5 or roll://1d20+5
        NoteOpen,     // note://open/<ref>, note:/<ref>, note:<ref>
        Unknown
    };

    // Optional hooks:
    std::function<void(const std::string& url)> onOpenExternal; // external URL
    std::function<void(const std::string& expr)> onRoll;        // dice expression
    std::function<void(const std::string& uuid)> onNoteOpen;    // open note by resolved UUID

    // Ref resolver: title / short-id / uuid -> uuid (empty if not found)
    std::function<std::string(const std::string& ref)> resolveNoteRef;

    MarkdownRenderer() = default;

    // imgui_md hook: called when a markdown link is activated
    void open_url() const override;
    void soft_break() override;

    static void openDefaultBrowser_(const std::string& url);

private:
    struct Parsed
    {
        Action kind{Action::Unknown};
        std::string payload; // expr, url, or note ref
    };

    // Parsing & resolution (helpers)
    Parsed parseHref_(const std::string& href) const;
    std::string resolveNote_(const std::string& ref) const;

    // Platform opener (implemented in .cpp, no headers leak here)

    // Small helpers
    static bool startsWith_(const std::string& s, const char* pref);
    static bool looksLikeUuid_(const std::string& s);
    static bool looksLikeShortId_(const std::string& s);
    static bool isHex_(char c);
};

//// MarkdownRenderer.h
//#pragma once
//#include "imgui_md.h"
//#include <functional>
//#include <string>
//#include <cctype>
//#include <algorithm>
//#include <windows.h>
//#include <shellapi.h>
//
//// Single-file, links-only markdown renderer.
//// - Inherit from imgui_md and override open_url()
//// - Ignore images/fonts/div styling for now (add later if needed)
//class MarkdownRenderer : public imgui_md
//{
//public:
//    enum class Action
//    {
//        OpenExternal, // http/https
//        RollExpr,     // roll:1d20+5 or roll://1d20+5
//        NoteOpen,     // note://open/<ref> or note:/<ref> (short form)
//        Unknown
//    };
//
//    // Callbacks you can hook from NoteEditorUI:
//    //  - If not set, defaults will be used (open external in browser; others no-op).
//    std::function<void(const std::string& url)> onOpenExternal; // default: opens default browser
//    std::function<void(const std::string& expr)> onRoll;        // e.g., send to chat dice roller
//    std::function<void(const std::string& uuid)> onNoteOpen;    // open a note tab by final UUID
//
//    // Resolver to map human-friendly refs -> UUID (title, slug, short id, etc.)
//    // Return empty string if not found. If not set, note links will be ignored gracefully.
//    std::function<std::string(const std::string& ref)> resolveNoteRef;
//
//    MarkdownRenderer() = default;
//
//    // imgui_md hook: invoked on link click with m_href filled.
//    void open_url() const override
//    {
//        const auto a = parseHref_(m_href);
//        switch (a.kind)
//        {
//            case Action::OpenExternal:
//                openDefaultBrowser_(a.payload);
//                break;
//
//            case Action::RollExpr:
//                if (onRoll)
//                    onRoll(a.payload);
//                break;
//
//            case Action::NoteOpen:
//            {
//                // allow full UUID, short UUID prefix, title/slug
//                std::string uuid = resolveNote_(a.payload);
//                if (!uuid.empty() && onNoteOpen)
//                    onNoteOpen(uuid);
//                break;
//            }
//
//            default:
//                // Fallback: try opening externally
//                openDefaultBrowser_(m_href);
//                break;
//        }
//    }
//
//private:
//    struct Parsed
//    {
//        Action kind{Action::Unknown};
//        std::string payload; // expr, url, ref, etc.
//    };
//
//    // -------- HREF parsing (all-in-one) --------
//    Parsed parseHref_(const std::string& href) const
//    {
//        // external?
//        if (startsWith_(href, "http://") || startsWith_(href, "https://"))
//        {
//            return {Action::OpenExternal, href};
//        }
//
//        // roll://expr or roll:expr
//        if (startsWith_(href, "roll://"))
//        {
//            return {Action::RollExpr, href.substr(7)};
//        }
//        if (startsWith_(href, "roll:"))
//        {
//            return {Action::RollExpr, href.substr(5)};
//        }
//
//        // note://open/<ref>
//        if (startsWith_(href, "note://open/"))
//        {
//            return {Action::NoteOpen, href.substr(std::string("note://open/").size())};
//        }
//
//        // Short form: note:/<ref>  (less typing)
//        if (startsWith_(href, "note:/"))
//        {
//            return {Action::NoteOpen, href.substr(6)};
//        }
//
//        // Optional: allow bare "note:<ref>"
//        if (startsWith_(href, "note:"))
//        {
//            return {Action::NoteOpen, href.substr(5)};
//        }
//
//        return {};
//    }
//
//    // -------- Note reference resolution --------
//    // Accepts:
//    //  - full UUID string
//    //  - short uuid prefix (>= 8 hex chars)
//    //  - title/slug (delegate to resolveNoteRef)
//    std::string resolveNote_(const std::string& ref) const
//    {
//        // If looks like a full UUID (very loose check), accept as-is.
//        if (looksLikeUuid_(ref))
//            return ref;
//
//        // If short hex-ish id (>=8), let resolver handle it (could be prefix).
//        if (looksLikeShortId_(ref))
//        {
//            if (resolveNoteRef)
//                return resolveNoteRef(ref);
//            return {};
//        }
//
//        // Otherwise treat as title/slug/alias
//        if (resolveNoteRef)
//            return resolveNoteRef(ref);
//        return {};
//    }
//
//    // -------- Platform opener (default browser) --------
//private:
//    static void openDefaultBrowser_(const std::string& url)
//    {
//        if (url.empty())
//            return;
//        // prefer ShellExecuteA for default handler
//        HINSTANCE r = ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
//        if ((INT_PTR)r <= 32)
//        {
//            // fallback
//            std::string cmd = "start \"\" \"" + url + "\"";
//            std::system(cmd.c_str());
//        }
//    }
//
//    // -------- small helpers --------
//    static bool startsWith_(const std::string& s, const char* pref)
//    {
//        const size_t n = std::strlen(pref);
//        return s.size() >= n && std::equal(pref, pref + n, s.begin());
//    }
//
//    static bool looksLikeUuid_(const std::string& s)
//    {
//        // Very loose check: 36 chars 8-4-4-4-12 with hex + dashes
//        if (s.size() != 36)
//            return false;
//        const int dashPos[4] = {8, 13, 18, 23};
//        for (int i = 0; i < 36; ++i)
//        {
//            if (i == dashPos[0] || i == dashPos[1] || i == dashPos[2] || i == dashPos[3])
//            {
//                if (s[i] != '-')
//                    return false;
//            }
//            else if (!isHex_(s[i]))
//            {
//                return false;
//            }
//        }
//        return true;
//    }
//
//    static bool looksLikeShortId_(const std::string& s)
//    {
//        // Accept short hex prefixes >= 8 chars (no dashes)
//        if (s.size() < 8)
//            return false;
//        for (char c : s)
//        {
//            if (c == '-')
//                return false;
//            if (!isHex_(c))
//                return false;
//        }
//        return true;
//    }
//
//    static bool isHex_(char c)
//    {
//        return std::isxdigit(static_cast<unsigned char>(c)) != 0;
//    }
//};

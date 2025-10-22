#include "MarkdownRenderer.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

// ---------- imgui_md override ----------
void MarkdownRenderer::open_url() const
{
    const auto a = parseHref_(m_href);
    switch (a.kind)
    {
        case Action::OpenExternal:
            if (onOpenExternal)
                onOpenExternal(a.payload);
            break;

        case Action::RollExpr:
            if (onRoll)
                onRoll(a.payload);
            break;

        case Action::NoteOpen:
        {
            std::string uuid = resolveNote_(a.payload);
            if (!uuid.empty() && onNoteOpen)
                onNoteOpen(uuid);
            break;
        }

        default:
            // Fallback: try external open if handler present
            if (onOpenExternal)
                onOpenExternal(m_href);
            else
                openDefaultBrowser_(m_href);
            break;
    }
}

void MarkdownRenderer::soft_break()
{
    // Turn Markdown soft-breaks into visible line breaks.
    // Either of these works; TextUnformatted("\n") is a common pattern with imgui_md.
    ImGui::TextUnformatted("\n");
    // Alternatively:
    // ImGui::NewLine();
}

// ---------- parse / resolve ----------
MarkdownRenderer::Parsed MarkdownRenderer::parseHref_(const std::string& href) const
{
    // external?
    if (startsWith_(href, "http://") || startsWith_(href, "https://"))
        return {Action::OpenExternal, href};

    // roll://expr or roll:expr
    if (startsWith_(href, "roll://"))
        return {Action::RollExpr, href.substr(7)};
    if (startsWith_(href, "roll:"))
        return {Action::RollExpr, href.substr(5)};

    // note://open/<ref>
    if (startsWith_(href, "note://open/"))
        return {Action::NoteOpen, href.substr(std::string("note://open/").size())};

    // Short forms: note:/<ref> / note:<ref>
    if (startsWith_(href, "note:/"))
        return {Action::NoteOpen, href.substr(6)};
    if (startsWith_(href, "note:"))
        return {Action::NoteOpen, href.substr(5)};

    return {};
}

std::string MarkdownRenderer::resolveNote_(const std::string& ref) const
{
    if (looksLikeUuid_(ref))
        return ref;

    if (resolveNoteRef)
        return resolveNoteRef(ref);

    return {};
}

// ---------- platform opener ----------
void MarkdownRenderer::openDefaultBrowser_(const std::string& url)
{
    if (url.empty())
        return;

    // Use wide API for unicode URLs
    int wlen = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
    std::wstring wurl;
    wurl.resize(wlen ? (wlen - 1) : 0);
    if (wlen > 0)
        MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, wurl.data(), wlen);

    HINSTANCE r = ShellExecuteW(nullptr, L"open", wurl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)r <= 32)
    {
        // Very dumb fallback
        std::string cmd = "start \"\" \"" + url + "\"";
        std::system(cmd.c_str());
    }
}

// ---------- small helpers ----------
bool MarkdownRenderer::startsWith_(const std::string& s, const char* pref)
{
    const size_t n = std::strlen(pref);
    return s.size() >= n && std::equal(pref, pref + n, s.begin());
}

bool MarkdownRenderer::looksLikeUuid_(const std::string& s)
{
    if (s.size() != 36)
        return false;
    const int dashPos[4] = {8, 13, 18, 23};
    for (int i = 0; i < 36; ++i)
    {
        if (i == dashPos[0] || i == dashPos[1] || i == dashPos[2] || i == dashPos[3])
        {
            if (s[i] != '-')
                return false;
        }
        else if (!isHex_(s[i]))
        {
            return false;
        }
    }
    return true;
}

bool MarkdownRenderer::looksLikeShortId_(const std::string& s)
{
    if (s.size() < 8)
        return false;
    for (char c : s)
    {
        if (c == '-')
            return false;
        if (!isHex_(c))
            return false;
    }
    return true;
}

bool MarkdownRenderer::isHex_(char c)
{
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
}

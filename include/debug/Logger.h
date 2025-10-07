// Logger.h (replace the previous version)
#pragma once
#include <mutex>
#include <unordered_map>
#include <deque>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <memory>
#include <ostream>
#include <chrono>
#include <cctype>

class Logger
{
public:
    enum class Level : uint8_t
    {
        Trace,
        Debug,
        Info,
        Warn,
        Error,
        Success
    };

    struct LogEntry
    {
        std::string text;
        Level level;
        uint64_t tsMs; // epoch ms (render formats once)
        // cached formatted time (optional; filled lazily)
        mutable std::string tsStr;
    };

    using Sink = std::function<void(const std::string& channel, const LogEntry& e)>;

    static Logger& instance()
    {
        static Logger L;
        return L;
    }

    // --- Public API ----------------------------------------------------------

    // Simple: default to Info (with auto-detect)
    void log(const std::string& channel, const std::string& line)
    {
        log(channel, Level::Info, line);
    }

    // Structured: you can set level explicitly if you want
    void log(const std::string& channel, Level lvl, const std::string& line)
    {
        LogEntry e{line, lvl, nowMs_(), {}};
        // auto-detect if level was Info but looks like warn/error
        if (lvl == Level::Info)
            e.level = autoDetect_(line);
        commit_(channel, std::move(e));
    }

    // Snapshot of entries for a channel
    std::vector<LogEntry> getChannel(const std::string& channel)
    {
        std::lock_guard<std::mutex> lk(m_);
        auto it = channels_.find(channel);
        if (it == channels_.end())
            return {};
        return {it->second.begin(), it->second.end()};
    }

    std::vector<std::string> channels() const
    {
        std::lock_guard<std::mutex> lk(m_);
        std::vector<std::string> names;
        names.reserve(channels_.size());
        for (auto& kv : channels_)
            names.push_back(kv.first);
        return names;
    }

    void clearChannel(const std::string& channel)
    {
        std::lock_guard<std::mutex> lk(m_);
        channels_[channel].clear();
    }

    void setChannelCapacity(size_t cap)
    {
        std::lock_guard<std::mutex> lk(m_);
        capacity_ = cap;
    }

    // Sinks now receive structured LogEntry
    int addSink(const Sink& s)
    {
        std::lock_guard<std::mutex> lk(m_);
        int id = ++nextSinkId_;
        sinks_.emplace_back(id, s);
        return id;
    }
    void removeSink(int id)
    {
        std::lock_guard<std::mutex> lk(m_);
        sinks_.erase(std::remove_if(sinks_.begin(), sinks_.end(),
                                    [id](auto& p)
                                    { return p.first == id; }),
                     sinks_.end());
    }
    void clearSinks()
    {
        std::lock_guard<std::mutex> lk(m_);
        sinks_.clear();
    }

    // --- std::cout / std::cerr capture (unchanged externally) ---------------
    void installStdCapture()
    {
        std::lock_guard<std::mutex> lk(m_);
        if (!coutRedirect_)
            coutRedirect_ = std::make_unique<OstreamRedirect>(std::cout, "main", Level::Debug);
        if (!cerrRedirect_)
            cerrRedirect_ = std::make_unique<OstreamRedirect>(std::cerr, "main", Level::Error);
        ensureSeed_();
    }
    void uninstallStdCapture()
    {
        std::lock_guard<std::mutex> lk(m_);
        cerrRedirect_.reset();
        coutRedirect_.reset();
    }

    // Helper to format ts once in UI (called from DebugConsole)
    static std::string formatTs(uint64_t ms)
    {
        using namespace std::chrono;
        auto tp = time_point<system_clock, milliseconds>(milliseconds(ms));
        auto t = system_clock::to_time_t(tp);
        auto ms_part = (int)(ms % 1000);
        struct tm bt;
#if defined(_WIN32)
        localtime_s(&bt, &t);
#else
        localtime_r(&t, &bt);
#endif
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d", bt.tm_hour, bt.tm_min, bt.tm_sec, ms_part);
        return std::string(buf);
    }

private:
    Logger() = default;
    // inside Logger (same file), replace LineToLoggerBuf with this version:
    class LineToLoggerBuf : public std::streambuf
    {
    public:
        // pass an optional forced level (when set, it overrides auto-detect)
        explicit LineToLoggerBuf(std::string channel, std::optional<Level> forced = std::nullopt) :
            channel_(std::move(channel)), forcedLevel_(forced) {}

    protected:
        int overflow(int ch) override
        {
            if (ch == traits_type::eof())
                return sync();
            char c = static_cast<char>(ch);
            buf_.push_back(c);
            if (c == '\n')
                flushLine_();
            return ch;
        }
        std::streamsize xsputn(const char* s, std::streamsize count) override
        {
            std::streamsize w = 0;
            for (; w < count; ++w)
            {
                char c = s[w];
                buf_.push_back(c);
                if (c == '\n')
                    flushLine_();
            }
            return w;
        }
        int sync() override
        {
            if (!buf_.empty())
            {
                if (buf_.back() == '\n')
                    buf_.pop_back();
                if (forcedLevel_)
                    Logger::instance().log(channel_, *forcedLevel_, buf_);
                else
                    Logger::instance().log(channel_, buf_); // auto-detect path
                buf_.clear();
            }
            return 0;
        }

    private:
        void flushLine_()
        {
            if (!buf_.empty() && buf_.back() == '\n')
                buf_.pop_back();
            if (forcedLevel_)
                Logger::instance().log(channel_, *forcedLevel_, buf_);
            else
                Logger::instance().log(channel_, buf_);
            buf_.clear();
        }

        std::string channel_;
        std::string buf_;
        std::optional<Level> forcedLevel_;
    };

    class OstreamRedirect
    {
    public:
        // add Level override parameter
        OstreamRedirect(std::ostream& os, const std::string& channel, std::optional<Level> forced = std::nullopt) :
            os_(os), original_(os.rdbuf()), buf_(channel, forced)
        {
            os_.rdbuf(&buf_);
        }
        ~OstreamRedirect()
        {
            if (original_)
            {
                try
                {
                    os_.rdbuf(original_);
                }
                catch (...)
                {
                }
            }
        }

        OstreamRedirect(const OstreamRedirect&) = delete;
        OstreamRedirect& operator=(const OstreamRedirect&) = delete;
        OstreamRedirect(OstreamRedirect&&) = delete;
        OstreamRedirect& operator=(OstreamRedirect&&) = delete;

    private:
        std::ostream& os_;
        std::streambuf* original_;
        LineToLoggerBuf buf_;
    };


    void ensureSeed_()
    {
        if (!seeded_)
        {
            channels_["main"];
            channels_["localtunnel"];
            seeded_ = true;
        }
    }

    static uint64_t nowMs_()
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    }

    static Level autoDetect_(const std::string& s)
    {
        // very small heuristics (case-insensitive): [error], error:, fail, warn, etc.
        auto has = [&](const char* needle)
        {
            auto it = std::search(
                s.begin(), s.end(),
                needle, needle + std::strlen(needle),
                [](char a, char b)
                { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); });
            return it != s.end();
        };
        if (has("[error]") || has(" error") || has("failed") || has("exception"))
            return Level::Error;
        if (has("[warn]") || has(" warning"))
            return Level::Warn;
        // If you wish, detect [debug] → Debug, [trace] → Trace
        if (has("[debug]"))
            return Level::Debug;
        if (has("[trace]"))
            return Level::Trace;
        return Level::Info;
    }

    void commit_(const std::string& channel, LogEntry e)
    {
        std::lock_guard<std::mutex> lk(m_);
        auto& q = channels_[channel];
        q.emplace_back(std::move(e));
        while (q.size() > capacity_)
            q.pop_front();
        // fan-out sinks
        auto& back = q.back();
        for (auto& [id, s] : sinks_)
            s(channel, back);
    }

    // data
    mutable std::mutex m_;
    size_t capacity_ = 4000;
    std::unordered_map<std::string, std::deque<LogEntry>> channels_;
    int nextSinkId_ = 0;
    std::vector<std::pair<int, Sink>> sinks_;
    bool seeded_ = false;
    std::unique_ptr<OstreamRedirect> coutRedirect_;
    std::unique_ptr<OstreamRedirect> cerrRedirect_;
};

//#pragma once
//#include <mutex>
//#include <unordered_map>
//#include <deque>
//#include <vector>
//#include <string>
//#include <functional>
//#include <algorithm>
//#include <memory>
//#include <ostream>
//
//// Thread-safe central logger with named channels (ring buffers).
//class Logger
//{
//public:
//    using Sink = std::function<void(const std::string& channel, const std::string& line)>;
//
//    static Logger& instance()
//    {
//        static Logger L;
//        return L;
//    }
//
//    // ---- Core API -----------------------------------------------------------
//
//    // Append a line to a channel.
//    void log(const std::string& channel, const std::string& line)
//    {
//        std::lock_guard<std::mutex> lk(m_);
//        auto& q = channels_[channel];
//        q.emplace_back(line);
//        while (q.size() > capacity_)
//            q.pop_front();
//        // fan-out to sinks (e.g., file, external)
//        for (auto& [id, s] : sinks_)
//            s(channel, line);
//    }
//
//    // Get snapshot of a channel lines.
//    std::vector<std::string> getChannel(const std::string& channel)
//    {
//        std::lock_guard<std::mutex> lk(m_);
//        auto it = channels_.find(channel);
//        if (it == channels_.end())
//            return {};
//        return {it->second.begin(), it->second.end()};
//    }
//
//    // List channel names (unordered).
//    std::vector<std::string> channels() const
//    {
//        std::lock_guard<std::mutex> lk(m_);
//        std::vector<std::string> names;
//        names.reserve(channels_.size());
//        for (auto& kv : channels_)
//            names.push_back(kv.first);
//        return names;
//    }
//
//    // Clear a channel’s buffer.
//    void clearChannel(const std::string& channel)
//    {
//        std::lock_guard<std::mutex> lk(m_);
//        channels_[channel].clear();
//    }
//
//    // Set ring buffer capacity for all channels.
//    void setChannelCapacity(size_t cap)
//    {
//        std::lock_guard<std::mutex> lk(m_);
//        capacity_ = cap;
//    }
//
//    // ---- Optional sinks (fan-out new lines) --------------------------------
//    int addSink(const Sink& s)
//    {
//        std::lock_guard<std::mutex> lk(m_);
//        int id = ++nextSinkId_;
//        sinks_.emplace_back(id, s);
//        return id;
//    }
//    void removeSink(int id)
//    {
//        std::lock_guard<std::mutex> lk(m_);
//        sinks_.erase(std::remove_if(sinks_.begin(), sinks_.end(),
//                                    [id](auto& p)
//                                    { return p.first == id; }),
//                     sinks_.end());
//    }
//    void clearSinks()
//    {
//        std::lock_guard<std::mutex> lk(m_);
//        sinks_.clear();
//    }
//
//    // ---- std::cout / std::cerr capture (so you don't have to call log()) ----
//    // Call once at startup: Logger::instance().installStdCapture();
//    void installStdCapture()
//    {
//        std::lock_guard<std::mutex> lk(m_);
//        if (!coutRedirect_)
//            coutRedirect_ = std::make_unique<OstreamRedirect>(std::cout, "main");
//        if (!cerrRedirect_)
//            cerrRedirect_ = std::make_unique<OstreamRedirect>(std::cerr, "main");
//        // seed channels so they show up immediately
//        ensureSeed_();
//    }
//    void uninstallStdCapture()
//    {
//        std::lock_guard<std::mutex> lk(m_);
//        cerrRedirect_.reset();
//        coutRedirect_.reset();
//    }
//
//private:
//    Logger() = default;
//
//    // --- line-buffering streambuf feeding Logger ----------------------------
//    class LineToLoggerBuf : public std::streambuf
//    {
//    public:
//        explicit LineToLoggerBuf(std::string channel) :
//            channel_(std::move(channel)) {}
//
//    protected:
//        int overflow(int ch) override
//        {
//            if (ch == traits_type::eof())
//                return sync();
//            char c = static_cast<char>(ch);
//            buf_.push_back(c);
//            if (c == '\n')
//                flushLine_();
//            return ch;
//        }
//        std::streamsize xsputn(const char* s, std::streamsize count) override
//        {
//            std::streamsize w = 0;
//            for (; w < count; ++w)
//            {
//                char c = s[w];
//                buf_.push_back(c);
//                if (c == '\n')
//                    flushLine_();
//            }
//            return w;
//        }
//        int sync() override
//        {
//            if (!buf_.empty())
//            {
//                if (buf_.back() == '\n')
//                    buf_.pop_back();
//                Logger::instance().log(channel_, buf_);
//                buf_.clear();
//            }
//            return 0;
//        }
//
//    private:
//        void flushLine_()
//        {
//            if (!buf_.empty() && buf_.back() == '\n')
//                buf_.pop_back();
//            Logger::instance().log(channel_, buf_);
//            buf_.clear();
//        }
//        std::string buf_;
//        std::string channel_;
//    };
//
//    // RAII redirect an ostream to LineToLoggerBuf
//    class OstreamRedirect
//    {
//    public:
//        OstreamRedirect(std::ostream& os, const std::string& channel) :
//            os_(os), original_(os.rdbuf()), buf_(channel)
//        {
//            os_.rdbuf(&buf_);
//        }
//
//        ~OstreamRedirect()
//        {
//            if (original_)
//            {
//                try
//                {
//                    os_.rdbuf(original_);
//                }
//                catch (...)
//                {
//                }
//            }
//        }
//
//        // non-copyable, non-movable
//        OstreamRedirect(const OstreamRedirect&) = delete;
//        OstreamRedirect& operator=(const OstreamRedirect&) = delete;
//        OstreamRedirect(OstreamRedirect&&) = delete;
//        OstreamRedirect& operator=(OstreamRedirect&&) = delete;
//
//    private:
//        std::ostream& os_;
//        std::streambuf* original_; // original streambuf to restore
//        LineToLoggerBuf buf_;      // our line-buffering streambuf
//    };
//
//    void ensureSeed_()
//    {
//        if (!seeded_)
//        {
//            channels_["main"];        // ensure exists
//            channels_["localtunnel"]; // ensure exists
//            seeded_ = true;
//        }
//    }
//
//    // data
//    mutable std::mutex m_;
//    size_t capacity_ = 4000;
//    std::unordered_map<std::string, std::deque<std::string>> channels_;
//    int nextSinkId_ = 0;
//    std::vector<std::pair<int, Sink>> sinks_;
//
//    // std capture
//    bool seeded_ = false;
//    std::unique_ptr<OstreamRedirect> coutRedirect_;
//    std::unique_ptr<OstreamRedirect> cerrRedirect_;
//};

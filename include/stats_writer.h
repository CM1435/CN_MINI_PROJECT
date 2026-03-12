#pragma once

// Windows socket headers must come first to avoid conflicts
#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <windows.h>
#endif

#include <string>
#include <fstream>
#include <mutex>
#include <deque>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>

#include "cache.h"

// ─────────────────────────────────────────────
//  RecentRequest — one logged request entry
// ─────────────────────────────────────────────
struct RecentRequest {
    std::string url;
    bool        hit;
    size_t      size;
    std::string time;
};

// ─────────────────────────────────────────────
//  StatsWriter
// ─────────────────────────────────────────────
class StatsWriter {
public:
    static StatsWriter& instance() {
        static StatsWriter inst;
        return inst;
    }

    void set_output_path(const std::string& path) {
        path_ = path;
    }

    void log_request(const std::string& url, bool hit, size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);

        std::time_t now = std::time(nullptr);
        struct tm* tm_info = std::localtime(&now);
        char buf[12];
        std::strftime(buf, sizeof(buf), "%H:%M:%S", tm_info);

        RecentRequest r;
        r.url  = url;
        r.hit  = hit;
        r.size = size;
        r.time = std::string(buf);

        recent_.push_front(r);
        if (recent_.size() > 20) {
            recent_.pop_back();
        }
    }

    void write(const CacheStats& stats,
               size_t cache_entries,
               size_t cache_bytes,
               size_t max_entries,
               size_t max_bytes,
               const std::string& policy)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        std::ofstream f(path_);
        if (!f.is_open()) return;

        f << "{\n";
        f << "  \"hits\":"                << stats.hits               << ",\n";
        f << "  \"misses\":"              << stats.misses             << ",\n";
        f << "  \"evictions\":"           << stats.evictions          << ",\n";
        f << "  \"active_connections\":"  << stats.active_connections << ",\n";
        f << "  \"bytes_saved\":"         << stats.bytes_saved        << ",\n";
        f << "  \"total_bytes_served\":"  << stats.total_bytes_served << ",\n";
        f << "  \"cache_entries\":"       << cache_entries            << ",\n";
        f << "  \"cache_bytes\":"         << cache_bytes              << ",\n";
        f << "  \"max_entries\":"         << max_entries              << ",\n";
        f << "  \"max_bytes\":"           << max_bytes                << ",\n";
        f << "  \"policy\":\""            << policy                   << "\",\n";
        f << "  \"recent_requests\": [\n";

        bool first = true;
        for (size_t i = 0; i < recent_.size(); i++) {
            const RecentRequest& r = recent_[i];
            if (!first) f << ",\n";
            f << "    {"
              << "\"url\":\""  << escape(r.url) << "\","
              << "\"hit\":"    << (r.hit ? "true" : "false") << ","
              << "\"size\":"   << r.size << ","
              << "\"time\":\"" << r.time << "\""
              << "}";
            first = false;
        }

        f << "\n  ]\n}\n";
        f.flush();
        f.close();
    }

private:
    StatsWriter() : path_("stats.json") {}

    std::string               path_;
    std::mutex                mutex_;
    std::deque<RecentRequest> recent_;

    std::string escape(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (size_t i = 0; i < s.size(); i++) {
            char c = s[i];
            if (c == '"'  ) { out += "\\\""; continue; }
            if (c == '\\' ) { out += "\\\\"; continue; }
            out += c;
        }
        return out;
    }
};

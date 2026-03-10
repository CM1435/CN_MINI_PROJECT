#pragma once
#include "cache.h"
#include <string>
#include <fstream>
#include <mutex>
#include <ctime>

// ─────────────────────────────────────────────
//  Logger — thread-safe file + console logger
// ─────────────────────────────────────────────
class Logger {
public:
    enum class Level { INFO, WARN, ERROR, DEBUG };

    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void init(const std::string& logfile, bool verbose = false);
    void log(Level level, const std::string& msg);
    void info (const std::string& msg) { log(Level::INFO,  msg); }
    void warn (const std::string& msg) { log(Level::WARN,  msg); }
    void error(const std::string& msg) { log(Level::ERROR, msg); }
    void debug(const std::string& msg) { if(verbose_) log(Level::DEBUG, msg); }

private:
    Logger() = default;
    std::ofstream  file_;
    std::mutex     mutex_;
    bool           verbose_ = false;

    std::string timestamp();
    std::string level_str(Level l);
};

// ─────────────────────────────────────────────
//  StatsDisplay — prints live stats to terminal
// ─────────────────────────────────────────────
class StatsDisplay {
public:
    static void print_banner();
    static void print_stats(const CacheStats& stats, size_t cache_entries, size_t cache_bytes);
    static void print_separator();
    static std::string format_bytes(size_t bytes);
};

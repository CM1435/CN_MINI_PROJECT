#include "../include/logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>

// ══════════════════════════════════════════════
//  Logger
// ══════════════════════════════════════════════

void Logger::init(const std::string& logfile, bool verbose) {
    verbose_ = verbose;
    file_.open(logfile, std::ios::app);
    if (!file_.is_open()) {
        std::cerr << "[Logger] WARNING: Could not open log file: " << logfile << "\n";
    }
}

void Logger::log(Level level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string line = "[" + timestamp() + "] [" + level_str(level) + "] " + msg;
    std::cout << line << "\n";
    if (file_.is_open()) file_ << line << "\n" << std::flush;
}

std::string Logger::timestamp() {
    auto now   = std::time(nullptr);
    auto* tm   = std::localtime(&now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    return std::string(buf);
}

std::string Logger::level_str(Level l) {
    switch (l) {
        case Level::INFO:  return " INFO";
        case Level::WARN:  return " WARN";
        case Level::ERROR: return "ERROR";
        case Level::DEBUG: return "DEBUG";
        default:           return "     ";
    }
}

// ══════════════════════════════════════════════
//  StatsDisplay
// ══════════════════════════════════════════════

void StatsDisplay::print_banner() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════╗\n";
    std::cout << "║       HTTP Proxy Cache Server  —  C++            ║\n";
    std::cout << "║       LRU / LFU  |  Multi-Threaded               ║\n";
    std::cout << "╚══════════════════════════════════════════════════╝\n\n";
}

void StatsDisplay::print_stats(const CacheStats& s, size_t entries, size_t bytes) {
    std::cout << "\n┌─────────────────────── CACHE STATS ───────────────────────┐\n";
    std::cout << "│  Hits:              " << std::setw(10) << s.hits           << "                          │\n";
    std::cout << "│  Misses:            " << std::setw(10) << s.misses         << "                          │\n";
    std::cout << "│  Hit Ratio:         " << std::setw(9)  << std::fixed
              << std::setprecision(1)     << s.hit_ratio() << "%                         │\n";
    std::cout << "│  Evictions:         " << std::setw(10) << s.evictions      << "                          │\n";
    std::cout << "│  Bandwidth Saved:   " << std::setw(10) << format_bytes(s.bytes_saved) << "                  │\n";
    std::cout << "│  Active Conns:      " << std::setw(10) << s.active_connections << "                       │\n";
    std::cout << "│  Cache Entries:     " << std::setw(10) << entries          << "                          │\n";
    std::cout << "│  Cache Size:        " << std::setw(10) << format_bytes(bytes) << "                     │\n";
    std::cout << "└────────────────────────────────────────────────────────────┘\n";
}

void StatsDisplay::print_separator() {
    std::cout << "────────────────────────────────────────────────────────────\n";
}

std::string StatsDisplay::format_bytes(size_t bytes) {
    std::ostringstream oss;
    if (bytes >= 1024 * 1024)
        oss << std::fixed << std::setprecision(2) << (bytes / (1024.0 * 1024.0)) << " MB";
    else if (bytes >= 1024)
        oss << std::fixed << std::setprecision(1) << (bytes / 1024.0) << " KB";
    else
        oss << bytes << " B";
    return oss.str();
}

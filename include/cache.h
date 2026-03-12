#pragma once
#include <string>
#include <unordered_map>
#include <list>
#include <mutex>
#include <chrono>
#include <vector>
#include <iostream>

// ─────────────────────────────────────────────
//  CacheEntry — one cached HTTP response
// ─────────────────────────────────────────────
struct CacheEntry {
    std::string url;
    std::string response;       // raw HTTP response bytes
    size_t      size;           // bytes
    int         frequency;      // for LFU
    int         ttl_seconds;    // time-to-live
    std::chrono::steady_clock::time_point inserted_at;
    std::chrono::steady_clock::time_point last_accessed;

    CacheEntry() : size(0), frequency(1), ttl_seconds(300) {}

    CacheEntry(const std::string& u, const std::string& r, int ttl = 300)
        : url(u), response(r), size(r.size()), frequency(1), ttl_seconds(ttl),
          inserted_at(std::chrono::steady_clock::now()),
          last_accessed(std::chrono::steady_clock::now()) {}

    bool is_expired() const {
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - inserted_at).count();
        return age >= ttl_seconds;
    }
};

// ─────────────────────────────────────────────
//  CacheStats — runtime statistics
// ─────────────────────────────────────────────
struct CacheStats {
    long long hits        = 0;
    long long misses      = 0;
    long long evictions   = 0;
    long long total_bytes_served  = 0;
    long long bytes_saved         = 0;   // bytes served from cache (not fetched)
    int       active_connections  = 0;

    double hit_ratio() const {
        long long total = hits + misses;
        return total == 0 ? 0.0 : (100.0 * hits / total);
    }
};

// ─────────────────────────────────────────────
//  Eviction Policy enum
// ─────────────────────────────────────────────
enum class EvictionPolicy { LRU, LFU };

// ─────────────────────────────────────────────
//  LRUCache
// ─────────────────────────────────────────────
class LRUCache {
public:
    explicit LRUCache(size_t max_entries, size_t max_bytes, CacheStats& stats)
        : max_entries_(max_entries), max_bytes_(max_bytes),
          current_bytes_(0), stats_(stats) {}

    bool get(const std::string& url, std::string& out_response);
    void put(const std::string& url, const std::string& response, int ttl = 300);
    void evict_expired();
    size_t size() const { return map_.size(); }
    size_t bytes() const { return current_bytes_; }
    void   print_contents() const;

private:
    size_t max_entries_;
    size_t max_bytes_;
    size_t current_bytes_;

    // list front = most recently used
    std::list<CacheEntry>                                   lru_list_;
    std::unordered_map<std::string, std::list<CacheEntry>::iterator> map_;

    CacheStats& stats_;
    mutable std::mutex mutex_;

    void evict_one();
};

// ─────────────────────────────────────────────
//  LFUCache
// ─────────────────────────────────────────────
class LFUCache {
public:
    explicit LFUCache(size_t max_entries, size_t max_bytes, CacheStats& stats)
        : max_entries_(max_entries), max_bytes_(max_bytes),
          current_bytes_(0), min_freq_(0), stats_(stats) {}

    bool get(const std::string& url, std::string& out_response);
    void put(const std::string& url, const std::string& response, int ttl = 300);
    void evict_expired();
    size_t size() const { return key_map_.size(); }
    size_t bytes() const { return current_bytes_; }
    void   print_contents() const;

private:
    size_t max_entries_;
    size_t max_bytes_;
    size_t current_bytes_;
    int    min_freq_;

    struct Node {
        CacheEntry entry;
        std::list<std::string>::iterator it;
    };

    std::unordered_map<std::string, Node>                      key_map_;
    std::unordered_map<int, std::list<std::string>>            freq_map_;

    CacheStats& stats_;
    mutable std::mutex mutex_;

    void increment_freq(const std::string& url);
    void evict_one();
};

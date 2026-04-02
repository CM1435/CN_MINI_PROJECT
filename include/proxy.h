#pragma once
#include "cache.h"
#include <string>
#include <atomic>
#include <set>

// ─────────────────────────────────────────────
//  ProxyConfig — runtime configuration
// ─────────────────────────────────────────────
struct ProxyConfig {
    int           port           = 8080;
    size_t        max_entries    = 500;
    size_t        max_bytes      = 50 * 1024 * 1024;  // 50 MB
    int           default_ttl    = 300;               // seconds
    EvictionPolicy policy        = EvictionPolicy::LRU;
    bool          verbose        = false;
    std::set<std::string> blacklist;                  // blocked domains
};

// ─────────────────────────────────────────────
//  HttpRequest — parsed HTTP request
// ─────────────────────────────────────────────
struct HttpRequest {
    std::string method;
    std::string url;
    std::string host;
    int         port = 80;
    std::string path;
    std::string raw;
    bool        valid = false;
};

// ─────────────────────────────────────────────
//  ProxyServer
// ─────────────────────────────────────────────
class ProxyServer {
public:
    explicit ProxyServer(const ProxyConfig& cfg);
    ~ProxyServer();

    void start();    // blocking — runs accept loop
    void stop();

    const CacheStats& stats() const { return stats_; }

    // connection handler (runs in its own thread — public for pthread access)
    void handle_client(int client_fd);

private:
    ProxyConfig   cfg_;
    CacheStats    stats_;
    LRUCache      lru_cache_;
    LFUCache      lfu_cache_;
    HybridCache   hybrid_cache_;
    int           server_fd_ = -1;
    std::atomic<bool> running_{false};

    // HTTP helpers
    HttpRequest   parse_request(const std::string& raw);
    std::string   fetch_from_origin(const HttpRequest& req);
    int           parse_ttl_from_headers(const std::string& response);
    bool          is_cacheable(const HttpRequest& req, const std::string& response);
    bool          is_blacklisted(const std::string& host);

    // cache dispatch (LRU or LFU based on config)
    bool cache_get(const std::string& url, std::string& out);
    void cache_put(const std::string& url, const std::string& resp, int ttl);

    // socket helpers
    int  create_server_socket();
    int  connect_to_host(const std::string& host, int port);
    void send_all(int fd, const std::string& data);
    std::string recv_all(int fd);
    std::string recv_http_response(int fd);

    // logging
    void log(const std::string& msg) const;
    void log_request(const HttpRequest& req, bool cache_hit, size_t bytes) const;
};
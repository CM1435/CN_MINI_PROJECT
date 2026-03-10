#include "../include/proxy.h"
#include "../include/logger.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <errno.h>

#include <cstring>
#include <sstream>
#include <thread>
#include <regex>
#include <algorithm>

#define RECV_BUFFER   8192
#define CONNECT_TIMEOUT 10

// ── thread args ──────────────────────────────
struct ClientArgs {
    ProxyServer* server;
    int          fd;
};

static void* thread_entry(void* arg) {
    auto* ca = static_cast<ClientArgs*>(arg);
    ca->server->handle_client(ca->fd);
    delete ca;
    return nullptr;
}

// ══════════════════════════════════════════════
//  ProxyServer
// ══════════════════════════════════════════════

ProxyServer::ProxyServer(const ProxyConfig& cfg)
    : cfg_(cfg),
      lru_cache_(cfg.max_entries, cfg.max_bytes, stats_),
      lfu_cache_(cfg.max_entries, cfg.max_bytes, stats_) {}

ProxyServer::~ProxyServer() {
    stop();
}

void ProxyServer::stop() {
    running_ = false;
    if (server_fd_ != -1) { close(server_fd_); server_fd_ = -1; }
}

void ProxyServer::start() {
    server_fd_ = create_server_socket();
    if (server_fd_ < 0) {
        Logger::instance().error("Failed to create server socket");
        return;
    }

    running_ = true;
    StatsDisplay::print_banner();
    Logger::instance().info("Proxy listening on port " + std::to_string(cfg_.port));
    Logger::instance().info("Policy: " + std::string(cfg_.policy == EvictionPolicy::LRU ? "LRU" : "LFU"));
    Logger::instance().info("Max entries: " + std::to_string(cfg_.max_entries) +
                            " | Max size: " + StatsDisplay::format_bytes(cfg_.max_bytes));

    // periodic TTL cleanup thread
    std::thread([this]() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            lru_cache_.evict_expired();
            lfu_cache_.evict_expired();
            Logger::instance().debug("TTL cleanup pass done");
        }
    }).detach();

    // periodic stats print
    std::thread([this]() {
        while (running_) {
            std::this_thread::sleep_for(std::chrono::seconds(15));
            size_t entries = (cfg_.policy == EvictionPolicy::LRU)
                             ? lru_cache_.size() : lfu_cache_.size();
            size_t bytes   = (cfg_.policy == EvictionPolicy::LRU)
                             ? lru_cache_.bytes() : lfu_cache_.bytes();
            StatsDisplay::print_stats(stats_, entries, bytes);
        }
    }).detach();

    // main accept loop
    while (running_) {
        sockaddr_in client_addr{};
        socklen_t   client_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_len);

        if (client_fd < 0) {
            if (running_) Logger::instance().warn("accept() failed: " + std::string(strerror(errno)));
            continue;
        }

        stats_.active_connections++;
        auto* args  = new ClientArgs{ this, client_fd };
        pthread_t tid;
        pthread_create(&tid, nullptr, thread_entry, args);
        pthread_detach(tid);
    }
}

// ─────────────────────────────────────────────
//  handle_client — runs per connection thread
// ─────────────────────────────────────────────
void ProxyServer::handle_client(int client_fd) {
    // set socket timeout
    struct timeval tv { CONNECT_TIMEOUT, 0 };
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    std::string raw = recv_all(client_fd);
    if (raw.empty()) {
        close(client_fd);
        stats_.active_connections--;
        return;
    }

    HttpRequest req = parse_request(raw);
    if (!req.valid) {
        Logger::instance().warn("Invalid/unparseable request — dropping");
        close(client_fd);
        stats_.active_connections--;
        return;
    }

    if (is_blacklisted(req.host)) {
        std::string blocked =
            "HTTP/1.1 403 Forbidden\r\n"
            "Content-Type: text/html\r\n\r\n"
            "<h1>403 Forbidden</h1><p>Domain blocked by proxy.</p>";
        send_all(client_fd, blocked);
        Logger::instance().warn("BLOCKED: " + req.host);
        close(client_fd);
        stats_.active_connections--;
        return;
    }

    // only cache GET
    std::string response;
    bool hit = false;

    if (req.method == "GET") {
        hit = cache_get(req.url, response);
    }

    if (!hit) {
        response = fetch_from_origin(req);
        if (response.empty()) {
            std::string err =
                "HTTP/1.1 502 Bad Gateway\r\n"
                "Content-Type: text/html\r\n\r\n"
                "<h1>502 Bad Gateway</h1><p>Could not reach origin server.</p>";
            send_all(client_fd, err);
            close(client_fd);
            stats_.active_connections--;
            return;
        }
        if (req.method == "GET" && is_cacheable(req, response)) {
            int ttl = parse_ttl_from_headers(response);
            cache_put(req.url, response, ttl);
        }
    }

    log_request(req, hit, response.size());
    stats_.total_bytes_served += response.size();
    send_all(client_fd, response);
    close(client_fd);
    stats_.active_connections--;
}

// ─────────────────────────────────────────────
//  HTTP Parsing
// ─────────────────────────────────────────────
HttpRequest ProxyServer::parse_request(const std::string& raw) {
    HttpRequest req;
    req.raw = raw;

    std::istringstream stream(raw);
    std::string line;
    if (!std::getline(stream, line)) return req;

    // e.g. "GET http://example.com/path HTTP/1.1"
    std::istringstream first_line(line);
    std::string url_str;
    first_line >> req.method >> url_str;
    if (req.method.empty() || url_str.empty()) return req;

    // strip http://
    if (url_str.substr(0, 7) == "http://")  url_str = url_str.substr(7);
    if (url_str.substr(0, 8) == "https://") url_str = url_str.substr(8);

    auto slash = url_str.find('/');
    std::string host_port = (slash != std::string::npos) ? url_str.substr(0, slash) : url_str;
    req.path = (slash != std::string::npos) ? url_str.substr(slash) : "/";

    auto colon = host_port.find(':');
    if (colon != std::string::npos) {
        req.host = host_port.substr(0, colon);
        try { req.port = std::stoi(host_port.substr(colon + 1)); }
        catch (...) { req.port = 80; }
    } else {
        req.host = host_port;
        req.port = 80;
    }

    // parse Host: header as fallback
    while (std::getline(stream, line)) {
        if (line.find("Host:") == 0 || line.find("host:") == 0) {
            auto val = line.substr(line.find(':') + 1);
            // trim
            val.erase(0, val.find_first_not_of(" \t\r\n"));
            val.erase(val.find_last_not_of(" \t\r\n") + 1);
            if (req.host.empty()) req.host = val;
        }
    }

    if (req.host.empty()) return req;
    req.url   = "http://" + req.host + req.path;
    req.valid = true;
    return req;
}

// ─────────────────────────────────────────────
//  Origin fetch
// ─────────────────────────────────────────────
std::string ProxyServer::fetch_from_origin(const HttpRequest& req) {
    int sock = connect_to_host(req.host, req.port);
    if (sock < 0) {
        Logger::instance().error("Cannot connect to " + req.host + ":" + std::to_string(req.port));
        return "";
    }

    // rebuild a clean request to forward
    std::string forward =
        req.method + " " + req.path + " HTTP/1.0\r\n" +
        "Host: " + req.host + "\r\n" +
        "Connection: close\r\n" +
        "User-Agent: ProxyCache/1.0\r\n\r\n";

    send_all(sock, forward);
    std::string response = recv_http_response(sock);
    close(sock);
    return response;
}

// ─────────────────────────────────────────────
//  Parse Cache-Control / Expires TTL
// ─────────────────────────────────────────────
int ProxyServer::parse_ttl_from_headers(const std::string& response) {
    // look for Cache-Control: max-age=N
    std::regex cc_regex("Cache-Control:.*?max-age=(\\d+)", std::regex::icase);
    std::smatch match;
    if (std::regex_search(response, match, cc_regex)) {
        try { return std::stoi(match[1].str()); }
        catch (...) {}
    }
    return cfg_.default_ttl;
}

// ─────────────────────────────────────────────
//  Cacheability check
// ─────────────────────────────────────────────
bool ProxyServer::is_cacheable(const HttpRequest& req, const std::string& response) {
    if (req.method != "GET") return false;
    // don't cache if no-store or no-cache
    if (response.find("no-store") != std::string::npos) return false;
    if (response.find("no-cache") != std::string::npos) return false;
    // only cache 200 OK responses
    if (response.find("HTTP/1.0 200") == std::string::npos &&
        response.find("HTTP/1.1 200") == std::string::npos) return false;
    return true;
}

bool ProxyServer::is_blacklisted(const std::string& host) {
    for (const auto& domain : cfg_.blacklist) {
        if (host.find(domain) != std::string::npos) return true;
    }
    return false;
}

// ─────────────────────────────────────────────
//  Cache dispatch
// ─────────────────────────────────────────────
bool ProxyServer::cache_get(const std::string& url, std::string& out) {
    if (cfg_.policy == EvictionPolicy::LRU) return lru_cache_.get(url, out);
    return lfu_cache_.get(url, out);
}

void ProxyServer::cache_put(const std::string& url, const std::string& resp, int ttl) {
    if (cfg_.policy == EvictionPolicy::LRU) lru_cache_.put(url, resp, ttl);
    else lfu_cache_.put(url, resp, ttl);
}

// ─────────────────────────────────────────────
//  Socket helpers
// ─────────────────────────────────────────────
int ProxyServer::create_server_socket() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(cfg_.port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) { close(fd); return -1; }
    if (listen(fd, 128) < 0) { close(fd); return -1; }
    return fd;
}

int ProxyServer::connect_to_host(const std::string& host, int port) {
    addrinfo hints{}, *res = nullptr;
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0)
        return -1;

    int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) { freeaddrinfo(res); return -1; }

    struct timeval tv { CONNECT_TIMEOUT, 0 };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (connect(sock, res->ai_addr, res->ai_addrlen) < 0) {
        close(sock); freeaddrinfo(res); return -1;
    }
    freeaddrinfo(res);
    return sock;
}

void ProxyServer::send_all(int fd, const std::string& data) {
    size_t total = 0;
    while (total < data.size()) {
        ssize_t sent = send(fd, data.c_str() + total, data.size() - total, MSG_NOSIGNAL);
        if (sent <= 0) break;
        total += sent;
    }
}

std::string ProxyServer::recv_all(int fd) {
    std::string result;
    char buf[RECV_BUFFER];
    ssize_t n;
    while ((n = recv(fd, buf, sizeof(buf), 0)) > 0) {
        result.append(buf, n);
        // stop at end of HTTP request headers
        if (result.find("\r\n\r\n") != std::string::npos) break;
    }
    return result;
}

std::string ProxyServer::recv_http_response(int fd) {
    std::string result;
    char buf[RECV_BUFFER];
    ssize_t n;
    while ((n = recv(fd, buf, sizeof(buf), 0)) > 0) {
        result.append(buf, n);
    }
    return result;
}

// ─────────────────────────────────────────────
//  Logging
// ─────────────────────────────────────────────
void ProxyServer::log(const std::string& msg) const {
    Logger::instance().info(msg);
}

void ProxyServer::log_request(const HttpRequest& req, bool hit, size_t bytes) const {
    std::string status = hit ? "[HIT] " : "[MISS]";
    std::string msg    = status + " " + req.method + " " + req.url +
                         " — " + StatsDisplay::format_bytes(bytes);
    Logger::instance().info(msg);
}

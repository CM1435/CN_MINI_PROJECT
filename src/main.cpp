#include "../include/proxy.h"
#include "../include/logger.h"
#include <iostream>
#include <csignal>
#include <string>

// ─────────────────────────────────────────────
//  Global proxy pointer for signal handler
// ─────────────────────────────────────────────
static ProxyServer* g_proxy = nullptr;

void signal_handler(int sig) {
    std::cout << "\n[SIGNAL] Caught signal " << sig << " — shutting down...\n";
    if (g_proxy) g_proxy->stop();
    std::exit(0);
}

// ─────────────────────────────────────────────
//  Usage
// ─────────────────────────────────────────────
void print_usage(const char* prog) {
    std::cout << "\nUsage: " << prog << " [OPTIONS]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -p <port>         Port to listen on          (default: 8080)\n";
    std::cout << "  -e <entries>      Max cache entries           (default: 500)\n";
    std::cout << "  -m <MB>           Max cache size in MB        (default: 50)\n";
    std::cout << "  -t <seconds>      Default TTL in seconds      (default: 300)\n";
    std::cout << "  -P <lru|lfu>      Eviction policy             (default: lru)\n";
    std::cout << "  -b <domain>       Blacklist a domain          (repeatable)\n";
    std::cout << "  -v                Verbose / debug logging\n";
    std::cout << "  -h                Show this help\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog << " -p 8080 -P lru -m 100 -t 600\n";
    std::cout << "  " << prog << " -p 3128 -P lfu -b ads.example.com -v\n\n";
}

// ─────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────
int main(int argc, char* argv[]) {
    ProxyConfig cfg;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }
        else if (arg == "-p" && i + 1 < argc) {
            cfg.port = std::stoi(argv[++i]);
        }
        else if (arg == "-e" && i + 1 < argc) {
            cfg.max_entries = std::stoul(argv[++i]);
        }
        else if (arg == "-m" && i + 1 < argc) {
            cfg.max_bytes = std::stoul(argv[++i]) * 1024 * 1024;
        }
        else if (arg == "-t" && i + 1 < argc) {
            cfg.default_ttl = std::stoi(argv[++i]);
        }
        else if (arg == "-P" && i + 1 < argc) {
            std::string pol = argv[++i];
            cfg.policy = (pol == "lfu") ? EvictionPolicy::LFU : EvictionPolicy::LRU;
        }
        else if (arg == "-b" && i + 1 < argc) {
            cfg.blacklist.insert(argv[++i]);
        }
        else if (arg == "-v") {
            cfg.verbose = true;
        }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }

    // init logger
    Logger::instance().init("proxy.log", cfg.verbose);

    // signal handling (no SIGPIPE on Windows)
    std::signal(SIGINT,  signal_handler);
    std::signal(SIGTERM, signal_handler);

    // start proxy
    ProxyServer proxy(cfg);
    g_proxy = &proxy;
    proxy.start();

    return 0;
}

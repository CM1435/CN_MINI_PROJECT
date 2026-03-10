# HTTP Proxy Cache Server — C++

A **multi-threaded HTTP proxy cache server** built in C++17.  
Supports **LRU** and **LFU** eviction policies, TTL-based expiration, domain blacklisting, and live stats.

---

## Project Structure

```
proxy_cache/
├── include/
│   ├── cache.h       — LRUCache, LFUCache, CacheEntry, CacheStats
│   ├── proxy.h       — ProxyServer, HttpRequest, ProxyConfig
│   └── logger.h      — Logger, StatsDisplay
├── src/
│   ├── cache.cpp     — LRU + LFU implementations
│   ├── proxy.cpp     — Proxy server, HTTP parsing, socket I/O
│   ├── logger.cpp    — Thread-safe logger + stats display
│   └── main.cpp      — CLI entry point
├── scripts/
│   └── bench.py      — Python benchmark script
└── Makefile
```

---

## Build

```bash
make          # release build
make debug    # debug build with symbols
make clean    # remove binary and log
```

Requires: **g++ (C++17)**, **pthreads**, Linux/macOS

---

## Run

```bash
# Basic — LRU, port 8080
./proxy_cache -p 8080 -P lru -v

# LFU, 100MB cache, 10 min TTL
./proxy_cache -p 8080 -P lfu -m 100 -t 600

# With domain blacklist
./proxy_cache -p 8080 -b ads.example.com -b tracker.io -v
```

### All Options

| Flag | Description | Default |
|------|-------------|---------|
| `-p <port>` | Port to listen on | 8080 |
| `-e <N>` | Max cached entries | 500 |
| `-m <MB>` | Max cache size in MB | 50 |
| `-t <sec>` | Default TTL (seconds) | 300 |
| `-P lru\|lfu` | Eviction policy | lru |
| `-b <domain>` | Blacklist domain (repeatable) | — |
| `-v` | Verbose/debug logging | off |

---

## Configure Browser / curl

Point your HTTP proxy to `localhost:8080`:

```bash
# curl
curl -x http://localhost:8080 http://example.com/

# wget
wget -e use_proxy=yes -e http_proxy=localhost:8080 http://example.com/
```

For browser: set HTTP proxy to `127.0.0.1` port `8080` in network settings.

---

## Benchmark

```bash
# Start proxy first, then in another terminal:
python3 scripts/bench.py --proxy localhost:8080 --rounds 5
```

---

## CN Concepts Demonstrated

| Concept | Where |
|---------|-------|
| TCP Socket Programming | `proxy.cpp` — `create_server_socket`, `connect_to_host` |
| HTTP/1.x Parsing | `proxy.cpp` — `parse_request` |
| Proxy Architecture | `proxy.cpp` — `handle_client`, `fetch_from_origin` |
| LRU Eviction | `cache.cpp` — `LRUCache` |
| LFU Eviction | `cache.cpp` — `LFUCache` |
| TTL / Cache Expiry | `cache.h` — `CacheEntry::is_expired()` |
| Multi-threading (pthreads) | `proxy.cpp` — `thread_entry`, `pthread_create` |
| Mutex Synchronization | `cache.cpp` — `std::mutex` in LRU/LFU |
| Cache-Control Headers | `proxy.cpp` — `parse_ttl_from_headers` |
| Bandwidth Tracking | `cache.h` — `CacheStats::bytes_saved` |

---

## Team Division

| Member | Files |
|--------|-------|
| 1 — Socket + HTTP | `proxy.cpp`, `proxy.h` |
| 2 — LRU Cache | `cache.cpp` (LRU part), `cache.h` |
| 3 — LFU + Threading | `cache.cpp` (LFU part), `main.cpp` |
| 4 — Logger + Bench | `logger.cpp`, `bench.py`, `README` |

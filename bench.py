#!/usr/bin/env python3
"""
bench.py — Benchmark script for HTTP Proxy Cache
Tests cache hit ratio and latency improvement.

Usage:
    python3 bench.py [--proxy localhost:8080] [--rounds 5]
"""

import urllib.request
import time
import argparse
import statistics
import sys

# URLs to benchmark (HTTP only for basic proxy)
TEST_URLS = [
    "http://example.com/",
    "http://httpbin.org/get",
    "http://httpbin.org/headers",
    "http://info.cern.ch/",
    "http://neverssl.com/",
]

def fetch_via_proxy(url: str, proxy_addr: str) -> tuple[float, int]:
    """Return (latency_ms, bytes_received)"""
    proxy_handler = urllib.request.ProxyHandler({"http": f"http://{proxy_addr}"})
    opener = urllib.request.build_opener(proxy_handler)
    start = time.perf_counter()
    try:
        with opener.open(url, timeout=10) as resp:
            data = resp.read()
        elapsed = (time.perf_counter() - start) * 1000
        return elapsed, len(data)
    except Exception as e:
        print(f"  [ERROR] {url}: {e}")
        return -1, 0

def run_benchmark(proxy_addr: str, rounds: int):
    print(f"\n{'='*60}")
    print(f"  HTTP Proxy Cache Benchmark")
    print(f"  Proxy: {proxy_addr} | Rounds: {rounds}")
    print(f"{'='*60}\n")

    results = {}

    for url in TEST_URLS:
        print(f"Testing: {url}")
        latencies_cold = []
        latencies_warm = []

        for r in range(rounds):
            # First request — likely a cache miss
            lat, sz = fetch_via_proxy(url, proxy_addr)
            if lat > 0:
                latencies_cold.append(lat)
                print(f"  Round {r+1} cold: {lat:.1f} ms  ({sz} bytes)")

            # Second request immediately — should be a cache hit
            lat2, sz2 = fetch_via_proxy(url, proxy_addr)
            if lat2 > 0:
                latencies_warm.append(lat2)
                print(f"  Round {r+1} warm: {lat2:.1f} ms  (cache hit expected)")

        results[url] = {
            "cold_avg": statistics.mean(latencies_cold) if latencies_cold else 0,
            "warm_avg": statistics.mean(latencies_warm) if latencies_warm else 0,
        }
        print()

    # Summary
    print(f"\n{'='*60}")
    print(f"  RESULTS SUMMARY")
    print(f"{'='*60}")
    print(f"{'URL':<40} {'Cold (ms)':>10} {'Warm (ms)':>10} {'Speedup':>10}")
    print(f"{'-'*70}")

    total_cold = []
    total_warm = []
    for url, r in results.items():
        short_url = url[:38] + ".." if len(url) > 40 else url
        speedup = (r["cold_avg"] / r["warm_avg"]) if r["warm_avg"] > 0 else float("inf")
        print(f"{short_url:<40} {r['cold_avg']:>10.1f} {r['warm_avg']:>10.1f} {speedup:>9.1f}x")
        if r["cold_avg"] > 0: total_cold.append(r["cold_avg"])
        if r["warm_avg"] > 0: total_warm.append(r["warm_avg"])

    print(f"{'-'*70}")
    if total_cold and total_warm:
        avg_cold    = statistics.mean(total_cold)
        avg_warm    = statistics.mean(total_warm)
        avg_speedup = avg_cold / avg_warm if avg_warm else 0
        saved_pct   = (1 - avg_warm / avg_cold) * 100 if avg_cold else 0
        print(f"{'AVERAGE':<40} {avg_cold:>10.1f} {avg_warm:>10.1f} {avg_speedup:>9.1f}x")
        print(f"\n  ✅ Cache reduced latency by {saved_pct:.1f}% on average")
    print(f"{'='*60}\n")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Proxy Cache Benchmark")
    parser.add_argument("--proxy",  default="localhost:8080", help="Proxy address (host:port)")
    parser.add_argument("--rounds", type=int, default=3, help="Number of rounds per URL")
    args = parser.parse_args()
    run_benchmark(args.proxy, args.rounds)

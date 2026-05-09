#!/bin/bash

# CONFIGURATION: Set this to a number smaller than the unique URLs (10)
# We use 6 to force evictions and prove Hybrid is smarter.
CACHE_SIZE=6

echo "⚙️  Recompiling project..."
make clean > /dev/null 2>&1 && make > /dev/null 2>&1

echo "🧪 Starting The Pure Bash Benchmark (Capacity: $CACHE_SIZE)..."

for policy in lru lfu hybrid; do
    POLICY_UPPER=$(echo "$policy" | tr '[:lower:]' '[:upper:]')
    echo -e "\n=================================================="
    echo "🚀 RUNNING TEST: $POLICY_UPPER CACHE"
    echo "=================================================="
    
    killall -9 proxy_cache > /dev/null 2>&1
    sleep 2 
    
    # Passing -e $CACHE_SIZE via command line to override the header default
    ./proxy_cache -p 8080 -P $policy -e $CACHE_SIZE > result_${policy}.txt 2>&1 &
    PROXY_PID=$!
    sleep 2 

    echo "  -> Running 30 deterministic network requests..."
    
    fetch() {
        curl -s -m 2 -x http://127.0.0.1:8080 "http://example.com/?q=$1" > /dev/null
    }

    # Phase 1: VIPs (10 requests) 
    # Hybrid graduates these to LFU VIP Tier (Views: 5 each)
    for i in {1..5}; do fetch "vip1"; fetch "vip2"; done
    
    # Phase 2: Junk (8 requests)
    # High frequency, but "yesterday's news". Standard LFU gets stuck on these.
    for i in {1..4}; do fetch "junk1"; fetch "junk2"; done
    
    # Phase 3: Bot Attack (4 requests)
    # This flushes a small LRU cache completely.
    for i in {1..4}; do fetch "bot$i"; done
    
    # Phase 4: Trends (4 requests)
    for i in {1..2}; do fetch "trend1"; fetch "trend2"; done
    
    # Phase 5: Final Exam (4 requests)
    # We test if the cache still remembers the VIPs and the Trends
    fetch "vip1"
    fetch "vip2"
    fetch "trend1"
    fetch "trend2"
    
    echo "⏳ Traffic complete! Waiting for final stats sync..."
    sleep 12 

    # Graceful shutdown to flush the buffer
    kill -INT $PROXY_PID > /dev/null 2>&1
    wait $PROXY_PID 2>/dev/null

    echo "📊 FINAL RESULTS FOR $POLICY_UPPER:"
    grep -A 12 "+---------------------- CACHE STATS" result_${policy}.txt | tail -n 13
done

echo -e "\n✅ Comparison Complete!"
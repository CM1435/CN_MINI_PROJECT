#!/bin/bash

echo "⚙️  Recompiling project with Cache Size = 4..."
make clean > /dev/null 2>&1 && make > /dev/null 2>&1

echo "🧪 Starting The Pure Bash Benchmark..."

for policy in lru lfu hybrid; do
    POLICY_UPPER=$(echo "$policy" | tr '[:lower:]' '[:upper:]')
    echo -e "\n=================================================="
    echo "🚀 RUNNING TEST: $POLICY_UPPER CACHE"
    echo "=================================================="
    
    # 1. Clear ports and start proxy
    killall -9 proxy_cache > /dev/null 2>&1
    sleep 2 
    
    # Note: Starting WITHOUT the -v flag so logs don't bury the table
    ./proxy_cache -p 8080 -P $policy > result_${policy}.txt 2>&1 &
    PROXY_PID=$!
    sleep 2 

    echo "  -> Running 30 deterministic network requests..."
    
    # Helper function: strictly fetch without following redirects
    fetch() {
        curl -s -m 2 -x http://127.0.0.1:8080 "http://example.com/?q=$1" > /dev/null
    }

    # Phase 1: VIP (10 requests) - Hybrid promotes to LFU
    for i in {1..5}; do fetch "vip1"; fetch "vip2"; done
    # Phase 2: Junk (8 requests) - Pollutes standard LFU
    for i in {1..4}; do fetch "junk1"; fetch "junk2"; done
    # Phase 3: Bots (4 requests) - Flushes standard LRU
    for i in {1..4}; do fetch "bot$i"; done
    # Phase 4: Trends (4 requests) - Ignored by standard LFU
    for i in {1..2}; do fetch "trend1"; fetch "trend2"; done
    # Phase 5: Final Exam (4 requests)
    fetch "vip1"; fetch "vip2"; fetch "trend1"; fetch "trend2"
    
    # Wait 10 seconds to ensure the proxy's 5-second print timer fires twice
    echo "⏳ Traffic complete! Waiting for final stats sync..."
    sleep 10 

    echo "📊 FINAL RESULTS FOR $POLICY_UPPER:"
    # Grab the table reliably without chopping the bottom off
    grep -A 12 "+---------------------- CACHE STATS" result_${policy}.txt | tail -n 13

    # Clean shutdown
    kill -9 $PROXY_PID > /dev/null 2>&1
    wait $PROXY_PID 2>/dev/null
done

echo -e "\n✅ Comparison Complete!"
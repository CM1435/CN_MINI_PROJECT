from collections import OrderedDict, defaultdict
import time

# ==========================================
# 1. LRU Cache (Least Recently Used)
# ==========================================
class LRUCache:
    def __init__(self, capacity: int):
        self.capacity = capacity
        # OrderedDict acts as BOTH the Hash Map and the Linked List
        self.cache = OrderedDict()
        self.hits = 0
        self.misses = 0

    def get(self, url: str) -> str:
        if url not in self.cache:
            self.misses += 1
            return None
        
        # HIT: Move to the front of the list (Right side in Python)
        self.cache.move_to_end(url)
        self.hits += 1
        return self.cache[url]

    def put(self, url: str, data: str):
        if url in self.cache:
            self.cache.move_to_end(url)
        
        self.cache[url] = data
        
        # EVICT: If full, pop from the back of the list (Left side in Python)
        if len(self.cache) > self.capacity:
            evicted_url, _ = self.cache.popitem(last=False)
            print(f"      [LRU Evicted] -> {evicted_url}")

# ==========================================
# 2. LFU Cache (Least Frequently Used)
# ==========================================
class LFUCache:
    def __init__(self, capacity: int):
        self.capacity = capacity
        self.min_freq = 0
        self.key_map = {} # Map[url] -> (data, frequency)
        self.freq_map = defaultdict(OrderedDict) # Map[frequency] -> OrderedDict of URLs
        self.hits = 0
        self.misses = 0

    def get(self, url: str) -> str:
        if url not in self.key_map:
            self.misses += 1
            return None
        
        # HIT: Update frequency
        data, freq = self.key_map[url]
        self._increment_freq(url, data, freq)
        self.hits += 1
        return data

    def put(self, url: str, data: str):
        if self.capacity == 0: return
        
        if url in self.key_map:
            _, freq = self.key_map[url]
            self._increment_freq(url, data, freq)
            return
            
        # EVICT: Look at the lowest bucket and pop the oldest item
        if len(self.key_map) >= self.capacity:
            evicted_url, _ = self.freq_map[self.min_freq].popitem(last=False)
            del self.key_map[evicted_url]
            print(f"      [LFU Evicted] -> {evicted_url} (Freq: {self.min_freq})")
            
        # Add new item to bucket 1
        self.key_map[url] = (data, 1)
        self.freq_map[1][url] = True
        self.min_freq = 1

    def _increment_freq(self, url, data, freq):
        # Remove from old bucket
        del self.freq_map[freq][url]
        if not self.freq_map[freq] and self.min_freq == freq:
            self.min_freq += 1
            
        # Move to new bucket
        self.key_map[url] = (data, freq + 1)
        self.freq_map[freq + 1][url] = True

# ==========================================
# 3. Hybrid Cache (LRU + LFU Tiered)
# ==========================================
class HybridCache:
    def __init__(self, capacity: int, threshold: int = 3):
        # Split capacity 50/50
        self.lru_tier = LRUCache(max(1, capacity // 2))
        self.lfu_tier = LFUCache(max(1, capacity // 2))
        self.threshold = threshold
        self.global_freq = defaultdict(int)
        
        self.hits = 0
        self.misses = 0

    def get(self, url: str) -> str:
        # Check LRU Tier (Probation)
        data = self.lru_tier.get(url)
        if data:
            self.hits += 1
            self.global_freq[url] += 1
            
            # GRADUATION CHECK
            if self.global_freq[url] >= self.threshold:
                print(f"      🎓 [GRADUATION] {url} moved to LFU VIP Tier!")
                del self.lru_tier.cache[url]
                self.lfu_tier.put(url, data)
                # Manually sync the LFU frequency with the global frequency
                self.lfu_tier.key_map[url] = (data, self.global_freq[url])
            return data

        # Check LFU Tier (VIP)
        data = self.lfu_tier.get(url)
        if data:
            self.hits += 1
            self.global_freq[url] += 1
            return data
            
        self.misses += 1
        return None

    def put(self, url: str, data: str):
        self.global_freq[url] += 1
        # Always put new items in the LRU probation tier
        self.lru_tier.put(url, data)


# ==========================================
# 4. The Proxy Simulator
# ==========================================
def run_simulation(policy: str, max_entries: int):
    print(f"\n{'='*50}")
    print(f"🚀 STARTING SIMULATION: Policy={policy.upper()}, Max Entries={max_entries}")
    print(f"{'='*50}")
    
    if policy == "lru":
        cache = LRUCache(max_entries)
    elif policy == "lfu":
        cache = LFUCache(max_entries)
    else:
        cache = HybridCache(max_entries, threshold=3)

    # Hardcoded websites to simulate traffic
    traffic_stream = [
        "google.com", "google.com", "google.com", # Very popular
        "youtube.com", "youtube.com",             # Popular
        "random-blog.com",                        # Scraper bot
        "spam-site.com",                          # Scraper bot
        "google.com",                             # Requesting popular again
        "breaking-news.com"                       # New trending topic
    ]

    for step, url in enumerate(traffic_stream):
        print(f"\n[Step {step+1}] Client requests: {url}")
        
        # 1. Try Cache
        data = cache.get(url)
        
        # 2. On Miss, fetch from internet and put in cache
        if data:
            print(f"      ✅ CACHE HIT! (0 MB Bandwidth used)")
        else:
            print(f"      ❌ CACHE MISS! Fetching from internet... (1 MB Bandwidth used)")
            cache.put(url, f"<html>Data for {url}</html>")

        time.sleep(0.5) # Slow down for readability

    print("\n📊 --- FINAL STATS ---")
    print(f"Total Hits: {cache.hits}")
    print(f"Total Misses: {cache.misses}")
    print(f"Bandwidth Saved: {cache.hits} MB")

# ==========================================
# Run the tests! Change parameters here.
# ==========================================
if __name__ == "__main__":
    # Test 1: LRU with 3 slots
    run_simulation(policy="lru", max_entries=3)
    
    # Test 2: LFU with 3 slots
    run_simulation(policy="lfu", max_entries=3)
    
    # Test 3: Hybrid with 4 slots (2 for LRU, 2 for LFU)
    run_simulation(policy="hybrid", max_entries=4)
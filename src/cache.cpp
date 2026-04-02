#include "../include/cache.h"
#include <algorithm>

// ══════════════════════════════════════════════
//  LRUCache Implementation
// ══════════════════════════════════════════════

bool LRUCache::get(const std::string& url, std::string& out_response) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = map_.find(url);
    if (it == map_.end()) {
        stats_.misses++;
        return false;
    }

    // check TTL expiry
    if (it->second->is_expired()) {
        current_bytes_ -= it->second->size;
        lru_list_.erase(it->second);
        map_.erase(it);
        stats_.misses++;
        stats_.evictions++;
        return false;
    }

    // move to front (most recently used)
    it->second->last_accessed = std::chrono::steady_clock::now();
    lru_list_.splice(lru_list_.begin(), lru_list_, it->second);

    out_response = it->second->response;
    stats_.hits++;
    stats_.bytes_saved += it->second->size;
    return true;
}

void LRUCache::put(const std::string& url, const std::string& response, int ttl) {
    std::lock_guard<std::mutex> lock(mutex_);

    // if already exists, remove old entry
    auto it = map_.find(url);
    if (it != map_.end()) {
        current_bytes_ -= it->second->size;
        lru_list_.erase(it->second);
        map_.erase(it);
    }

    // evict if over limits
    while (!lru_list_.empty() &&
           (map_.size() >= max_entries_ || current_bytes_ + response.size() > max_bytes_)) {
        evict_one();
    }

    CacheEntry entry(url, response, ttl);
    lru_list_.push_front(entry);
    map_[url] = lru_list_.begin();
    current_bytes_ += response.size();
}

bool LRUCache::remove(const std::string& url) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = map_.find(url);
    if (it == map_.end()) return false;
    
    current_bytes_ -= it->second->size;
    lru_list_.erase(it->second);
    map_.erase(it);
    return true;
}

void LRUCache::evict_one() {
    if (lru_list_.empty()) return;
    auto& last = lru_list_.back();
    current_bytes_ -= last.size;
    map_.erase(last.url);
    lru_list_.pop_back();
    stats_.evictions++;
}

void LRUCache::evict_expired() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = lru_list_.begin(); it != lru_list_.end(); ) {
        if (it->is_expired()) {
            current_bytes_ -= it->size;
            map_.erase(it->url);
            it = lru_list_.erase(it);
            stats_.evictions++;
        } else {
            ++it;
        }
    }
}

void LRUCache::print_contents() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "\n[LRU Cache Contents — " << map_.size() << " entries, "
              << current_bytes_ / 1024 << " KB]\n";
    int i = 1;
    for (const auto& e : lru_list_) {
        std::cout << "  " << i++ << ". " << e.url
                  << " [" << e.size / 1024 << " KB, TTL=" << e.ttl_seconds << "s]\n";
        if (i > 10) { std::cout << "  ... (showing first 10)\n"; break; }
    }
}

// ══════════════════════════════════════════════
//  LFUCache Implementation
// ══════════════════════════════════════════════

bool LFUCache::get(const std::string& url, std::string& out_response) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = key_map_.find(url);
    if (it == key_map_.end()) {
        stats_.misses++;
        return false;
    }

    if (it->second.entry.is_expired()) {
        // remove from freq_map
        int freq = it->second.entry.frequency;
        freq_map_[freq].erase(it->second.it);
        if (freq_map_[freq].empty()) freq_map_.erase(freq);
        current_bytes_ -= it->second.entry.size;
        key_map_.erase(it);
        stats_.misses++;
        stats_.evictions++;
        return false;
    }

    increment_freq(url);
    out_response = key_map_[url].entry.response;
    stats_.hits++;
    stats_.bytes_saved += key_map_[url].entry.size;
    return true;
}

void LFUCache::put(const std::string& url, const std::string& response, int ttl) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (key_map_.count(url)) {
        key_map_[url].entry.response = response;
        key_map_[url].entry.size     = response.size();
        increment_freq(url);
        return;
    }

    while (!key_map_.empty() &&
           (key_map_.size() >= max_entries_ || current_bytes_ + response.size() > max_bytes_)) {
        evict_one();
    }

    CacheEntry entry(url, response, ttl);
    entry.frequency = 1;

    freq_map_[1].push_back(url);
    auto list_it = std::prev(freq_map_[1].end());

    key_map_[url] = { entry, list_it };
    current_bytes_ += response.size();
    min_freq_ = 1;
}

void LFUCache::reset_ttl(const std::string& url, int ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = key_map_.find(url);
    if (it != key_map_.end()) {
        it->second.entry.inserted_at = std::chrono::steady_clock::now();
        it->second.entry.ttl_seconds = ttl;
    }
}

void LFUCache::increment_freq(const std::string& url) {
    auto& node = key_map_[url];
    int old_freq = node.entry.frequency;
    int new_freq = old_freq + 1;

    // remove from old freq bucket
    freq_map_[old_freq].erase(node.it);
    if (freq_map_[old_freq].empty()) {
        freq_map_.erase(old_freq);
        if (min_freq_ == old_freq) min_freq_ = new_freq;
    }

    // insert into new freq bucket
    freq_map_[new_freq].push_back(url);
    node.it = std::prev(freq_map_[new_freq].end());
    node.entry.frequency = new_freq;
    node.entry.last_accessed = std::chrono::steady_clock::now();
}

void LFUCache::evict_one() {
    if (key_map_.empty()) return;
    auto& min_list = freq_map_[min_freq_];
    if (min_list.empty()) return;

    std::string evict_url = min_list.front();
    min_list.pop_front();
    if (min_list.empty()) freq_map_.erase(min_freq_);

    current_bytes_ -= key_map_[evict_url].entry.size;
    key_map_.erase(evict_url);
    stats_.evictions++;
}

void LFUCache::evict_expired() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> to_remove;

    for (auto& [url, node] : key_map_) {
        if (node.entry.is_expired()) to_remove.push_back(url);
    }

    for (const auto& url : to_remove) {
        auto& node = key_map_[url];
        int freq = node.entry.frequency;
        freq_map_[freq].erase(node.it);
        if (freq_map_[freq].empty()) freq_map_.erase(freq);
        current_bytes_ -= node.entry.size;
        key_map_.erase(url);
        stats_.evictions++;
    }
}

void LFUCache::print_contents() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "\n[LFU Cache Contents — " << key_map_.size() << " entries, "
              << current_bytes_ / 1024 << " KB]\n";
    int i = 1;
    for (const auto& [url, node] : key_map_) {
        std::cout << "  " << i++ << ". " << url
                  << " [freq=" << node.entry.frequency
                  << ", " << node.entry.size / 1024 << " KB]\n";
        if (i > 10) { std::cout << "  ... (showing first 10)\n"; break; }
    }
}

// ══════════════════════════════════════════════
//  HybridCache Implementation
// ══════════════════════════════════════════════

HybridCache::HybridCache(size_t max_entries, size_t max_bytes, CacheStats& stats, int threshold)
    : max_entries_(max_entries), max_bytes_(max_bytes), threshold_T_(threshold), stats_(stats),
      lru_tier_(max_entries / 2, max_bytes / 2, stats), // Give 50% capacity to LRU
      lfu_tier_(max_entries / 2, max_bytes / 2, stats)  // Give 50% capacity to LFU
{}

bool HybridCache::get(const std::string& url, std::string& out_response) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. Check the LRU Tier
    if (lru_tier_.get(url, out_response)) {
        global_freq_[url]++;
        
        // Threshold check for graduation
        if (global_freq_[url] > threshold_T_) {
            lru_tier_.remove(url);
            lfu_tier_.put(url, out_response, 300); // Graduate to LFU with fresh TTL
        }
        return true;
    }

    // 2. Check the LFU Tier
    if (lfu_tier_.get(url, out_response)) {
        global_freq_[url]++;
        lfu_tier_.reset_ttl(url, 300); // Reset TTL on hit as per the paper
        return true;
    }

    return false;
}

void HybridCache::put(const std::string& url, const std::string& response, int ttl) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Paper logic: New items always go into LRU first, frequency starts at 1
    global_freq_[url] = 1;
    lru_tier_.put(url, response, ttl); 
}

void HybridCache::evict_expired() {
    lru_tier_.evict_expired();
    lfu_tier_.evict_expired();
}

size_t HybridCache::size() const { return lru_tier_.size() + lfu_tier_.size(); }
size_t HybridCache::bytes() const { return lru_tier_.bytes() + lfu_tier_.bytes(); }

void HybridCache::print_contents() const {
    lru_tier_.print_contents();
    lfu_tier_.print_contents();
}
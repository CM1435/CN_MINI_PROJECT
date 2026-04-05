import urllib.request

proxy = urllib.request.ProxyHandler({'http': '127.0.0.1:8080'})
opener = urllib.request.build_opener(proxy)

def fetch(url):
    try:
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
        # Added timeout=2 so the script NEVER freezes
        opener.open(req, timeout=2).read()
    except Exception:
        pass

print("  -> Phase 1: VIP Websites (Freq 5)")
vips = ["http://example.com/?q=vip1", "http://example.com/?q=vip2"]
for _ in range(5):
    for u in vips: fetch(u)

print("  -> Phase 2: Old Junk (Freq 4)")
junk = ["http://example.com/?q=junk1", "http://example.com/?q=junk2"]
for _ in range(4):
    for u in junk: fetch(u)

print("  -> Phase 3: Bot Attack (Freq 1)")
for i in range(4):
    fetch(f"http://example.com/?q=bot{i}")

print("  -> Phase 4: New Trends (Freq 2)")
trends = ["http://example.com/?q=trend1", "http://example.com/?q=trend2"]
for _ in range(2):
    for u in trends: fetch(u)

print("  -> Phase 5: THE FINAL EXAM (Fetch VIPs + Trends)")
final_test = vips + trends
for u in final_test: fetch(u)

print("  -> Python Script Finished! Handing back to Bash...")
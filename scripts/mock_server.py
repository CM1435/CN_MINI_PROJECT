from http.server import BaseHTTPRequestHandler, HTTPServer

class MockHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-type", "text/plain")
        self.send_header("Cache-Control", "max-age=3600") # Force proxy to cache
        self.end_headers()
        self.wfile.write(b"Perfect Lab Data")
        
    def log_message(self, format, *args):
        pass # Keep the terminal quiet

print("🌐 Fake Internet running on port 9090...")
HTTPServer(('127.0.0.1', 9090), MockHandler).serve_forever()
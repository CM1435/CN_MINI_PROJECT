CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
INCLUDES = -Iinclude
SRC      = src/main.cpp src/proxy.cpp src/cache.cpp src/logger.cpp
TARGET   = proxy_cache

.PHONY: all clean run debug

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) -o $(TARGET)
	@echo "\n✅  Build successful → ./$(TARGET)"
	@echo "   Run with: ./$(TARGET) -p 8080 -P lru -v"

debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

clean:
	rm -f $(TARGET) proxy.log

run: all
	./$(TARGET) -p 8080 -P lru -t 300 -m 50 -v

run-lfu: all
	./$(TARGET) -p 8080 -P lfu -t 300 -m 50 -v

bench:
	@echo "Running benchmark (proxy must be running on :8080)..."
	python3 scripts/bench.py --proxy localhost:8080 --rounds 3

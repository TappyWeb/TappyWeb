CXX = clang++
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra -Wpedantic -pthread

all: bin/tappy bin/tappy-update

src/assets.hpp: assets/htmx.min.js assets/default.css tools/embed.py
	python3 tools/embed.py

bin/tappy: src/main.cpp src/assets.hpp src/update_support.hpp vendor/httplib.h
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -Ivendor src/main.cpp -o $@

test: bin/tappy
	python3 tests/test_cli.py

run: bin/tappy
	./bin/tappy serve examples/tasks.tappy

.PHONY: all test run

pkg: src/assets.hpp
	python3 tools/package_macos.py

.PHONY: pkg

bin/tappy-update: tools/updater.cpp src/update_support.hpp vendor/json.hpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -Ivendor tools/updater.cpp -o $@

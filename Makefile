CXX = clang++
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra -Wpedantic -pthread

all: bin/tappy bin/tappy-update

src/assets.hpp: assets/htmx.min.js assets/default.css tools/embed.py
	python3 tools/embed.py

bin/tappy: src/main.cpp src/assets.hpp src/update_support.hpp src/themes.hpp vendor/httplib.h
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -Ivendor src/main.cpp -o $@

test: bin/tappy bin/tappy-theme-tests
	python3 tests/test_cli.py
	./bin/tappy-theme-tests

run: bin/tappy
	./bin/tappy serve examples/tasks.tappy

.PHONY: all test run

pkg: src/assets.hpp
	python3 tools/package_macos.py

.PHONY: pkg

bin/tappy-update: tools/updater.cpp src/update_support.hpp vendor/json.hpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -Ivendor tools/updater.cpp -o $@

bin/tappy-theme-tests: tests/themes.cpp src/themes.hpp src/update_support.hpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) tests/themes.cpp -o $@

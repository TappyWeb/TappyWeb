CXX = clang++
CXXFLAGS = -std=c++20 -O2 -Wall -Wextra -Wpedantic -pthread

all: bin/tappy bin/tappy-update

bin/tappy-embed: tools/embed.cpp
	@mkdir -p bin
	$(CXX) -std=c++17 -O2 tools/embed.cpp -o $@

src/assets.hpp: assets/htmx.min.js assets/default.css bin/tappy-embed
	./bin/tappy-embed .

bin/tappy: src/main.cpp src/assets.hpp src/update_support.hpp src/themes.hpp src/python.hpp vendor/httplib.h vendor/json.hpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -Ivendor src/main.cpp -o $@

test: bin/tappy bin/tappy-theme-tests
	python3 tests/test_cli.py
	./bin/tappy-theme-tests

run: bin/tappy
	./bin/tappy serve examples/tasks.tappy

.PHONY: all test run

pkg: src/assets.hpp bin/tappy-package
	./bin/tappy-package macos

.PHONY: pkg

bin/tappy-update: tools/updater.cpp src/update_support.hpp vendor/json.hpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) -Ivendor tools/updater.cpp -o $@

bin/tappy-theme-tests: tests/themes.cpp src/themes.hpp src/update_support.hpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) tests/themes.cpp -o $@

bin/tappy-package: tools/package.cpp src/update_support.hpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) tools/package.cpp -o $@

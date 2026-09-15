CXX = clang++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic -Ifirmware/STUDIO/src -Idesktop
ifeq ($(shell uname -s),Darwin)
CXXFLAGS += -isystem $(shell xcrun --show-sdk-path)/usr/include/c++/v1
endif
CORE = $(wildcard firmware/STUDIO/src/studio/*.cpp) desktop/Host.cpp
HEADERS = $(wildcard firmware/STUDIO/src/studio/*.h) desktop/Host.h
.PHONY: all test sanitize clean
all: build/studio_sim build/studio_tests
build:
	mkdir -p build
build/studio_sim: $(CORE) desktop/main.cpp $(HEADERS) | build
	$(CXX) $(CXXFLAGS) $(filter %.cpp,$^) -o $@
build/studio_tests: $(CORE) tests/tests.cpp $(HEADERS) | build
	$(CXX) $(CXXFLAGS) $(filter %.cpp,$^) -o $@
test: build/studio_tests
	./build/studio_tests
sanitize: | build
	$(CXX) $(CXXFLAGS) -O1 -g -fsanitize=address,undefined $(CORE) tests/tests.cpp -o build/studio_tests_sanitized
	./build/studio_tests_sanitized
clean:
	rm -rf build

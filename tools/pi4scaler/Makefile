#
# Builds the PC tests for the Pi 4's software smooth scaling and CRT shader
# (src/pi4/scaler.*, see test.cpp) and for sharing a frame between cores
# (src/pi4/frameshare.h, see test_share.cpp).
#
#   tools/pi4scaler/run_tests.sh
#

ROOT := ../..
CXX ?= c++
# -O3, as the Pi 4 build compiles scaler.cpp (see the Makefile at the top).
CXXFLAGS = -O3 -g -Wall -std=c++17

OUT := build

all: $(OUT)/pi4scaler_test $(OUT)/share_test

$(OUT)/pi4scaler_test: test.cpp $(ROOT)/src/pi4/scaler.cpp $(ROOT)/src/pi4/scaler.h
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -o $@ test.cpp $(ROOT)/src/pi4/scaler.cpp -lm

# The frame sharing between cores, with threads, under ThreadSanitizer.
$(OUT)/share_test: test_share.cpp $(ROOT)/src/pi4/frameshare.h
	@mkdir -p $(OUT)
	$(CXX) -O2 -g -Wall -std=c++17 -fsanitize=thread -o $@ test_share.cpp -pthread

clean:
	rm -rf $(OUT)

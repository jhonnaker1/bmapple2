#
# Builds the PC tests for the Pi 4's direct display driver (src/pi4kms/,
# see test.cpp).
#
#   tools/pi4kms/run_tests.sh
#

ROOT := ../..
CXX ?= c++
CXXFLAGS = -O2 -g -Wall -Wextra -std=c++17
SRCS := test.cpp test_modes.cpp $(ROOT)/src/pi4kms/filter.cpp \
        $(ROOT)/src/pi4kms/dlist.cpp $(ROOT)/src/pi4kms/modes.cpp

OUT := build

all: $(OUT)/pi4kms_test

$(OUT)/pi4kms_test: $(SRCS) tests.h $(wildcard $(ROOT)/src/pi4kms/*.h)
	@mkdir -p $(OUT)
	$(CXX) $(CXXFLAGS) -o $@ $(SRCS) -lm

clean:
	rm -rf $(OUT)

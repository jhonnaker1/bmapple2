#
# Builds the PC test harness for the emulator port (see host_sim.c).
#
#   make -C tools/hostsim
#   tools/hostsim/run_tests.sh
#

ROOT := ../..
KEGS := $(ROOT)/third_party/kegs/src
include $(ROOT)/src/port/kegs.mk

CC ?= cc
CFLAGS = -O2 -g -std=gnu11 -I $(KEGS) -I $(ROOT)/src/port -DBMAPPLE2_HOST_LIBC

OUT := build
OBJS := $(addprefix $(OUT)/kegs/,$(KEGS_SRCS:.c=.o) $(KEGS_HOST_SRCS:.c=.o)) \
        $(OUT)/port/platform.o $(OUT)/port/keymap.o $(OUT)/port/joystick.o \
        $(OUT)/port/snddrv.o $(OUT)/port/menu.o $(OUT)/port/osd.o \
        $(OUT)/port/scc_socket_driver.o \
        $(OUT)/host_sim.o $(OUT)/live.o

all: $(OUT)/bmapple2_sim

$(OUT)/bmapple2_sim: $(OBJS)
	$(CC) -o $@ $(OBJS) -lm

$(OUT)/kegs/clock.o: $(KEGS)/clock.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(KEGS_CLOCK_RENAMES) -w -c -o $@ $<

$(OUT)/kegs/%.o: $(KEGS)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -w -c -o $@ $<

# KEGS's code, changed: KEGS's warnings are its own.
$(OUT)/port/scc_socket_driver.o: $(ROOT)/src/port/scc_socket_driver.c $(ROOT)/src/host.h
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -w -c -o $@ $<

$(OUT)/port/%.o: $(ROOT)/src/port/%.c $(ROOT)/src/port/bmapple2.h \
                 $(ROOT)/src/port/osd.h $(ROOT)/src/host.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wall -Wextra -Wno-unused-parameter -c -o $@ $<

$(OUT)/host_sim.o: host_sim.c live.h $(ROOT)/src/host.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wall -c -o $@ $<

$(OUT)/live.o: live.c live.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Wall -Wextra -DLIVE_HTML=\"$(abspath live.html)\" -c -o $@ $<

clean:
	rm -rf $(OUT)

.PHONY: all clean

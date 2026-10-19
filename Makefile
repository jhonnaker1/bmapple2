#
# Makefile for the BMApple2 kernel image.
#
# Run ./make_all.sh once first; it builds circle-stdlib and then this.
# After that "make" rebuilds the kernel image for the board circle-stdlib is
# built for: kernel7.img (Pi 2), kernel8-32.img (Pi 3), kernel8-rpi4.img
# (Pi 4, 64-bit) or kernel_2712.img (Pi 5, 64-bit).
# ./build_sdcard.sh stages everything for the SD card in build/sdcard.
#

ROOT := $(CURDIR)

ifeq ($(wildcard third_party/circle-stdlib/Config.mk),)
$(error circle-stdlib is not configured. Run ./make_all.sh first)
endif

include third_party/circle-stdlib/Config.mk
# RASPPI (the board) is in Circle's own Config.mk, which circle-stdlib's
# configure writes and Circle's Rules.mk reads; it's needed here first to
# pick the board's display and sound code.
include $(CIRCLEHOME)/Config.mk

BMAPPLE2_VERSION ?= $(shell tools/version.sh --semver)
BMAPPLE2_GIT ?= $(shell tools/version.sh --git)
KEGS := third_party/kegs/src
KEGS_VERSION := $(shell sed -n 's/.*g_kegs_version_str\[\] = "\(.*\)";.*/\1/p' $(KEGS)/sim65816.c)

include src/port/kegs.mk

BUILD := build
KEGS_OBJS := $(addprefix $(BUILD)/kegs/,$(KEGS_SRCS:.c=.o))
KEGS_LIB := $(BUILD)/libkegs.a

OBJS = src/main.o src/kernel.o src/logging.o src/options.o \
       src/port/platform.o src/port/keymap.o src/port/joystick.o \
       src/port/snddrv.o src/port/menu.o src/port/osd.o \
       src/port/scc_stub.o src/port/scc_socket_driver.o src/port/posix_extra.o \
       src/port/fswrap.o \
       src/network.o src/network_time_sync.o \
       src/webui/webui.o src/webui/webui_http.o src/webui/webui_fs.o \
       src/webui/webui_machine.o src/webui/webui_assets.o \
       src/sdcard/sd_fs_fatfs.o src/sdcard/sd_fs_file.o

# Display and sound. The Pi 2 and 3 use the VideoCore through VCHIQ
# (dispmanx, from BMC64); the Pi 4 and 5 use Circle's framebuffer and HDMI
# sound drivers (src/av/, shared by both; see docs/PI4.md and docs/PI5.md).
ifneq ($(filter 4 5,$(RASPPI)),)
OBJS += src/av/video.o src/av/sound.o src/av/hdmisound.o src/av/scaler.o \
        src/kms/common/filter.o src/kms/common/modes.o \
        src/v3d/gpu.o src/v3d/draw.o src/v3d/crt.o \
        src/v3d/shaders.o
# The direct display driver: the Pi 4's (src/kms/pi4/) or the Pi 5's
# (src/kms/pi5/), with shared filter kernels and mode calculations.
ifeq ($(RASPPI),4)
OBJS += src/kms/pi4/dlist.o src/kms/pi4/hvs.o src/kms/pi4/display.o \
        src/kms/pi4/phy.o src/kms/pi4/modeset.o
else
OBJS += src/kms/pi5/dlist.o src/kms/pi5/hvs.o src/kms/pi5/display.o \
        src/kms/pi5/phy.o src/kms/pi5/modeset.o
endif
# Smooth scaling and the CRT shader are drawn by the ARM: -O3 lets GCC turn
# their per-pixel loops into NEON code (tools/pi4scaler tests the same).
src/av/scaler.o: OPTIMIZE = -O3
else
OBJS += src/fbl.o src/crt_pi_idx.o src/crt_pi_rgb.o src/vchiqsound.o \
        src/vchiqsoundbasedevice.o
endif

include $(CIRCLEHOME)/Rules.mk

INCLUDE := $(CIRCLE_STDLIB_INCLUDES) $(INCLUDE) -I $(CIRCLEHOME)/addon/fatfs

# See src/port/fswrap.c
LDFLAGS += --wrap=stat

VERSION_DEFINES = -DBMAPPLE2_VERSION=\"$(BMAPPLE2_VERSION)\" \
                  -DBMAPPLE2_GIT=\"$(BMAPPLE2_GIT)\" -DKEGS_VERSION=\"$(KEGS_VERSION)\"
CFLAGS += $(VERSION_DEFINES)
CPPFLAGS += $(VERSION_DEFINES)

# The port's C files include KEGS's headers (defc.h and the rest).
src/port/%.o src/port/%.d: CFLAGS += -I $(KEGS) -I src/port -idirafter src/port/compat -std=gnu11

# KEGS itself: compiled into build/ so the submodule stays clean. clock.c's
# get_dtime() and micro_sleep() are renamed out of the way of BMApple2's
# (src/port/kegs.mk).
KEGS_CFLAGS = -I $(KEGS) -I src/port -idirafter src/port/compat -std=gnu11 -w

$(BUILD)/kegs/clock.o: $(KEGS)/clock.c
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) $(KEGS_CFLAGS) $(KEGS_CLOCK_RENAMES) -c -o $@ $<

$(BUILD)/kegs/%.o: $(KEGS)/%.c
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) $(KEGS_CFLAGS) -c -o $@ $<

$(KEGS_LIB): $(KEGS_OBJS)
	@echo "  AR    $@"
	@rm -f $@
	@$(AR) cr $@ $(KEGS_OBJS)

# The web UI's pages are compiled in. Regenerate them when they change.
WEBUI_ASSETS = $(shell find src/webui/assets -type f)
src/webui/webui_assets.c: $(WEBUI_ASSETS) tools/gen_webui_assets.py
	@echo "  GEN   $@"
	@python3 tools/gen_webui_assets.py

$(TARGET).img: $(KEGS_LIB)

ifneq ($(filter 4 5,$(RASPPI)),)
VIDEOCORE_LIBS :=
else
VIDEOCORE_LIBS := \
	$(CIRCLEHOME)/addon/vc4/vchiq/libvchiq.a \
	$(CIRCLEHOME)/addon/vc4/interface/bcm_host/libbcm_host.a \
	$(CIRCLEHOME)/addon/vc4/interface/khronos/libkhrn_client.a \
	$(CIRCLEHOME)/addon/vc4/interface/vmcs_host/libvmcs_host.a \
	$(CIRCLEHOME)/addon/vc4/interface/vcos/libvcos.a \
	$(CIRCLEHOME)/addon/linux/liblinuxemu.a
endif

LIBS := $(KEGS_LIB) \
	$(CIRCLE_STDLIB_INSTALL_DIR)/lib/libm.a \
	$(CIRCLE_STDLIB_INSTALL_DIR)/lib/libc.a \
	$(CIRCLE_STDLIB_INSTALL_DIR)/lib/libcirclenewlib.a \
	$(CIRCLEHOME)/addon/SDCard/libsdcard.a \
	$(CIRCLEHOME)/lib/usb/libusb.a \
	$(CIRCLEHOME)/lib/input/libinput.a \
	$(CIRCLEHOME)/addon/fatfs/libfatfs.a \
	$(CIRCLEHOME)/lib/fs/libfs.a \
	$(CIRCLEHOME)/lib/net/libnet.a \
	$(CIRCLEHOME)/addon/wlan/hostap/wpa_supplicant/libwpa_supplicant.a \
	$(CIRCLEHOME)/addon/wlan/libwlan.a \
	$(VIDEOCORE_LIBS) \
	$(CIRCLEHOME)/lib/sound/libsound.a \
	$(CIRCLEHOME)/lib/sched/libsched.a \
	$(CIRCLEHOME)/lib/libcircle.a

EXTRACLEAN += $(OBJS) src/*.o src/port/*.d src/*.d src/webui/*.d src/sdcard/*.d \
              src/av/*.o src/av/*.d src/kms/common/*.o src/kms/common/*.d \
              src/kms/pi4/*.o src/kms/pi4/*.d \
              src/kms/pi5/*.o src/kms/pi5/*.d src/v3d/*.o src/v3d/*.d
.PHONY: distclean

distclean: clean
	rm -rf $(BUILD)

-include $(DEPS)

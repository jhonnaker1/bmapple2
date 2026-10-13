#
# kegs.mk - the KEGS sources BMApple2 builds.
#
# These come unmodified from the third_party/kegs submodule: the list in
# KEGS's src/ldvars (OBJECTS) plus compile_time.c, except
#  - sound_driver.c and joystick_driver.c, replaced by src/port/snddrv.c
#    and src/port/joystick.c;
#  - scc_unixdriver.c (a host serial device, set up with termios), replaced
#    on the Pi by src/port/scc_stub.c; the PC test harness builds it
#    (KEGS_HOST_SRCS);
#  - the front ends (xdriver.c, windriver.c, the Mac's Swift files) and the
#    sound drivers they use (pulseaudio_driver.c, macsnd_driver.c,
#    win32snd_driver.c): src/port/platform.c is BMApple2's.
# clock.c is compiled with get_dtime() and micro_sleep() renamed, so that
# BMApple2's (platform.c) are used (see the Makefile).
#
# When updating KEGS, check its src/ldvars for added or removed files (see
# docs/UPDATING.md).
#

KEGS_SRCS = \
	adb.c \
	applesingle.c \
	clock.c \
	compile_time.c \
	config.c \
	debugger.c \
	doc.c \
	dyna_filt.c \
	dyna_type.c \
	dyna_validate.c \
	dynapro.c \
	engine_c.c \
	iwm.c \
	mockingboard.c \
	moremem.c \
	paddles.c \
	scc.c \
	scc_socket_driver.c \
	scc_windriver.c \
	sim65816.c \
	smartport.c \
	sound.c \
	undeflate.c \
	unshk.c \
	video.c \
	voc.c \
	woz.c

# clock.c's own timing functions, renamed out of the way.
KEGS_CLOCK_RENAMES = -Dget_dtime=kegs_clock_get_dtime -Dmicro_sleep=kegs_clock_micro_sleep

# Built only for the PC test harness (tools/hostsim).
KEGS_HOST_SRCS = scc_unixdriver.c

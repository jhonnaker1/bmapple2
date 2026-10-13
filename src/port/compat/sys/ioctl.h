/*
 * sys/ioctl.h - stand-in for circle-newlib, which has none.
 *
 * KEGS's defc.h and sound.h include it, but none of what BMApple2 builds
 * uses anything from it (the serial port's modem lines, and the sound
 * child process KEGS's own sound driver talks to through shared memory).
 * Only the Pi build sees this directory (-idirafter in the Makefile).
 *
 * This file is part of BMApple2, a bare metal Apple II and IIgs emulator for
 * the Raspberry Pi. See LICENSE for licensing details.
 */

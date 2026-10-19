/*
 * bmapple2.h - shared declarations for the BMApple2 KEGS port.
 *
 * This file is part of BMApple2, a bare metal Apple II and IIgs emulator for
 * the Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef BMAPPLE2_PORT_H_
#define BMAPPLE2_PORT_H_

#include <stdint.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* USB HID usage codes (keyboard page) that BMApple2 itself uses. */
enum {
	USAGE_A = 0x04,
	USAGE_RETURN = 0x28, USAGE_ESCAPE = 0x29, USAGE_BACKSPACE = 0x2a,
	USAGE_TAB = 0x2b, USAGE_SPACE = 0x2c,
	USAGE_F1 = 0x3a, USAGE_F4 = 0x3d, USAGE_F10 = 0x43, USAGE_F11 = 0x44,
	USAGE_F12 = 0x45,
	USAGE_PAUSE = 0x48, USAGE_HOME = 0x4a, USAGE_PAGEUP = 0x4b,
	USAGE_DELETE = 0x4c, USAGE_END = 0x4d, USAGE_PAGEDOWN = 0x4e,
	USAGE_RIGHT = 0x4f, USAGE_LEFT = 0x50, USAGE_DOWN = 0x51, USAGE_UP = 0x52,
	USAGE_KP_ENTER = 0x58, USAGE_POWER = 0x66,
	USAGE_LCTRL = 0xe0, USAGE_LSHIFT = 0xe1, USAGE_LALT = 0xe2,
	USAGE_RCTRL = 0xe4, USAGE_RSHIFT = 0xe5, USAGE_RALT = 0xe6
};

/* KEGS keeps the function keys (F1 Command, F2 Option, F3 Escape, F4 its
   configuration screen, F6 speed, Ctrl+F12 Reset); F10 is free. */
#define KEY_MENU   USAGE_F10

/* platform.c */
void bmapple2_main(void);
const char *bmapple2_sd_root(void);
/* The emulator's frame buffer, for the on-screen display. */
uint16_t *bmapple2_framebuffer(int *pitch_px);
/* Shows the on-screen display's 640x460 area (TRUE) or the emulator's
   picture (FALSE). */
void bmapple2_osd_view(int on);
/* Called while a menu waits: keeps sound fed and runs network requests. */
void bmapple2_idle(void);
double bmapple2_refresh_rate(void);
int bmapple2_vsync_locked(void);
void bmapple2_video_settings_changed(void);
/* The disk activity line (disk_activity): on or off. */
int bmapple2_disk_activity(void);
void bmapple2_set_disk_activity(int on);
/* The display's resolution setting changed (the Pi 5). */
void bmapple2_display_mode_changed(void);
/* The machines, the one running (-1 if there is no ROM), and choosing
   another (its ROM is loaded and it starts; kept in bmapple2.cfg). */
enum { BMAPPLE2_IIGS3, BMAPPLE2_IIGS1, BMAPPLE2_IIE, BMAPPLE2_MACHINES };
int bmapple2_machine(void);
const char *bmapple2_machine_name(int machine);
int bmapple2_machine_available(int machine);
void bmapple2_set_machine(int machine);
/* The monitor: colour, or black and white, green or amber (kept in
   bmapple2.cfg). */
enum { BMAPPLE2_MONITORS = 4 };
int bmapple2_monitor(void);
const char *bmapple2_monitor_name(int monitor);
void bmapple2_set_monitor(int monitor);
/* The sound volume, 0-100 (kept in bmapple2.cfg). */
int bmapple2_volume(void);
void bmapple2_set_volume(int volume);
/* The running machine's ROM file, or NULL. */
const char *bmapple2_rom_file(void);
/* Reboot or power off, closing files first. */
void bmapple2_request_power(int power_off);
/* Boots the machine again, as switching it off and on does. */
void bmapple2_restart(void);
/* Web UI requests waiting to be done; menus close for them. */
int bmapple2_pending_requests(void);
/* Opens KEGS's own configuration screen (F4). */
void bmapple2_kegs_config(void);

/* keymap.c: USB HID usage to Apple ADB key code, or -1. */
int bmapple2_usage_to_adb(int usage);

/* snddrv.c */
void bmapple2_sound_open(void);
void bmapple2_feed_silence(void);

/* menu.c */
void bmapple2_menu(void);

#endif /* BMAPPLE2_PORT_H_ */

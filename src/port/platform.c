/*
 * platform.c - KEGS platform layer for BMApple2.
 *
 * Does what KEGS's front ends (xdriver.c, windriver.c, macdriver) do, on
 * top of the Circle kernel services in src/host.h: starts KEGS, runs its
 * main loop a frame (run_16ms()) at a time, copies its picture to the
 * display, and passes on keys, the mouse and the joysticks. Sound is in
 * snddrv.c, the joysticks in joystick.c, the menu in menu.c and osd.c.
 *
 * Picture: KEGS draws 32-bit pixels (0xRRGGBB, as its Windows front end
 * asks for them with video_set_*_mask(); KEGS works its colours out for
 * 8-bit fields) and reports the rectangles that changed each frame
 * (video_out_data()); only those are copied, converted to RGB565.
 *
 * Timing: KEGS paces itself to the IIgs's 59.92 Hz with get_dtime() and
 * micro_sleep() (clock.c). Here those are BMApple2's: when the display
 * refreshes at (close to) that rate, each frame is shown on its own display
 * refresh and KEGS's sleep is skipped, so scrolling is perfectly smooth;
 * otherwise KEGS's sleep does the pacing.
 *
 * This file is part of BMApple2, a bare metal Apple II and IIgs emulator for
 * the Raspberry Pi. See LICENSE for licensing details.
 */

#include "defc.h"

#include <math.h>
#include <strings.h>
#include <unistd.h>

#include "../host.h"
#include "bmapple2.h"
#include "osd.h"

/* KEGS (protos.h declares them; the names are KEGS's). */
extern int g_audio_enable;
extern int g_limit_speed;
extern int g_joystick_type;
extern const char g_kegs_version_str[];

/* ---- Settings ---------------------------------------------------------------- */

const char *bmapple2_sd_root(void)
{
	return host_option("root", "/");
}

static int option_on(const char *name, int default_value)
{
	const char *value = host_option(name, NULL);
	if (value == NULL || *value == '\0')
		return default_value;
	return strcmp(value, "1") == 0 || strcmp(value, "on") == 0
	    || strcmp(value, "yes") == 0 || strcmp(value, "true") == 0;
}

/* ---- Video ------------------------------------------------------------------- */

/* KEGS's window: the 640x400 Apple area and its borders, 704 x 462. KEGS's
   picture is taller: its status lines (F5) are below that, up to
   kegs_height lines, and video_out_data() writes them too. */
#define FB_W X_A2_WINDOW_WIDTH
#define FB_H X_A2_WINDOW_HEIGHT

static uint16_t *fb_pixels;
static int fb_pitch_px;
static word32 *kegs_pixels;     /* what video_out_data() writes */
static int kegs_height;         /* its height (FB_W wide) */
static Kimage *main_kimage;
static int osd_view;

static double refresh_rate = 60.0;
static int vsync_lock;

uint16_t *bmapple2_framebuffer(int *pitch_px)
{
	*pitch_px = fb_pitch_px;
	return fb_pixels;
}

/* KEGS's 640x400 Apple area filled a 4:3 monitor, so its pixels are 5/6
   as wide as they are high; aspect= in cmdline.txt multiplies that. */
#define PIXEL_ASPECT (5.0 / 6.0)

static void apply_view(void)
{
	double aspect = (double) FB_W / FB_H * PIXEL_ASPECT
	                * host_option_double("aspect", 1.0);
	if (osd_view)
		host_video_set_view(0, 0, 640, 460, 640.0 / 460.0);
	else
		host_video_set_view(0, 0, FB_W, FB_H, aspect);
}

void bmapple2_osd_view(int on)
{
	osd_view = on;
	apply_view();
	if (!on && main_kimage != NULL)
		video_set_x_refresh_needed(main_kimage, 1);
}

static void clear_framebuffer(void)
{
	int y;
	for (y = 0; y < FB_H; y++)
		memset(fb_pixels + y * fb_pitch_px, 0, FB_W * 2);
}

static void measure_refresh_rate(void)
{
	enum { SKIP = 3, COUNT = 20 };
	double start = 0.0, rate;
	int i;
	for (i = 0; i < SKIP + COUNT; i++) {
		host_video_present(TRUE);
		if (i == SKIP - 1)
			start = host_time();
	}
	rate = COUNT / (host_time() - start);
	if (rate > 20.0 && rate < 250.0)
		refresh_rate = rate;
	printf("BMApple2: display refresh rate %.3f Hz\n", refresh_rate);
}

static void update_sync_mode(void)
{
	const char *mode = host_option("vsync", "auto");
	if (strcmp(mode, "off") == 0 || strcmp(mode, "0") == 0)
		vsync_lock = FALSE;
	else if (strcmp(mode, "on") == 0 || strcmp(mode, "1") == 0)
		vsync_lock = TRUE;
	else {
		/* The IIgs runs at 59.92 Hz: a 60 Hz display is right for it. */
		double ratio = refresh_rate / VBL_RATE;
		vsync_lock = ratio > 0.98 && ratio < 1.02;
	}
	printf("BMApple2: display %.2f Hz, IIgs %.2f Hz, %s\n", refresh_rate, VBL_RATE,
	       vsync_lock ? "synchronised to the display" : "timer paced");
}

double bmapple2_refresh_rate(void)
{
	return refresh_rate;
}

int bmapple2_vsync_locked(void)
{
	return vsync_lock;
}

void bmapple2_video_settings_changed(void)
{
	update_sync_mode();
	apply_view();
}

/* The changed parts of KEGS's picture into the frame buffer. */
static void copy_picture(void)
{
	Change_rect rect;
	int i;
	for (i = 0; i < MAX_CHANGE_RECTS; i++) {
		int x, y;
		if (!video_out_data(kegs_pixels, main_kimage, FB_W, &rect, i))
			break;
		if (rect.y + rect.height > kegs_height || rect.x + rect.width > FB_W)
			continue;  /* never written: video_out_data() checks the same */
		for (y = rect.y; y < rect.y + rect.height && y < FB_H; y++) {
			const word32 *src = kegs_pixels + (size_t) y * FB_W;
			uint16_t *dst = fb_pixels + (size_t) y * fb_pitch_px;
			for (x = rect.x; x < rect.x + rect.width && x < FB_W; x++) {
				word32 p = src[x];
				dst[x] = (uint16_t) (((p >> 8) & 0xf800) | ((p >> 5) & 0x07e0)
				                     | ((p >> 3) & 0x001f));
			}
		}
	}
}

/* ---- Timing (clock.c's, renamed when it is compiled: see kegs.mk) -------- */

double get_dtime(void)
{
	return host_time();
}

int micro_sleep(double dtime)
{
	if (dtime <= 0.0)
		return 0;
	/* With the display doing the pacing, waiting for the next refresh
	   is the sleep. */
	if (vsync_lock && !osd_view)
		return 0;
	if (dtime > 0.25)
		dtime = 0.25;
	host_sleep_us((unsigned) (dtime * 1000000.0));
	return 0;
}

/* ---- Files ---------------------------------------------------------------- */

/* KEGS writes to disk images as the IIgs writes to its disks, with nothing
   kept back to write later, but the SD card's file system caches what it can.
   Since the Pi can be switched off at any time, push everything out to the
   card once a second. */
static void sync_files(void)
{
	int fd;
	fflush(NULL);
	for (fd = 3; fd < 30; fd++)
		fsync(fd);
}

/* ---- Starting ----------------------------------------------------------- */

/* KEGS stops if config.kegs isn't there (it only writes one it has read),
   so a new card gets an empty one, as KEGS's own first run would write. */
static void create_config(const char *path)
{
	struct stat st;
	FILE *f;
	if (stat(path, &st) == 0)
		return;
	f = fopen(path, "w");
	if (f == NULL) {
		printf("BMApple2: cannot create %s\n", path);
		return;
	}
	fprintf(f, "# KEGS configuration file version %s\n", g_kegs_version_str);
	fclose(f);
	printf("BMApple2: created %s\n", path);
}

/* ---- Restarting -------------------------------------------------------------- */

/* Boots the machine again, as switching it off and on does: the power-up
   byte is made wrong ($3F4 is $3F3 XOR $A5 once the machine has started),
   which every Apple II ROM takes as a power on, and the machine is reset. */
void bmapple2_restart(void)
{
	set_memory_c(0x3f4, (get_memory_c(0x3f3) ^ 0xa5) ^ 0xff, 0);
	do_reset();
}

/* ---- Starting a disk image ------------------------------------------------ */

/* Which slot boots an image: 6 for 5.25" disks, 5 for 3.5" ones, 7 (KEGS's
   SmartPort hard disks) for anything bigger. A WOZ image says which drive
   it is for (its INFO chunk); the others go by size, which is enough to
   tell 140K (plus a 2MG or nibble image's extra) from 800K from more. */
static int boot_slot(const char *path)
{
	struct stat st;
	const char *dot = strrchr(path, '.');
	if (dot != NULL && strcasecmp(dot, ".woz") == 0) {
		unsigned char header[22];
		FILE *f = fopen(path, "rb");
		int disk_type = 0;
		if (f != NULL) {
			if (fread(header, 1, sizeof header, f) == sizeof header
			    && memcmp(header + 12, "INFO", 4) == 0)
				disk_type = header[21];
			fclose(f);
		}
		return disk_type == 2 ? 5 : 6;
	}
	if (stat(path, &st) != 0)
		return -1;
	if (st.st_size <= 300 * 1024)
		return 6;
	if (st.st_size <= 900 * 1024)
		return 5;
	return 7;
}

/* Puts PATH in drive 1 of the slot that boots it and restarts. The IIgs
   (its Control Panel's startup slot "Scan", the default) and the IIe boot
   from the highest slot with a disk, so drive 1 of the slots above it is
   emptied; F4 puts those disks back. */
static void autostart(const char *path)
{
	int slot = boot_slot(path);
	int s;
	if (slot < 0) {
		printf("Web UI: cannot start %s: not found\n", path);
		return;
	}
	printf("Web UI: starting %s from slot %d\n", path, slot);
	for (s = 7; s > slot; s--)
		iwm_eject_disk_by_num(s, 0);
	insert_disk(slot, 0, path, 0, NULL, -1, 0);
	bmapple2_restart();
}

/* ---- Requests from the web UI ------------------------------------------ */

static volatile int request_power = -1;   /* 0 reboot, 1 power off */
static volatile int request_coldstart = FALSE;
static char autostart_path[600];
static volatile int request_autostart = FALSE;

void bmapple2_request_power(int power_off)
{
	request_power = power_off ? 1 : 0;
}

/* Called by the web UI. */
void bmapple2_request_coldstart(void)
{
	request_coldstart = TRUE;
}

/* Called by the web UI ("start this file"), on core 0: the emulator takes
   it up between frames (handle_requests()). One at a time. */
void emu_autostart_interrupt(const char *path)
{
	if (request_autostart)
		return;
	snprintf(autostart_path, sizeof autostart_path, "%s", path);
	__sync_synchronize();
	request_autostart = TRUE;
}

int bmapple2_pending_requests(void)
{
	return request_power >= 0 || request_coldstart || request_autostart;
}

static void handle_requests(void)
{
	if (request_autostart) {
		__sync_synchronize();
		autostart(autostart_path);
		request_autostart = FALSE;
	}
	if (request_coldstart) {
		request_coldstart = FALSE;
		printf("Web UI: restart\n");
		bmapple2_restart();
	}
	if (request_power >= 0) {
		int power_off = request_power;
		request_power = -1;
		printf("%s\n", power_off ? "Shutting down" : "Rebooting");
		sync_files();
		if (power_off)
			host_power_off();
		else
			host_reboot();
	}
}

static double last_sync_time;

void bmapple2_idle(void)
{
	double now = host_time();
	bmapple2_feed_silence();
	if (now - last_sync_time >= 1.0) {
		last_sync_time = now;
		sync_files();
	}
	if (request_power >= 0)
		handle_requests();
}

/* ---- ROM --------------------------------------------------------------- */

static char rom_path[256];

/* PATH in the card's DIR folder ("" for the root). */
static void card_path(char *path, size_t size, const char *dir, const char *name)
{
	const char *root = bmapple2_sd_root();
	size_t len = strlen(root);
	snprintf(path, size, "%s%s%s%s%s", root, len > 0 && root[len - 1] == '/' ? "" : "/",
	         dir, dir[0] ? "/" : "", name);
}

/* Reads exactly SIZE bytes of roms/NAME into BUF. */
static int read_rom_part(const char *name, unsigned char *buf, size_t size)
{
	char path[256];
	struct stat st;
	FILE *f;
	size_t got;
	card_path(path, sizeof path, "roms", name);
	if (stat(path, &st) != 0 || (size_t) st.st_size != size)
		return FALSE;
	f = fopen(path, "rb");
	if (f == NULL)
		return FALSE;
	got = fread(buf, 1, size, f);
	fclose(f);
	return got == size;
}

static void write_rom(const char *name, const unsigned char *data, size_t size)
{
	char path[256];
	FILE *f;
	card_path(path, sizeof path, "roms", name);
	f = fopen(path, "wb");
	if (f == NULL || fwrite(data, 1, size, f) != size) {
		printf("BMApple2: cannot write %s\n", path);
		if (f != NULL)
			fclose(f);
		return;
	}
	fclose(f);
	printf("BMApple2: made %s\n", path);
}

/* MAME's ROM sets (Ample's too) have the chips, not ROM files: for the
   IIgs ROM 3, apple2gs.zip's 341-0728 and 341-0748, the second with its
   halves the other way round (MAME's apple2gs.cpp); for the enhanced IIe,
   apple2e.zip's apple2ee/342-0304-a.e10 ($C000-$DFFF) and 342-0303-a.e8
   ($E000-$FFFF), and a2diskiing.zip's 341-0027-a.p5 (the Disk II card's
   PROM, at $600 in KEGS's IIe ROM file). Made into ROM.03 and APPLE2E.ROM
   once, when those aren't there yet. */
static void make_roms(void)
{
	char path[256];
	struct stat st;
	unsigned char *buf;

	card_path(path, sizeof path, "roms", "ROM.03");
	if (stat(path, &st) != 0) {
		buf = malloc(256 * 1024);
		if (buf != NULL && read_rom_part("341-0728", buf, 128 * 1024)
		    && read_rom_part("341-0748", buf + 128 * 1024, 128 * 1024)) {
			unsigned char *half = malloc(64 * 1024);
			if (half != NULL) {
				memcpy(half, buf + 128 * 1024, 64 * 1024);
				memmove(buf + 128 * 1024, buf + 192 * 1024, 64 * 1024);
				memcpy(buf + 192 * 1024, half, 64 * 1024);
				free(half);
				write_rom("ROM.03", buf, 256 * 1024);
			}
		}
		free(buf);
	}

	card_path(path, sizeof path, "roms", "APPLE2E.ROM");
	if (stat(path, &st) != 0) {
		buf = calloc(32 * 1024, 1);
		if (buf != NULL && read_rom_part("342-0304-a.e10", buf + 16 * 1024, 8 * 1024)
		    && read_rom_part("342-0303-a.e8", buf + 24 * 1024, 8 * 1024)) {
			if (!read_rom_part("341-0027-a.p5", buf + 0x600, 256))
				printf("BMApple2: no 341-0027-a.p5: the IIe has no Disk II card\n");
			write_rom("APPLE2E.ROM", buf, 32 * 1024);
		}
		free(buf);
	}
}

/* The ROM is looked for in the roms folder, then the card's root, under the
   names it usually has (KEGS's list, config.c, and the files ROM sets of
   other emulators use), and passed to KEGS with -rom: KEGS's own search
   (g_kegs_default_paths) works from the current folder and HOME, which a
   bare metal kernel doesn't have. ROM 3 is preferred to ROM 01, and both
   to an Apple IIe ROM (32K), which makes KEGS a IIe. */
static const char *find_rom(void)
{
	static const char *const names[] = {
		"ROM.03", "ROM03", "APPLE2GS.ROM2", "Rom03gd", "ROM.01", "ROM01",
		"APPLE2GS.ROM", "342-0077-b", "xgs.rom", "XGS.ROM", "ROM",
		"APPLE2E.ROM", NULL
	};
	static const char *const dirs[] = {"roms", "", NULL};
	int d, n;
	make_roms();
	for (d = 0; dirs[d] != NULL; d++) {
		for (n = 0; names[n] != NULL; n++) {
			struct stat st;
			card_path(rom_path, sizeof rom_path, dirs[d], names[n]);
			if (stat(rom_path, &st) == 0 && S_ISREG(st.st_mode)
			    && (st.st_size == 32 * 1024 || st.st_size == 128 * 1024
			        || st.st_size == 256 * 1024))
				return rom_path;
		}
	}
	rom_path[0] = '\0';
	return NULL;
}

const char *bmapple2_rom_file(void)
{
	return rom_path[0] ? rom_path : NULL;
}

void bmapple2_kegs_config(void)
{
	cfg_toggle_config_panel();
}

/* ---- Input --------------------------------------------------------------- */

static int open_menu;
static int pad_last_menu[HOST_PADS];
static int pad_last_keys[HOST_PADS];

/* Gamepad buttons that are keys, as in BMCoco: X is Space, Y is Return. */
static const struct {
	int bit;
	int adb;
} pad_keys[] = {
	{HOST_PAD_X, 0x31},
	{HOST_PAD_Y, 0x24},
};

static void process_input(void)
{
	int usage, pressed, pad, dx, dy, buttons;

	while (host_key_event(&usage, &pressed)) {
		int adb;
		/* The power key: Shut Down (the Pi 400 sends it for Fn+F10). */
		if (usage == USAGE_POWER) {
			if (pressed)
				bmapple2_request_power(1);
			continue;
		}
		if (usage == KEY_MENU) {
			if (pressed)
				open_menu = TRUE;
			continue;
		}
		adb = bmapple2_usage_to_adb(usage);
		if (adb >= 0)
			adb_physical_key_update(main_kimage, adb, 0, !pressed);
	}

	if (host_mouse_read(&dx, &dy, &buttons)) {
		/* 0x1000: x and y are deltas (adb.c). */
		adb_update_mouse(main_kimage, dx, dy, buttons & 7, 0x1000 | 7);
	}

	/* Gamepad Home, or Select and Start together, open the menu (A, B and
	   the stick are the joystick: joystick.c). */
	for (pad = 0; pad < HOST_PADS; pad++) {
		int state = host_pad_state(pad);
		int menu;
		if (state < 0)
			state = 0;
		menu = (state & HOST_PAD_HOME)
		       || ((state & HOST_PAD_SELECT) && (state & HOST_PAD_START));
		if (menu && !pad_last_menu[pad])
			open_menu = TRUE;
		pad_last_menu[pad] = menu;
	}

	/* X and Y, from any gamepad. */
	for (pad = 0; pad < HOST_PADS; pad++) {
		int state = host_pad_state(pad);
		unsigned k;
		if (state < 0)
			state = 0;
		for (k = 0; k < sizeof pad_keys / sizeof pad_keys[0]; k++) {
			int down = (state & pad_keys[k].bit) != 0;
			if (down != ((pad_last_keys[pad] & pad_keys[k].bit) != 0))
				adb_physical_key_update(main_kimage, pad_keys[k].adb, 0, !down);
		}
		pad_last_keys[pad] = state;
	}
}

/* ---- Main loop ------------------------------------------------------------- */

/* Shows the web UI's address once the network is up. */
static void show_network_address(void)
{
	static int shown;
	char address[32];
	if (shown || !option_on("webui", TRUE))
		return;
	if (host_network_address(address, sizeof(address))) {
		char text[64];
		snprintf(text, sizeof(text), "Web UI: http://%s/", address);
		osd_notify(text);
		shown = TRUE;
	}
}

void bmapple2_main(void)
{
	static char cfg_path[256];
	static char *argv[8];
	int argc = 0, pitch;
	double last_second;

	if (host_video_init(FB_W, FB_H, &fb_pixels, &pitch) != 0) {
		printf("BMApple2: no display\n");
		for (;;)
			host_sleep_us(1000000);
	}
	fb_pitch_px = pitch / 2;
	clear_framebuffer();
	apply_view();
	/* Where the host can set the video mode (the Pi 4), the display runs
	   at the IIgs's (and KEGS's IIe's) 59.92 Hz, then it is timed. */
	host_display_match_rate(VBL_RATE);
	measure_refresh_rate();
	update_sync_mode();

	/* config.kegs (disks, settings) in the card's root; the ROM found. */
	snprintf(cfg_path, sizeof(cfg_path), "%s%sconfig.kegs", bmapple2_sd_root(),
	         bmapple2_sd_root()[strlen(bmapple2_sd_root()) - 1] == '/' ? "" : "/");
	create_config(cfg_path);
	argv[argc++] = "bmapple2";
	if (getenv("BMAPPLE2_KEGS_NOIGNHALT"))
		argv[argc++] = "-noignhalt";
	argv[argc++] = "-cfg";
	argv[argc++] = cfg_path;
	if (find_rom() != NULL) {
		argv[argc++] = "-rom";
		argv[argc++] = rom_path;
		printf("BMApple2: ROM %s\n", rom_path);
	} else {
		/* KEGS then opens its configuration screen to choose one. */
		printf("BMApple2: no IIgs ROM in roms/ (see roms/ROMS.txt)\n");
	}
	argv[argc] = NULL;
	if (parse_argv(argc, argv, 1) != 0)
		printf("BMApple2: KEGS didn't like its arguments\n");

	g_audio_enable = option_on("sound", TRUE) ? 1 : 0;

	/* 0xRRGGBB pixels (see the top). */
	video_set_red_mask(0xff0000);
	video_set_green_mask(0x00ff00);
	video_set_blue_mask(0x0000ff);
	video_set_alpha_mask(0);
	/* The Apple II's 16 colours, from the masks (as KEGS's front ends do);
	   Super Hi-Res works its own out as it goes. */
	video_set_palette();

	/* The joystick is the gamepads and GPIO joysticks (joystick.c), KEGS's
	   "Native Joystick 1", rather than its default of the keypad. Set
	   before KEGS reads config.kegs, it is the default, so a choice made
	   in KEGS's settings (F4) still sticks. */
	g_joystick_type = 2;

	/* The speed of the machine the ROM is for: 1 MHz for an Apple IIe
	   (a 32K ROM), the IIgs's own 2.8 MHz for a IIgs, rather than KEGS's
	   default of an 8 MHz ZipGS accelerator, which the Pi 2 and 3 can't
	   always keep up with, and which runs old games too fast. As with the
	   joystick, a speed chosen with F6 or F4 still sticks. */
	{
		struct stat st;
		int iie = rom_path[0] != '\0' && stat(rom_path, &st) == 0
		          && st.st_size == 32 * 1024;
		g_limit_speed = iie ? 1 : 2;
	}

	if (kegs_init(32, FB_W, FB_H, 1) != 0) {
		osd_open();
		osd_message("KEGS did not start. The log (see the User Guide) says why.", FALSE);
		for (;;) {
			bmapple2_idle();
			host_video_present(TRUE);
		}
	}
	main_kimage = video_get_kimage(0);
	video_set_active(main_kimage, 1);
	video_update_scale(main_kimage, FB_W, FB_H, 1);
	/* The whole picture, status lines included (see FB_H). */
	/* The status lines come and go (F5), so leave room for many. */
	kegs_height = main_kimage->a2_height + 256;
	if (kegs_height < 1024)
		kegs_height = 1024;
	if (main_kimage->a2_width_full > FB_W)
		printf("BMApple2: KEGS's picture is %d wide, expected %d\n",
		       main_kimage->a2_width_full, FB_W);
	kegs_pixels = calloc((size_t) FB_W * kegs_height, sizeof(*kegs_pixels));

	last_second = host_time();
	for (;;) {
		double now;

		process_input();
		if (open_menu) {
			open_menu = FALSE;
			adb_kbd_repeat_off();
			bmapple2_menu();
			video_set_x_refresh_needed(main_kimage, 1);
		}

		if (run_16ms() != 0) {
			osd_open();
			osd_message("KEGS stopped. The log (see the User Guide) says why.", FALSE);
			for (;;) {
				bmapple2_idle();
				host_video_present(TRUE);
			}
		}
		copy_picture();
		osd_draw_notification(fb_pixels, fb_pitch_px, FB_W, FB_H);
		host_video_present(vsync_lock);

		now = host_time();
		if (now - last_second >= 1.0) {
			last_second = now;
			sync_files();
			show_network_address();
		}
		handle_requests();
	}
}

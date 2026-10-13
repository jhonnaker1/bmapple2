/*
 * menu.c - the BMApple2 menu.
 *
 * F10, a gamepad's Home button or Select+Start open it. BMApple2's own
 * settings (network, sound, video, power); everything about the emulated
 * IIgs (disks, the ROM, speed, the machine's settings) is in KEGS's own
 * configuration screen, F4, which this menu opens too. Settings are saved
 * in bmapple2.cfg and used again at the next boot.
 *
 * From BMCoco's menu.c.
 *
 * This file is part of BMApple2, a bare metal Apple II and IIgs emulator for
 * the Raspberry Pi. See LICENSE for licensing details.
 */

#include "defc.h"

#include <stdint.h>

#include "../host.h"
#include "bmapple2.h"
#include "osd.h"

#ifndef BMAPPLE2_VERSION
#define BMAPPLE2_VERSION "dev"
#endif

#define WPA_CONFIG "wpa_supplicant.conf"

/* Something changed that only takes effect after a reboot. */
static int reboot_needed;

/* ---- Settings ---------------------------------------------------------- */

static void save_setting(const char *name, const char *value)
{
	if (!host_option_set(name, value) || !host_options_save())
		osd_message("Cannot save bmapple2.cfg", TRUE);
}

static void save_setting_int(const char *name, int value)
{
	char text[16];
	snprintf(text, sizeof(text), "%d", value);
	save_setting(name, text);
}

static int setting_on(const char *name, int default_value)
{
	const char *value = host_option(name, NULL);
	if (value == NULL || *value == '\0')
		return default_value;
	return strcmp(value, "1") == 0 || strcmp(value, "yes") == 0
	    || strcmp(value, "true") == 0 || strcmp(value, "on") == 0;
}

static unsigned check(int on)
{
	return OSD_ITEM_CHECK | (on ? OSD_ITEM_CHECKED : 0);
}

static void power(int off)
{
	bmapple2_request_power(off);
	bmapple2_idle();
}

/* ---- Video -------------------------------------------------------------- */

static const char *vsync_mode(void)
{
	const char *mode = host_option("vsync", "auto");
	if (strcmp(mode, "on") == 0 || strcmp(mode, "1") == 0)
		return "On";
	if (strcmp(mode, "off") == 0 || strcmp(mode, "0") == 0)
		return "Off";
	return bmapple2_vsync_locked() ? "Auto (locked)" : "Auto (timer)";
}

static void video_menu(void)
{
	enum { V_VSYNC, V_SHADER, V_SMOOTH };
	int sel = 0;
	for (;;) {
		struct osd_item items[] = {
			{"Frame Lock", vsync_mode(), OSD_ITEM_SUBMENU, V_VSYNC},
			{"CRT Effect (reboot)", NULL, check(setting_on("crt_shader", FALSE)), V_SHADER},
			{"Smooth Scaling (reboot)", NULL, check(setting_on("smooth", FALSE)), V_SMOOTH},
		};
		int chosen = osd_menu("Video", items, sizeof(items) / sizeof(items[0]), &sel);
		int i;
		switch (chosen) {
		case V_VSYNC: {
			static const char *const choices[] = {
				"Automatic", "On: follow the display", "Off: keep exact time", NULL
			};
			static const char *const values[] = {"auto", "on", "off"};
			const char *mode = host_option("vsync", "auto");
			int current = strcmp(mode, "on") == 0 ? 1 : strcmp(mode, "off") == 0 ? 2 : 0;
			i = osd_choose("Frame Lock", choices, current);
			if (i >= 0) {
				save_setting("vsync", values[i]);
				bmapple2_video_settings_changed();
			}
			break;
		}
		case V_SHADER:
			save_setting("crt_shader", setting_on("crt_shader", FALSE) ? "0" : "1");
			reboot_needed = TRUE;
			break;
		case V_SMOOTH:
			save_setting("smooth", setting_on("smooth", FALSE) ? "0" : "1");
			reboot_needed = TRUE;
			break;
		default:
			return;
		}
	}
}

/* ---- Sound ---------------------------------------------------------------- */

static void sound_menu(void)
{
	enum { S_OUTPUT };
	static const char *const outputs[] = {"auto", "hdmi", "analog"};
	static const char *const output_names[] = {"Automatic", "HDMI", "Headphone jack", NULL};
	int sel = 0;
	for (;;) {
		const char *out = host_option("audio_out", "auto");
		int out_index = strcmp(out, "hdmi") == 0 ? 1
		                : strcmp(out, "analog") == 0 || strcmp(out, "headphones") == 0 ? 2 : 0;
		struct osd_item items[] = {
			{"Output (reboot)", output_names[out_index], OSD_ITEM_SUBMENU, S_OUTPUT},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{"The IIgs's volume is set in its", NULL, OSD_ITEM_LABEL, -1},
			{"Control Panel (Ctrl+Cmd+Esc).", NULL, OSD_ITEM_LABEL, -1},
		};
		int chosen = osd_menu("Sound", items, sizeof(items) / sizeof(items[0]), &sel);
		int i;
		if (chosen != S_OUTPUT)
			return;
		i = osd_choose("Sound Output", output_names, out_index);
		if (i >= 0 && i != out_index) {
			save_setting("audio_out", outputs[i]);
			reboot_needed = TRUE;
		}
	}
}

/* ---- Network --------------------------------------------------------------- */

static char wifi_ssid[33], wifi_psk[64], wifi_country[3];

static void wpa_path(char *path, size_t size)
{
	const char *root = bmapple2_sd_root();
	size_t len = strlen(root);
	snprintf(path, size, "%s%s%s", root, len > 0 && root[len - 1] == '/' ? "" : "/", WPA_CONFIG);
}

static void copy_quoted(char *dst, size_t size, const char *src)
{
	const char *end;
	if (*src == '"')
		src++;
	end = strchr(src, '"');
	if (end == NULL)
		end = src + strcspn(src, "\r\n");
	if ((size_t) (end - src) >= size)
		end = src + size - 1;
	memcpy(dst, src, end - src);
	dst[end - src] = '\0';
}

static void read_wifi_config(void)
{
	char path[256], line[160];
	FILE *f;
	wifi_ssid[0] = wifi_psk[0] = '\0';
	strcpy(wifi_country, "US");
	wpa_path(path, sizeof(path));
	f = fopen(path, "r");
	if (f == NULL)
		return;
	while (fgets(line, sizeof(line), f) != NULL) {
		char *p = line + strspn(line, " \t");
		if (strncmp(p, "country=", 8) == 0)
			copy_quoted(wifi_country, sizeof(wifi_country), p + 8);
		else if (strncmp(p, "ssid=", 5) == 0 && wifi_ssid[0] == '\0')
			copy_quoted(wifi_ssid, sizeof(wifi_ssid), p + 5);
		else if (strncmp(p, "psk=", 4) == 0 && wifi_psk[0] == '\0')
			copy_quoted(wifi_psk, sizeof(wifi_psk), p + 4);
	}
	fclose(f);
}

static int write_wifi_config(void)
{
	char path[256];
	FILE *f;
	wpa_path(path, sizeof(path));
	f = fopen(path, "w");
	if (f == NULL)
		return FALSE;
	fprintf(f, "country=%s\n\nnetwork={\n\tssid=\"%s\"\n", wifi_country, wifi_ssid);
	if (wifi_psk[0] != '\0')
		fprintf(f, "\tpsk=\"%s\"\n\tkey_mgmt=WPA-PSK\n", wifi_psk);
	else
		fprintf(f, "\tkey_mgmt=NONE\n");
	fprintf(f, "}\n");
	return fclose(f) == 0;
}

static void edit_wifi(const char *title, char *value, size_t size, int secret)
{
	char text[64];
	snprintf(text, sizeof(text), "%s", value);
	if (osd_edit(title, text, size < sizeof(text) ? size : sizeof(text), secret)) {
		snprintf(value, size, "%s", text);
		if (write_wifi_config())
			reboot_needed = TRUE;
		else
			osd_message("Cannot write " WPA_CONFIG, TRUE);
	}
}

static int compare_signal(const void *a, const void *b)
{
	return ((const struct host_wifi_ap *) b)->signal - ((const struct host_wifi_ap *) a)->signal;
}

/* Returns 1 if a network was chosen, 2 if it also needs a password. */
static int scan_wifi(void)
{
	enum { MAX_APS = 20 };
	static struct host_wifi_ap aps[MAX_APS];
	static char signal[MAX_APS][16];
	struct osd_item items[MAX_APS];
	int n, i, sel = 0, chosen;

	osd_message("Scanning for Wi-Fi networks...", FALSE);
	n = host_wifi_scan(aps, MAX_APS);
	if (n < 0) {
		if (osd_confirm("Wi-Fi is Off", "Switch to Wi-Fi and reboot?")) {
			save_setting("network", "wifi");
			power(0);
		}
		return 0;
	}
	if (n == 0) {
		osd_message("No Wi-Fi networks found.", TRUE);
		return 0;
	}
	qsort(aps, n, sizeof(aps[0]), compare_signal);
	for (i = 0; i < n; i++) {
		snprintf(signal[i], sizeof(signal[i]), "%s%d", aps[i].secure ? "* " : "", aps[i].signal);
		items[i] = (struct osd_item){aps[i].ssid, signal[i], 0, i};
	}
	chosen = osd_menu("Choose a Wi-Fi Network", items, n, &sel);
	if (chosen < 0)
		return 0;
	if (!aps[chosen].secure || strcmp(wifi_ssid, aps[chosen].ssid) != 0)
		wifi_psk[0] = '\0';
	snprintf(wifi_ssid, sizeof(wifi_ssid), "%s", aps[chosen].ssid);
	if (!write_wifi_config()) {
		osd_message("Cannot write " WPA_CONFIG, TRUE);
		return 0;
	}
	save_setting("network", "wifi");
	reboot_needed = TRUE;
	return aps[chosen].secure && wifi_psk[0] == '\0' ? 2 : 1;
}

static void network_menu(void)
{
	enum { N_DEVICE, N_SCAN, N_SSID, N_PASSWORD, N_COUNTRY, N_WEBUI, N_PIN, N_TIMEZONE, N_REBOOT };
	static const char *const devices[] = {"off", "ethernet", "wifi", NULL};
	static const char *const device_names[] = {"Off", "Ethernet", "Wi-Fi", NULL};
	int sel = 2;
	for (;;) {
		char status[64], tz[16];
		const char *net = host_option("network", "off");
		const char *pin = host_option("webui_pin", "");
		int device = strcmp(net, "ethernet") == 0 ? 1 : strcmp(net, "wifi") == 0 ? 2 : 0;
		int minutes = host_option_int("timezone", 0);
		int chosen, i;

		read_wifi_config();
		host_network_status(status, sizeof(status));
		if (minutes == 0)
			strcpy(tz, "UTC");
		else
			snprintf(tz, sizeof(tz), "UTC%+d", minutes / 60);
		{
			struct osd_item items[] = {
				{status, NULL, OSD_ITEM_LABEL, -1},
				{"", NULL, OSD_ITEM_LABEL, -1},
				{"Network", device_names[device], OSD_ITEM_SUBMENU, N_DEVICE},
				{"Scan for Wi-Fi Networks", NULL, OSD_ITEM_SUBMENU, N_SCAN},
				{"Wi-Fi Network", wifi_ssid[0] ? wifi_ssid : "(none)", OSD_ITEM_SUBMENU, N_SSID},
				{"Wi-Fi Password", wifi_psk[0] ? "********" : "(none)", OSD_ITEM_SUBMENU, N_PASSWORD},
				{"Wi-Fi Country", wifi_country, OSD_ITEM_SUBMENU, N_COUNTRY},
				{"Web UI", NULL, check(setting_on("webui", TRUE)), N_WEBUI},
				{"Web UI PIN", pin[0] ? pin : "(none)", OSD_ITEM_SUBMENU, N_PIN},
				{"Time Zone", tz, OSD_ITEM_SUBMENU, N_TIMEZONE},
				{"", NULL, OSD_ITEM_LABEL, -1},
				{"Changes apply after a reboot.", NULL, OSD_ITEM_LABEL, -1},
				{"Reboot Now", NULL, 0, N_REBOOT},
			};
			chosen = osd_menu("Network", items, sizeof(items) / sizeof(items[0]), &sel);
		}
		switch (chosen) {
		case N_DEVICE:
			i = osd_choose("Network", device_names, device);
			if (i >= 0 && i != device) {
				save_setting("network", devices[i]);
				reboot_needed = TRUE;
			}
			break;
		case N_SCAN:
			if (scan_wifi() == 2) {
				wifi_psk[0] = '\0';
				edit_wifi("Wi-Fi Password", wifi_psk, sizeof(wifi_psk), TRUE);
			}
			break;
		case N_SSID:
			edit_wifi("Wi-Fi Network Name", wifi_ssid, sizeof(wifi_ssid), FALSE);
			break;
		case N_PASSWORD:
			wifi_psk[0] = '\0';
			edit_wifi("Wi-Fi Password (blank: none)", wifi_psk, sizeof(wifi_psk), TRUE);
			break;
		case N_COUNTRY:
			edit_wifi("Two Letter Country Code", wifi_country, sizeof(wifi_country), FALSE);
			break;
		case N_WEBUI:
			save_setting("webui", setting_on("webui", TRUE) ? "0" : "1");
			reboot_needed = TRUE;
			break;
		case N_PIN: {
			char text[9];
			snprintf(text, sizeof(text), "%s", pin);
			if (osd_edit("Web UI PIN (blank: none)", text, sizeof(text), FALSE)) {
				save_setting("webui_pin", text);
				reboot_needed = TRUE;
			}
			break;
		}
		case N_TIMEZONE: {
			static char labels[27][8];
			static const char *zones[28];
			int h, current = 12;
			for (h = -12; h <= 14; h++) {
				if (h == 0)
					strcpy(labels[h + 12], "UTC");
				else
					snprintf(labels[h + 12], sizeof(labels[0]), "UTC%+d", h);
				zones[h + 12] = labels[h + 12];
				if (h * 60 == minutes)
					current = h + 12;
			}
			zones[27] = NULL;
			h = osd_choose("Time Zone", zones, current);
			if (h >= 0 && h != current) {
				save_setting_int("timezone", (h - 12) * 60);
				reboot_needed = TRUE;
			}
			break;
		}
		case N_REBOOT:
			power(0);
			return;
		default:
			return;
		}
	}
}

/* ---- Information ----------------------------------------------------------- */

extern const char g_kegs_version_str[];   /* sim65816.c */

static void about(void)
{
	char text[640], status[64];
	const char *rom = bmapple2_rom_file();
	int w, h;
	host_display_size(&w, &h);
	host_network_status(status, sizeof(status));
	snprintf(text, sizeof(text),
	         "BMApple2 " BMAPPLE2_VERSION "\n"
	         "Bare metal Apple IIgs emulator\n"
	         "for the Raspberry Pi.\n\n"
	         "Emulation: KEGS %s\n"
	         "by Kent Dickey (GPL 3).\n"
	         "Based on BMCoco, BMA800XL and BMC64.\n\n"
	         "ROM: %s\n"
	         "Display: %dx%d %.2f Hz\n"
	         "IIgs: %.2f Hz, %s\n"
	         "Network: %s",
	         g_kegs_version_str, rom ? rom : "(none)", w, h, bmapple2_refresh_rate(), VBL_RATE,
	         bmapple2_vsync_locked() ? "locked to display" : "timer paced", status);
	osd_info("About BMApple2", text);
}

static void offer_reboot(void)
{
	if (osd_confirm("Reboot?", "Settings changed that need a reboot. Reboot now?"))
		power(0);
}

/* ---- Main menu ---------------------------------------------------------------- */

void bmapple2_menu(void)
{
	enum { M_KEGS, M_RESET, M_RESTART, M_VIDEO, M_SOUND, M_NETWORK, M_ABOUT, M_REBOOT, M_OFF };
	static int sel = 0;
	char title[48];

	snprintf(title, sizeof(title), "BMApple2 %s", BMAPPLE2_VERSION);
	reboot_needed = FALSE;
	osd_open();
	for (;;) {
		struct osd_item items[] = {
			{"KEGS Settings (F4)", NULL, OSD_ITEM_SUBMENU, M_KEGS},
			{"Reset (Ctrl+F12)", NULL, 0, M_RESET},
			{"Restart (boot again)", NULL, 0, M_RESTART},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{"Video", NULL, OSD_ITEM_SUBMENU, M_VIDEO},
			{"Sound", NULL, OSD_ITEM_SUBMENU, M_SOUND},
			{"Network", NULL, OSD_ITEM_SUBMENU, M_NETWORK},
			{"About", NULL, OSD_ITEM_SUBMENU, M_ABOUT},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{"Reboot", NULL, 0, M_REBOOT},
			{"Shut Down", NULL, 0, M_OFF},
			{"", NULL, OSD_ITEM_LABEL, -1},
			{"Disks, ROM, speed: KEGS Settings.", NULL, OSD_ITEM_LABEL, -1},
		};
		int chosen = osd_menu(title, items, sizeof(items) / sizeof(items[0]), &sel);
		int leave = FALSE;

		switch (chosen) {
		case M_KEGS: bmapple2_kegs_config(); leave = TRUE; break;
		case M_RESET: do_reset(); leave = TRUE; break;
		case M_RESTART: bmapple2_restart(); leave = TRUE; break;
		case M_VIDEO: video_menu(); break;
		case M_SOUND: sound_menu(); break;
		case M_NETWORK: network_menu(); break;
		case M_ABOUT: about(); break;
		case M_REBOOT:
			if (osd_confirm("Reboot", "Reboot BMApple2?"))
				power(0);
			break;
		case M_OFF:
			if (osd_confirm("Shut Down", "Shut down? It's then safe to switch off."))
				power(1);
			break;
		default:
			leave = TRUE;
			break;
		}
		if (leave || osd_closing())
			break;
	}
	if (reboot_needed && !osd_closing())
		offer_reboot();
	osd_close();
}

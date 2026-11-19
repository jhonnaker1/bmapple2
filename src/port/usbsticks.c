/*
 * usbsticks.c - USB sticks for KEGS.
 *
 * The kernel mounts up to three FAT USB sticks as the FatFs volumes "USB:",
 * "USB2:" and "USB3:" (src/kernel.cpp, from BMA800XL). KEGS's file browser
 * (F4) only knows the card's folders, so while a stick is in it shows as a
 * folder of that name in the card's top folder ("/USB:"), and a path in it
 * ("/USB:/games/x.po") stands for the stick's ("USB:/games/x.po"). On the
 * Pi, fswrap.c puts the folders in the listing and the paths through
 * bmapple2_usb_path() for every file call KEGS makes; KEGS is unchanged.
 *
 * Pulling a stick out ejects the disks loaded from it (once a second, from
 * platform.c); config.kegs keeps them, so they are loaded again at the
 * next boot with the stick in.
 *
 * This file is part of BMApple2, a bare metal Apple II and IIgs emulator for
 * the Raspberry Pi. See LICENSE for licensing details.
 */

#include "defc.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "../host.h"
#include "bmapple2.h"

/* The folder names the sticks have in the card's top folder. */
static const char *const kVolume[HOST_USB_STICKS] = {"USB:", "USB2:", "USB3:"};

extern int g_config_kegs_update_needed;

/* PATH without the card's root folder ("/" on the Pi, the test's folder in
   the simulator) or the slashes at its start. */
static const char *after_root(const char *path)
{
	const char *root = bmapple2_sd_root();
	size_t len = strlen(root);
	while (len > 0 && root[len - 1] == '/')
		len--;
	if (len > 0 && strncmp(path, root, len) == 0 && path[len] == '/')
		path += len;
	while (*path == '/')
		path++;
	return path;
}

int bmapple2_usb_path(const char *path, char *out, size_t size)
{
	const char *p;
	int n;
	if (path == NULL)
		return -1;
	p = after_root(path);
	for (n = 0; n < HOST_USB_STICKS; n++) {
		size_t len = strlen(kVolume[n]);
		const char *root = host_usb_root(n);
		if (root == NULL || strncasecmp(p, kVolume[n], len) != 0 ||
		    (p[len] != '/' && p[len] != '\0'))
			continue;
		p += len;
		while (*p == '/')
			p++;
		if ((size_t)snprintf(out, size, "%s%s", root, p) >= size)
			return -1;
		return n;
	}
	return -1;
}

int bmapple2_usb_stick_of(const char *path)
{
	char real[1024];
	int n = bmapple2_usb_path(path, real, sizeof(real));
	if (n >= 0 || path == NULL)
		return n;
	/* A stick's own path ("USB:/x.po", or "usb/x.po" in the simulator) */
	while (path[0] == '.' && path[1] == '/')
		path += 2;
	for (n = 0; n < HOST_USB_STICKS; n++) {
		const char *root = host_usb_root(n);
		if (root != NULL && strncasecmp(path, root, strlen(root)) == 0)
			return n;
	}
	return -1;
}

int bmapple2_usb_is_card_root(const char *path)
{
	const char *p;
	if (path == NULL)
		return 0;
	p = after_root(path);
	if (strncasecmp(p, "SD:", 3) == 0)
		p += 3;
	while (*p == '/')
		p++;
	return *p == '\0' || strcmp(p, ".") == 0 || strcmp(p, "./") == 0;
}

const char *bmapple2_usb_folder(int i)
{
	int n;
	for (n = 0; n < HOST_USB_STICKS; n++) {
		if (host_usb_stick(n) != 0 && i-- == 0)
			return kVolume[n];
	}
	return NULL;
}

/* Ejects the disks loaded from stick N, in slots 5, 6 and 7. */
static void eject_stick(int n)
{
	int saved = g_config_kegs_update_needed;
	int ejected = 0;
	int slot, drive;
	for (slot = 5; slot <= 7; slot++) {
		int drives = slot == 7 ? MAX_C7_DISKS : 2;
		for (drive = 0; drive < drives; drive++) {
			Disk *dsk = iwm_get_dsk_from_slot_drive(slot, drive);
			if (dsk->fd < 0 || dsk->name_ptr == NULL ||
			    bmapple2_usb_stick_of(dsk->name_ptr) != n)
				continue;
			iwm_eject_disk(dsk);
			ejected++;
		}
	}
	/* Ejecting asks KEGS to save config.kegs; it keeps them for the next
	   boot instead. */
	g_config_kegs_update_needed = saved;
	printf("BMApple2: USB stick %s removed: %d ejected\n", kVolume[n], ejected);
}

void bmapple2_check_usb_sticks(void)
{
	static unsigned last[HOST_USB_STICKS];
	int n;
	for (n = 0; n < HOST_USB_STICKS; n++) {
		unsigned now = host_usb_stick(n);
		if (last[n] != 0 && now != last[n])
			eject_stick(n);
		last[n] = now;
	}
}

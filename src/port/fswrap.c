/*
 * BMA800XL - A bare metal Atari 800XL emulator for Raspberry Pi
 * Copyright (C) 2026 aminch <https://github.com/aminch>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Modified for BMApple2.
 *
 * fswrap.c - file system fixes for KEGS on Circle's FatFs.
 *
 * The kernel is linked with --wrap for the file calls below, so KEGS's
 * calls (and BMApple2's) come here first, then go to the real ones.
 *
 * FatFs cannot stat() the root directory of a volume, so KEGS's file
 * browser would think "/" doesn't exist and refuse to go up to it: the
 * root (and ".") are reported as directories.
 *
 * The USB sticks (usbsticks.c): the card's top folder lists a folder for
 * each stick plugged in ("USB:", "USB2:", "USB3:"), and a path in one
 * ("/USB:/games/x.po") is the stick's ("USB:/games/x.po").
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../host.h"
#include "bmapple2.h"

int __real_stat(const char *path, struct stat *st);
int __real_lstat(const char *path, struct stat *st);
int __real_open(const char *path, int flags, ...);
FILE *__real_fopen(const char *path, const char *mode);
DIR *__real_opendir(const char *path);
struct dirent *__real_readdir(DIR *dir);
int __real_closedir(DIR *dir);
int __real_unlink(const char *path);
int __real_rename(const char *from, const char *to);
int __real_mkdir(const char *path, mode_t mode);

/* A path, with one in a stick's folder made the stick's own. BUF holds it
   when it is changed. */
#define PATH_BUF 1024
static const char *real_path(const char *path, char *buf)
{
	return bmapple2_usb_path(path, buf, PATH_BUF) >= 0 ? buf : path;
}

static int is_root(const char *path)
{
	const char *colon = strchr(path, ':');
	/* Skip a volume name such as "SD:" */
	if (colon != NULL)
		path = colon + 1;
	while (*path == '/')
		path++;
	return *path == '\0' || strcmp(path, ".") == 0 || strcmp(path, "./") == 0;
}

static int do_stat(const char *path, struct stat *st, int link)
{
	char buf[PATH_BUF];
	int stick;
	if (path == NULL || st == NULL)
		return link ? __real_lstat(path, st) : __real_stat(path, st);
	stick = bmapple2_usb_path(path, buf, sizeof(buf));
	if (stick >= 0) {
		if (host_usb_stick(stick) == 0) {
			errno = ENOENT;
			return -1;
		}
		path = buf;
	}
	if (is_root(path)) {
		memset(st, 0, sizeof(*st));
		st->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
		st->st_nlink = 1;
		return 0;
	}
	return link ? __real_lstat(path, st) : __real_stat(path, st);
}

int __wrap_stat(const char *path, struct stat *st) { return do_stat(path, st, 0); }

int __wrap_lstat(const char *path, struct stat *st) { return do_stat(path, st, 1); }

int __wrap_open(const char *path, int flags, ...)
{
	char buf[PATH_BUF];
	int mode = 0;
	if (flags & O_CREAT) {
		va_list ap;
		va_start(ap, flags);
		mode = va_arg(ap, int);
		va_end(ap);
	}
	return __real_open(real_path(path, buf), flags, mode);
}

FILE *__wrap_fopen(const char *path, const char *mode)
{
	char buf[PATH_BUF];
	return __real_fopen(real_path(path, buf), mode);
}

int __wrap_unlink(const char *path)
{
	char buf[PATH_BUF];
	return __real_unlink(real_path(path, buf));
}

int __wrap_rename(const char *from, const char *to)
{
	char buf[PATH_BUF], buf2[PATH_BUF];
	return __real_rename(real_path(from, buf), real_path(to, buf2));
}

int __wrap_mkdir(const char *path, mode_t mode)
{
	char buf[PATH_BUF];
	return __real_mkdir(real_path(path, buf), mode);
}

/* Listings of the card's top folder, which get the sticks' folders after
   the card's own entries: the next stick to list for each. */
#define ROOT_LISTINGS 4
static struct {
	DIR *dir;
	int next;
} root_listing[ROOT_LISTINGS];
static struct dirent stick_entry;

DIR *__wrap_opendir(const char *path)
{
	char buf[PATH_BUF];
	DIR *dir = __real_opendir(real_path(path, buf));
	int i;
	if (dir != NULL && bmapple2_usb_is_card_root(path)) {
		for (i = 0; i < ROOT_LISTINGS; i++) {
			if (root_listing[i].dir == NULL) {
				root_listing[i].dir = dir;
				root_listing[i].next = 0;
				break;
			}
		}
	}
	return dir;
}

struct dirent *__wrap_readdir(DIR *dir)
{
	struct dirent *entry = __real_readdir(dir);
	const char *folder;
	int i;
	if (entry != NULL || dir == NULL)
		return entry;
	for (i = 0; i < ROOT_LISTINGS; i++) {
		if (root_listing[i].dir != dir)
			continue;
		folder = bmapple2_usb_folder(root_listing[i].next);
		if (folder == NULL)
			return NULL;
		root_listing[i].next++;
		memset(&stick_entry, 0, sizeof(stick_entry));
		strncpy(stick_entry.d_name, folder, sizeof(stick_entry.d_name) - 1);
		return &stick_entry;
	}
	return NULL;
}

int __wrap_closedir(DIR *dir)
{
	int i;
	for (i = 0; i < ROOT_LISTINGS; i++) {
		if (root_listing[i].dir == dir)
			root_listing[i].dir = NULL;
	}
	return __real_closedir(dir);
}

/*
 * keymap.c - USB keyboard keys to Apple ADB key codes for KEGS.
 *
 * The kernel reports keys as USB HID usage codes; KEGS takes Apple Desktop
 * Bus key codes, the ones Mac keyboards send (KEGS's Mac front end passes
 * them on as they are). The ADB codes are from Apple's "Inside Macintosh:
 * Macintosh Toolbox Essentials", figure 2-10 (virtual key codes), which
 * KEGS's own X11 table (src/xdriver.c) agrees with.
 *
 * Apple's Command (open apple) key is the PC's Windows key, or a Mac
 * keyboard's command key; Option (solid apple) is Alt. F1 and F2 are
 * Command and Option too (KEGS does that).
 *
 * This file is part of BMApple2, a bare metal Apple II and IIgs emulator for
 * the Raspberry Pi. See LICENSE for licensing details.
 */

#include "bmapple2.h"

/* Indexed by USB HID usage (see has_code()). */
static const unsigned char usage_to_adb[256] = {
	[0x04] = 0x00, /* a */  [0x05] = 0x0b, /* b */  [0x06] = 0x08, /* c */
	[0x07] = 0x02, /* d */  [0x08] = 0x0e, /* e */  [0x09] = 0x03, /* f */
	[0x0a] = 0x05, /* g */  [0x0b] = 0x04, /* h */  [0x0c] = 0x22, /* i */
	[0x0d] = 0x26, /* j */  [0x0e] = 0x28, /* k */  [0x0f] = 0x25, /* l */
	[0x10] = 0x2e, /* m */  [0x11] = 0x2d, /* n */  [0x12] = 0x1f, /* o */
	[0x13] = 0x23, /* p */  [0x14] = 0x0c, /* q */  [0x15] = 0x0f, /* r */
	[0x16] = 0x01, /* s */  [0x17] = 0x11, /* t */  [0x18] = 0x20, /* u */
	[0x19] = 0x09, /* v */  [0x1a] = 0x0d, /* w */  [0x1b] = 0x07, /* x */
	[0x1c] = 0x10, /* y */  [0x1d] = 0x06, /* z */
	[0x1e] = 0x12, /* 1 */  [0x1f] = 0x13, /* 2 */  [0x20] = 0x14, /* 3 */
	[0x21] = 0x15, /* 4 */  [0x22] = 0x17, /* 5 */  [0x23] = 0x16, /* 6 */
	[0x24] = 0x1a, /* 7 */  [0x25] = 0x1c, /* 8 */  [0x26] = 0x19, /* 9 */
	[0x27] = 0x1d, /* 0 */
	[0x28] = 0x24, /* Return */     [0x29] = 0x35, /* Escape */
	[0x2a] = 0x33, /* Backspace: Delete */  [0x2b] = 0x30, /* Tab */
	[0x2c] = 0x31, /* Space */      [0x2d] = 0x1b, /* - */
	[0x2e] = 0x18, /* = */          [0x2f] = 0x21, /* [ */
	[0x30] = 0x1e, /* ] */          [0x31] = 0x2a, /* \ */
	[0x32] = 0x2a, /* non-US # */   [0x33] = 0x29, /* ; */
	[0x34] = 0x27, /* ' */          [0x35] = 0x32, /* ` */
	[0x36] = 0x2b, /* , */          [0x37] = 0x2f, /* . */
	[0x38] = 0x2c, /* / */          [0x39] = 0x39, /* Caps Lock */
	[0x3a] = 0x7a, /* F1 */  [0x3b] = 0x78, /* F2 */  [0x3c] = 0x63, /* F3 */
	[0x3d] = 0x76, /* F4 */  [0x3e] = 0x60, /* F5 */  [0x3f] = 0x61, /* F6 */
	[0x40] = 0x62, /* F7 */  [0x41] = 0x64, /* F8 */  [0x42] = 0x65, /* F9 */
	/* F10 is BMApple2's menu (platform.c). */
	[0x44] = 0x67, /* F11 */ [0x45] = 0x6f, /* F12: KEGS's Reset */
	[0x46] = 0x69, /* Print Screen: F13 */
	[0x48] = 0x71, /* Pause: F15 */
	[0x49] = 0x72, /* Insert: Help */  [0x4a] = 0x73, /* Home */
	[0x4b] = 0x74, /* Page Up */    [0x4c] = 0x75, /* Delete (forward) */
	[0x4d] = 0x77, /* End */        [0x4e] = 0x79, /* Page Down */
	[0x4f] = 0x3c, /* Right */      [0x50] = 0x3b, /* Left */
	[0x51] = 0x3d, /* Down */       [0x52] = 0x3e, /* Up */
	[0x53] = 0x47, /* Num Lock: keypad Clear */
	[0x54] = 0x4b, /* keypad / */   [0x55] = 0x43, /* keypad * */
	[0x56] = 0x4e, /* keypad - */   [0x57] = 0x45, /* keypad + */
	[0x58] = 0x4c, /* keypad Enter */
	[0x59] = 0x53, /* keypad 1 */   [0x5a] = 0x54, /* keypad 2 */
	[0x5b] = 0x55, /* keypad 3 */   [0x5c] = 0x56, /* keypad 4 */
	[0x5d] = 0x57, /* keypad 5 */   [0x5e] = 0x58, /* keypad 6 */
	[0x5f] = 0x59, /* keypad 7 */   [0x60] = 0x5b, /* keypad 8 */
	[0x61] = 0x5c, /* keypad 9 */   [0x62] = 0x52, /* keypad 0 */
	[0x63] = 0x41, /* keypad . */   [0x64] = 0x0a, /* ISO \ (section) */
	[0x67] = 0x51, /* keypad = */
	[0xe0] = 0x36, /* left Control */  [0xe1] = 0x38, /* left Shift */
	[0xe2] = 0x3a, /* left Alt: Option */
	[0xe3] = 0x37, /* left Windows/command: Command */
	[0xe4] = 0x36, /* right Control */ [0xe5] = 0x38, /* right Shift */
	[0xe6] = 0x3a, /* right Alt: Option */
	[0xe7] = 0x37, /* right Windows/command: Command */
};

/* Which usages are in the table (0x00 is a real ADB code, the A key, so a
   zero entry can't mean "none"). */
static int has_code(int usage)
{
	return (usage >= 0x04 && usage <= 0x42) || (usage >= 0x44 && usage <= 0x46)
	    || (usage >= 0x48 && usage <= 0x64) || usage == 0x67
	    || (usage >= 0xe0 && usage <= 0xe7);
}

int bmapple2_usage_to_adb(int usage)
{
	if (usage < 0 || usage > 0xff || !has_code(usage))
		return -1;
	return usage_to_adb[usage];
}

/*
 * joystick.c - the Apple II's joysticks and paddles for BMApple2.
 *
 * Replaces KEGS's joystick_driver.c (which reads Linux's /dev/js0 or Mac
 * HID devices) with the same three functions, fed from USB gamepads and the
 * GPIO header like KEGS's Mac version is from its HID callback.
 *
 * The Apple II's paddles are analog, like the CoCo's joysticks: a gamepad's
 * left stick gives the position as it is; the D-pad and GPIO joysticks
 * push it to the edges.
 *
 *   gamepad 1 / GPIO 1: paddles 0 and 1 (the joystick), buttons 0 (A) and
 *                       1 (B)
 *   gamepad 2 / GPIO 2: paddles 2 and 3, button 2 (A)
 *
 * This file is part of BMApple2, a bare metal Apple II and IIgs emulator for
 * the Raspberry Pi. See LICENSE for licensing details.
 */

#include "defc.h"

#include "../host.h"
#include "bmapple2.h"

extern int g_joystick_native_type1;   /* paddles.c */
extern int g_joystick_native_type2;
extern int g_paddle_buttons;
extern int g_paddle_val[];

void paddle_update_trigger_dcycs(dword64 dfcyc);

/* Past a quarter of the range a stick also reports D-pad presses (the
   kernel); nearer the middle it is only a position. */
#define STICK_THRESHOLD 8192

/* One axis, -32768 (left/up) to 32767 (right/down), as KEGS's paddles take
   it. */
static int axis(int pad, int axis_index, int state, int gpio)
{
	int neg_pad = axis_index == 0 ? HOST_PAD_LEFT : HOST_PAD_UP;
	int pos_pad = axis_index == 0 ? HOST_PAD_RIGHT : HOST_PAD_DOWN;
	int neg_gpio = axis_index == 0 ? HOST_JOY_LEFT : HOST_JOY_UP;
	int pos_gpio = axis_index == 0 ? HOST_JOY_RIGHT : HOST_JOY_DOWN;
	int analog = host_pad_axis(pad, axis_index);

	if (gpio & neg_gpio)
		return -32768;
	if (gpio & pos_gpio)
		return 32767;
	if (analog >= 0) {
		int offset = analog - 32768;
		if (offset > STICK_THRESHOLD || offset < -STICK_THRESHOLD
		    || !(state & (neg_pad | pos_pad)))
			return offset > 32767 ? 32767 : offset;
	}
	if (state & neg_pad)
		return -32768;
	if (state & pos_pad)
		return 32767;
	return 0;
}

void joystick_init(void)
{
	/* Always there: a gamepad or GPIO joystick can be plugged in at any
	   time, and with none the sticks rest in the middle. */
	g_joystick_native_type1 = 1;
	g_joystick_native_type2 = 1;
}

static int pad_buttons(void)
{
	int buttons = 0;
	int pad0 = host_pad_state(0), pad1 = host_pad_state(1);
	if (pad0 < 0)
		pad0 = 0;
	if (pad1 < 0)
		pad1 = 0;
	if ((pad0 & HOST_PAD_A) || (host_gpio_joystick(0) & HOST_JOY_FIRE))
		buttons |= 1;
	if (pad0 & HOST_PAD_B)
		buttons |= 2;
	if ((pad1 & HOST_PAD_A) || (host_gpio_joystick(1) & HOST_JOY_FIRE))
		buttons |= 4;
	return buttons;
}

void joystick_update(dword64 dfcyc)
{
	int pad;
	for (pad = 0; pad < 2; pad++) {
		int state = host_pad_state(pad);
		int gpio = host_gpio_joystick(pad);
		if (state < 0)
			state = 0;
		g_paddle_val[pad * 2] = axis(pad, 0, state, gpio);
		g_paddle_val[pad * 2 + 1] = axis(pad, 1, state, gpio);
	}
	/* Bits 2 and 3 high: as KEGS's own drivers do with no third or
	   fourth button. */
	g_paddle_buttons = 0x8 | pad_buttons();
	paddle_update_trigger_dcycs(dfcyc);
}

void joystick_update_buttons(void)
{
	g_paddle_buttons = 0x8 | pad_buttons();
}

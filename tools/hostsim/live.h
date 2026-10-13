/*
 * live.h - the host simulator's live mode (see live.c).
 *
 * This file is part of BMApple2, a bare metal Apple IIgs emulator for the
 * the Raspberry Pi. See LICENSE for licensing details.
 */

#ifndef BMAPPLE2_LIVE_H_
#define BMAPPLE2_LIVE_H_

#include <stdint.h>

int live_init(int port);
void live_poll(void);
void live_audio(const int16_t *samples, unsigned n);

/* From host_sim.c */
void sim_push_key(int usage, int pressed);
void sim_set_pad(int pad, int bits, int x, int y);
const uint16_t *sim_screen(int *w, int *h, int *pitch);

#endif

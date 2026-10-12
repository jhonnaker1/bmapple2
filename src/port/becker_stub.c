/*
 * becker_stub.c - XRoar's Becker port, switched off.
 *
 * The Becker port connects the CoCo to a DriveWire server over TCP. XRoar's
 * becker.c includes the BSD socket headers even when the port is built out
 * (no WANT_BECKER), and circle-stdlib's don't compile on their own, so
 * BMCoco builds this instead: the same external definitions of the inline
 * functions in becker.h that becker.c has without WANT_BECKER.
 *
 * This file is part of BMCoco, a bare metal Tandy Color Computer emulator for
 * the Raspberry Pi. See LICENSE for licensing details.
 */

#include "top-config.h"

#include <stdint.h>
#include <stdlib.h>

#include "becker.h"

extern inline struct becker *becker_open(void);
extern inline void becker_close(struct becker *b);
extern inline void becker_reset(struct becker *b);
extern inline uint8_t becker_read_status(struct becker *b);
extern inline uint8_t becker_read_data(struct becker *b);
extern inline void becker_write_data(struct becker *b, uint8_t D);

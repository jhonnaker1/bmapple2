/*
 * scc_stub.c - KEGS's real serial port driver, for a Pi with none.
 *
 * Replaces scc_unixdriver.c, which opens a host serial device and sets it
 * up with termios (which circle-newlib lacks). The IIgs's serial ports can
 * still talk to the network (KEGS's socket driver, scc_socket_driver.c) or
 * to nothing; asking for a real device fails as it would on a computer
 * without one.
 *
 * This file is part of BMApple2, a bare metal Apple II and IIgs emulator for
 * the Raspberry Pi. See LICENSE for licensing details.
 */

#include "defc.h"
#include "scc.h"

extern Scc g_scc[2];

void scc_serial_unix_open(int port)
{
	g_scc[port].unix_dev_fd = -1;
	g_scc[port].cur_state = -1;     /* failed to open */
}

void scc_serial_unix_close(int port)
{
	g_scc[port].unix_dev_fd = -1;
}

void scc_serial_unix_change_params(int port)
{
}

void scc_serial_unix_fill_readbuf(dword64 dfcyc, int port, int space_left)
{
}

void scc_serial_unix_empty_writebuf(int port)
{
}

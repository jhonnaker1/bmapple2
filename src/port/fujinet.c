/*
 * fujinet.c - a FujiNet's devices on slot 7's SmartPort.
 *
 * FujiNet-PC, FujiNet's program for a computer, built for the Apple II,
 * offers its devices (its disks, network, clock, printer, modem and the
 * FujiNet itself, which its CONFIG program talks to) as SmartPort units,
 * over TCP: "SmartPort over SLIP", as FujiNet Go's AppleWin has. FujiNet
 * connects to the emulator, here on port 1985 (the "fujinet_port"
 * setting) when "fujinet" is on. smartport.c (BMApple2's copy of KEGS's)
 * puts FujiNet's units first on slot 7, and sends the Apple's calls to
 * them here.
 *
 * The protocol, from FujiNet's own code (lib/devrelay in fujinet-firmware):
 * each packet is SLIP framed (END $C0 before and after; $C0 and $DB in it
 * sent as $DB $DC and $DB $DD). A request is a sequence number, the
 * SmartPort command, its parameter count, the unit, two zeros, five bytes
 * of parameters (those after the command list's buffer pointer), then the
 * data written, if any. The answer is the sequence number, the status (a
 * SmartPort error, 0 if none) and the data read, if any. When FujiNet
 * connects, its units are found by sending INIT to unit 1, 2, ... until
 * one answers with a status not 0: that one is the last.
 *
 * The Apple waits for each answer (at most 5 seconds, as FujiNet Go does),
 * while the network keeps running (host_tcp_wait()).
 *
 * This file is part of BMApple2, a bare metal Apple II and IIgs emulator for
 * the Raspberry Pi. See LICENSE for licensing details.
 */

#include <stdio.h>
#include <string.h>

#include "../host.h"
#include "bmapple2.h"

#define SLIP_END      0xC0
#define SLIP_ESC      0xDB
#define SLIP_ESC_END  0xDC
#define SLIP_ESC_ESC  0xDD

#define SP_INIT 5

#define REPLY_TIMEOUT_MS 5000
#define MAX_UNITS 32
#define MAX_PACKET 8192

static int enabled = -1;       /* "fujinet": -1 until read */
static int port;               /* "fujinet_port" */
static int listening;
static int connection = -1;    /* FujiNet's, or -1 */
static int units;              /* its units, 0 until found */
static uint8_t sequence;

/* The packet being received, SLIP decoded. */
static uint8_t in[MAX_PACKET];
static int in_len, in_escape, in_started, in_long;
/* Bytes received and not yet looked at: what follows a packet's end is
   kept for the next. */
static uint8_t rx[512];
static int rx_pos, rx_got;

int bmapple2_fujinet_enabled(void)
{
	if (enabled < 0) {
		const char *on = host_option("fujinet", "0");
		enabled = strcmp(on, "1") == 0 || strcmp(on, "yes") == 0
		       || strcmp(on, "true") == 0 || strcmp(on, "on") == 0;
		port = host_option_int("fujinet_port", 1985);
	}
	return enabled;
}

int bmapple2_fujinet_port(void)
{
	bmapple2_fujinet_enabled();
	return port;
}

int bmapple2_fujinet_units(void)
{
	return connection >= 0 ? units : 0;
}

static void drop(const char *why)
{
	if (connection >= 0) {
		printf("FujiNet: %s; disconnected\n", why);
		host_tcp_close(connection);
	}
	connection = -1;
	units = 0;
	in_started = 0;
	rx_pos = rx_got = 0;
}

static int send_all(const uint8_t *bytes, int n)
{
	int waited = 0;
	while (n > 0) {
		int sent = host_tcp_send(connection, bytes, (unsigned) n);
		if (sent < 0)
			return 0;
		if (sent == 0) {
			if (++waited > REPLY_TIMEOUT_MS)
				return 0;
			host_sleep_us(1000);
			continue;
		}
		bytes += sent;
		n -= sent;
	}
	return 1;
}

/* Sends PACKET, SLIP framed. */
static int send_packet(const uint8_t *packet, int n)
{
	uint8_t out[2 * 600 + 2];
	int i, o = 0;
	out[o++] = SLIP_END;
	for (i = 0; i < n; i++) {
		if (o > (int) sizeof(out) - 3) {
			if (!send_all(out, o))
				return 0;
			o = 0;
		}
		if (packet[i] == SLIP_END) {
			out[o++] = SLIP_ESC;
			out[o++] = SLIP_ESC_END;
		} else if (packet[i] == SLIP_ESC) {
			out[o++] = SLIP_ESC;
			out[o++] = SLIP_ESC_ESC;
		} else {
			out[o++] = packet[i];
		}
	}
	out[o++] = SLIP_END;
	return send_all(out, o);
}

/* Reads what has come in; returns 1 when a whole packet is in IN (IN_LEN
   bytes), 0 if not yet, -1 if the connection is gone. */
static int receive_packet(void)
{
	for (;;) {
		if (rx_pos >= rx_got) {
			rx_pos = 0;
			rx_got = host_tcp_recv(connection, rx, sizeof(rx));
			if (rx_got < 0) {
				rx_got = 0;
				return -1;
			}
			if (rx_got == 0)
				return 0;
		}
		while (rx_pos < rx_got) {
			uint8_t b = rx[rx_pos++];
			if (b == SLIP_END) {
				if (in_started && in_len > 0) {
					in_started = 0;
					if (in_long)
						continue;
					return 1;
				}
				in_started = 1;
				in_len = in_escape = in_long = 0;
				continue;
			}
			if (!in_started)
				continue;
			if (in_escape) {
				b = b == SLIP_ESC_END ? SLIP_END : b == SLIP_ESC_ESC ? SLIP_ESC : b;
				in_escape = 0;
			} else if (b == SLIP_ESC) {
				in_escape = 1;
				continue;
			}
			if (in_len < MAX_PACKET)
				in[in_len++] = b;
			else
				in_long = 1;
		}
	}
}

int bmapple2_fujinet_request(int cmd, int pcount, int unit, const uint8_t *params,
                             const uint8_t *data, int data_len,
                             uint8_t *reply, int max, int *reply_len)
{
	static uint8_t packet[11 + 1024];
	int i, r;

	*reply_len = 0;
	if (connection < 0)
		return -1;
	if (data_len > (int) sizeof(packet) - 11)
		data_len = (int) sizeof(packet) - 11;
	sequence++;
	memset(packet, 0, 11);
	packet[0] = sequence;
	packet[1] = (uint8_t) cmd;
	packet[2] = (uint8_t) pcount;
	packet[3] = (uint8_t) unit;
	if (params != NULL)
		memcpy(packet + 6, params, 5);
	if (data_len > 0)
		memcpy(packet + 11, data, (size_t) data_len);
	if (!send_packet(packet, 11 + data_len)) {
		drop("sending failed");
		return -1;
	}
	for (;;) {
		r = receive_packet();
		if (r < 0) {
			drop("FujiNet closed the connection");
			return -1;
		}
		if (r > 0) {
			if (in_len >= 2 && in[0] == sequence)
				break;
			continue;   /* an old answer */
		}
		r = host_tcp_wait(connection, REPLY_TIMEOUT_MS);
		if (r < 0) {
			drop("FujiNet closed the connection");
			return -1;
		}
		if (r == 0) {
			drop("no answer");
			return -1;
		}
	}
	for (i = 2; i < in_len && i - 2 < max; i++)
		reply[i - 2] = in[i];
	*reply_len = i - 2;
	return in[1];
}

/* A new connection: find FujiNet's units. */
static void scan(void)
{
	int unit, status, len;
	uint8_t reply[16];
	units = MAX_UNITS;   /* for the requests below */
	for (unit = 1; unit <= MAX_UNITS; unit++) {
		status = bmapple2_fujinet_request(SP_INIT, 1, unit, NULL, NULL, 0,
		                                  reply, sizeof(reply), &len);
		if (status < 0)
			return;   /* dropped */
		if (status != 0)
			break;
	}
	units = unit > MAX_UNITS ? MAX_UNITS : unit;
	printf("FujiNet: connected, %d devices\n", units);
}

void bmapple2_fujinet_poll(void)
{
	int c;
	if (!bmapple2_fujinet_enabled())
		return;
	if (!listening)
		listening = host_tcp_listen(port) ? 1 : -1;
	if (listening < 0)
		return;
	c = host_tcp_accept();
	if (c < 0)
		return;
	/* FujiNet-PC started again: the new connection replaces the old. */
	if (connection >= 0)
		drop("FujiNet connected again");
	connection = c;
	in_started = 0;
	rx_pos = rx_got = 0;
	scan();
}

int bmapple2_fujinet_wait(double seconds)
{
	double end;
	if (!bmapple2_fujinet_enabled())
		return 0;
	end = host_time() + seconds;
	while (bmapple2_fujinet_units() == 0 && host_time() < end) {
		bmapple2_fujinet_poll();
		if (bmapple2_fujinet_units() == 0)
			host_sleep_us(20000);
	}
	return bmapple2_fujinet_units() > 0;
}

void bmapple2_fujinet_status(char *text, size_t size)
{
	if (!bmapple2_fujinet_enabled())
		snprintf(text, size, "Off");
	else if (listening < 0)
		snprintf(text, size, "Can't use port %d", port);
	else if (bmapple2_fujinet_units() > 0)
		snprintf(text, size, "Connected, %d devices", units);
	else
		snprintf(text, size, "Waiting on port %d", port);
}

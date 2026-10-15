/*
 * snddrv.c - KEGS's sound output for BMApple2.
 *
 * Replaces KEGS's sound_driver.c, which sends the sound to a child process,
 * PulseAudio, Core Audio or Windows, with the same functions writing to the
 * kernel's sound output (host.h). KEGS's sound.c fills a ring of
 * SOUND_SHM_SAMP_SIZE stereo samples (16 bits left, 16 bits right, in one
 * 32-bit word) and calls snddrv_send_sound() once a frame with how many to
 * play, or how many zeroes when the IIgs is silent; this is
 * child_sound_playit() from sound_driver.c, without the child.
 *
 * The emulation runs at the display's pace or a timer's, never quite the
 * sound output's, so the output is kept about half full by dropping or
 * repeating a sample when it strays (as BMCoco does).
 *
 * This file is part of BMApple2, a bare metal Apple II and IIgs emulator for
 * the Raspberry Pi. See LICENSE for licensing details.
 */

#include "defc.h"
#include "sound.h"

#include "../host.h"
#include "bmapple2.h"

#define AO_RATE 48000

extern int g_audio_enable;
extern int g_audio_rate;
extern word32 *g_sound_shm_addr;

void sound_set_audio_rate(int rate);

/* Read by sim65816.c. */
int g_preferred_rate = AO_RATE;

static int ring_pos;            /* next sample to play in g_sound_shm_addr */
static unsigned out_rate;       /* 0: no sound */
static unsigned out_latency;    /* frames */
static int16_t silence[2048 * 2];
static int volume = -1;                  /* 0-100, -1 until read */
static int16_t scaled[1024 * 2];

/* The volume BMApple2 plays at, on top of the IIgs's own (its Control
   Panel's; a IIe has none). */
int bmapple2_volume(void)
{
	if (volume < 0) {
		volume = host_option_int("volume", 100);
		if (volume < 0)
			volume = 0;
		if (volume > 100)
			volume = 100;
	}
	return volume;
}

void bmapple2_set_volume(int v)
{
	char text[8];
	volume = v < 0 ? 0 : v > 100 ? 100 : v;
	snprintf(text, sizeof text, "%d", volume);
	if (!host_option_set("volume", text) || !host_options_save())
		printf("BMApple2: cannot save volume= in bmapple2.cfg\n");
}

/* N stereo frames to the output, at the volume. */
static void write_frames(const int16_t *samples, unsigned n)
{
	int v = bmapple2_volume();
	if (v >= 100) {
		host_sound_write(samples, n);
		return;
	}
	while (n > 0) {
		unsigned chunk = n > 1024 ? 1024 : n, i;
		for (i = 0; i < chunk * 2; i++)
			scaled[i] = (int16_t) (samples[i] * v / 100);
		host_sound_write(scaled, chunk);
		samples += chunk * 2;
		n -= chunk;
	}
}

void bmapple2_sound_open(void)
{
	unsigned ms = (unsigned) host_option_int("audio_buffer_ms", 60);
	if (ms < 20)
		ms = 20;
	if (ms > 500)
		ms = 500;
	out_latency = AO_RATE * ms / 1000;
	out_rate = host_sound_open(AO_RATE, 2, out_latency);
	if (out_rate == 0)
		printf("BMApple2: no sound\n");
}

void bmapple2_feed_silence(void)
{
	unsigned frames;
	if (out_rate == 0)
		return;
	frames = host_sound_free_frames();
	if (frames > out_latency / 2) {
		frames -= out_latency / 2;
		while (frames > 0) {
			unsigned n = frames > 2048 ? 2048 : frames;
			host_sound_write(silence, n);
			frames -= n;
		}
	}
}

/* N stereo frames to the output, a frame dropped or repeated as needed. */
static void play(const int16_t *samples, unsigned n)
{
	static int16_t extra[2];
	unsigned free_frames, used;
	if (out_rate == 0 || n == 0)
		return;
	free_frames = host_sound_free_frames();
	used = free_frames < out_latency ? out_latency - free_frames : 0;
	if (used > (out_latency * 3) / 4 && n > 1)
		n--;                          /* getting full: one less */
	if (n > free_frames)
		n = free_frames;          /* no room at all: drop the rest */
	if (n > 0)
		write_frames(samples, n);
	if (used < out_latency / 4 && n > 0) {
		/* running dry: the last frame once more */
		extra[0] = samples[(n - 1) * 2];
		extra[1] = samples[(n - 1) * 2 + 1];
		write_frames(extra, 1);
	}
}

void snddrv_init(void)
{
	if (g_audio_enable == 0) {
		sound_set_audio_rate(g_preferred_rate);
		return;
	}
	g_sound_shm_addr = calloc(SOUND_SHM_SAMP_SIZE, SAMPLE_CHAN_SIZE);
	ring_pos = 0;
	bmapple2_sound_open();
	sound_set_audio_rate(out_rate ? (int) out_rate : g_preferred_rate);
}

void snddrv_shutdown(void)
{
}

void snddrv_send_sound(int real_samps, int size)
{
	if (g_audio_enable == 0 || g_sound_shm_addr == 0)
		return;
	if (real_samps) {
		/* The ring may wrap. */
		while (size > 0) {
			int n = size;
			if (ring_pos + n > SOUND_SHM_SAMP_SIZE)
				n = SOUND_SHM_SAMP_SIZE - ring_pos;
			play((const int16_t *) &g_sound_shm_addr[ring_pos], (unsigned) n);
			ring_pos = (ring_pos + n) % SOUND_SHM_SAMP_SIZE;
			size -= n;
		}
	} else {
		/* Silence. */
		ring_pos = (ring_pos + size) % SOUND_SHM_SAMP_SIZE;
		while (size > 0) {
			int n = size > 2048 ? 2048 : size;
			play(silence, (unsigned) n);
			size -= n;
		}
	}
}

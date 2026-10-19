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
 * sound output's (a IIgs's 59.92 Hz locked to a 60 Hz display is 0.13%
 * fast), so the output is kept about half full: the sound is squeezed or
 * stretched a little, the more the further the buffer is from half full,
 * up to 2% (as BMCoco does). Dropping a whole sample, or what doesn't fit,
 * clicks.
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
	/* The output may run at another rate (the VideoCore's 44.1 kHz on the
	   Pi Zero to 3): KEGS then makes its sound at that rate, and the
	   buffer is counted in its frames. The latency is asked for in
	   44.1 kHz frames (host.h). */
	out_rate = host_sound_open(AO_RATE, 2, 44100 * ms / 1000);
	out_latency = (out_rate ? out_rate : AO_RATE) * ms / 1000;
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

#define PLAY_CHUNK 512

/* N stereo frames resampled to WANT (linear interpolation, in 16.16 fixed
   point) into OUT. */
static void resample(const int16_t *in, unsigned n, int16_t *out, unsigned want)
{
	unsigned i, ch;
	for (i = 0; i < want; i++) {
		uint32_t pos = want > 1 ? (uint32_t) (((uint64_t) i * (n - 1) << 16) / (want - 1)) : 0;
		unsigned k = pos >> 16;
		int frac = (int) (pos & 0xffff);
		for (ch = 0; ch < 2; ch++) {
			int a = in[k * 2 + ch];
			int b = k + 1 < n ? in[(k + 1) * 2 + ch] : a;
			out[i * 2 + ch] = (int16_t) (a + (((b - a) * frac) >> 16));
		}
	}
}

/* Up to PLAY_CHUNK stereo frames to the output, squeezed or stretched to
   keep the buffer about half full. */
static void play_chunk(const int16_t *samples, unsigned n)
{
	static int16_t buf[(PLAY_CHUNK * 2 + 1) * 2];
	static double carry;   /* the fraction of a frame left over */
	unsigned free_frames, used, want;
	free_frames = host_sound_free_frames();
	used = free_frames < out_latency ? out_latency - free_frames : 0;
	if (n < 2) {
		want = n;
	} else if (used < out_latency / 4) {
		/* Running dry (the emulation slower than real time: a Pi too
		   slow for it): stretched to up to twice its length, lower in
		   pitch, rather than gaps. */
		unsigned quarter = out_latency / 4;
		want = n + 1 + n * (quarter - used) / quarter;
		if (want > PLAY_CHUNK * 2 + 1)
			want = PLAY_CHUNK * 2 + 1;
		carry = 0.0;
	} else {
		double half = out_latency / 2.0;
		double error = (half - used) / half;   /* 1 empty, -1 full */
		double exact;
		if (error > 1.0)
			error = 1.0;
		if (error < -1.0)
			error = -1.0;
		exact = n * (1.0 + 0.02 * error) + carry;
		want = (unsigned) exact;
		carry = exact - want;
	}
	if (want != n) {
		resample(samples, n, buf, want);
		samples = buf;
		n = want;
	}
	if (n > free_frames)
		n = free_frames;          /* no room at all (far ahead): drop it */
	if (n > 0)
		write_frames(samples, n);
}

/* N stereo frames to the output. */
static void play(const int16_t *samples, unsigned n)
{
	if (out_rate == 0)
		return;
	while (n > 0) {
		unsigned m = n > PLAY_CHUNK ? PLAY_CHUNK : n;
		play_chunk(samples, m);
		samples += m * 2;
		n -= m;
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

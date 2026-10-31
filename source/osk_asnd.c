/*-------------------------------------------------------------
osk_asnd.c -- sound effect playback through ASND

Kept in its own object so that only apps calling OSK_InitSound()
need to link -lasnd.
-------------------------------------------------------------*/
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <asndlib.h>

#include "osk_internal.h"

/* ASND mixes at 48 kHz and steps through a voice without interpolating,
   which turns the 32 kHz clicks gritty. So each sound is upsampled once
   to 48 kHz with a Catmull-Rom spline and played at (nearly) one sample
   per output sample. */
#define MIX_RATE 48000

static u8 volume = 255;
static s16 *mixed[OSK_SOUND_COUNT];	/* 48 kHz, 32 byte aligned and padded */
static unsigned mixed_bytes[OSK_SOUND_COUNT];

static float at(const osk_sound_data *d, int i)
{
	if (i < 0)
		return d->pcm[0];
	return (unsigned)i < d->samples ? d->pcm[i] : 0;
}

static bool upsample(osk_sound sound)
{
	osk_sound_data d;
	unsigned n, bytes, i;
	float step;
	bool owned;
	s16 *out;

	if (!osk_sound_load(sound, &d, &owned))
		return false;
	n = (unsigned)((u64)d.samples * MIX_RATE / d.rate);
	bytes = (n * 2 + 31) & ~31u;
	step = (float)d.rate / MIX_RATE;
	out = memalign(32, bytes);
	if (out) {
		for (i = 0; i < n; i++) {
			float t = i * step, f;
			int k = (int)t;
			float p0 = at(&d, k - 1), p1 = at(&d, k), p2 = at(&d, k + 1), p3 = at(&d, k + 2), v;

			f = t - k;
			v = p1 + 0.5f * f * (p2 - p0 + f * (2 * p0 - 5 * p1 + 4 * p2 - p3 +
							f * (3 * (p1 - p2) + p3 - p0)));
			out[i] = v < -32768 ? -32768 : v > 32767 ? 32767 : (s16)v;
		}
		memset(out + n, 0, bytes - n * 2);
		DCFlushRange(out, bytes);
		mixed[sound] = out;
		mixed_bytes[sound] = bytes;
	}
	if (owned)
		free((void *)d.pcm);
	return out != NULL;
}

static void play(osk_sound sound)
{
	unsigned jitter;
	s32 voice;
	int pitch = MIX_RATE;

	if ((unsigned)sound >= OSK_SOUND_COUNT)
		return;
	if (!mixed[sound] && !upsample(sound))
		return;
	voice = ASND_GetFirstUnusedVoice();
	if (voice < 0)
		return;
	jitter = osk_sound_clips[sound].jitter_cents;
	if (jitter) {
		/* 2^(cents / 1200) ~ 1 + cents * ln 2 / 1200 for small values */
		float r = (rand() % 2001 - 1000) / 1000.0f;
		pitch = (int)(MIX_RATE * (1 + r * jitter * 0.000577623f));
	}
	ASND_SetVoice(voice, VOICE_MONO_16BIT_BE, pitch, 0, mixed[sound],
		      mixed_bytes[sound], volume, volume, NULL);
}

bool OSK_InitSound(bool init_asnd)
{
	if (init_asnd) {
		ASND_Init();
		ASND_Pause(0);
	}
	osk_sound_hook = play;
	return true;
}

void OSK_ShutdownSound(void)
{
	osk_sound_hook = NULL;
}

void OSK_SetSoundVolume(u8 v)
{
	volume = v;
}

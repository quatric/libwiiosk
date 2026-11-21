/*-------------------------------------------------------------
osk_asnd.c -- sound effect playback through ASND

Kept in its own object so that only apps calling OSK_InitSound()
need to link -lasnd.
-------------------------------------------------------------*/
#include <stdlib.h>
#include <asndlib.h>

#include "osk_internal.h"

/* The sounds are rendered at 48 kHz, ASND's mix rate, 32 byte aligned and padded with silence,
   so they play as they are at (nearly) one sample per output sample. */
#define MIX_RATE 48000

static u8 volume = 255;
static u32 flushed;			/* sounds whose samples are out of the data cache */

static void play(osk_sound sound)
{
	osk_sound_data d;
	unsigned jitter, bytes;
	s32 voice;
	int pitch = MIX_RATE;

	if ((unsigned)sound >= OSK_SOUND_COUNT || !osk_sound_load(sound, &d))
		return;
	bytes = (d.samples * 2 + 31) & ~31u;
	if (!(flushed >> sound & 1)) {
		DCFlushRange((void *)d.pcm, bytes);
		flushed |= 1u << sound;
	}
	voice = ASND_GetFirstUnusedVoice();
	if (voice < 0)
		return;
	jitter = d.jitter_cents;
	if (jitter) {
		/* 2^(cents / 1200) ~ 1 + cents * ln 2 / 1200 for small values */
		float r = (rand() % 2001 - 1000) / 1000.0f;
		pitch = (int)(MIX_RATE * (1 + r * jitter * 0.000577623f));
	}
	ASND_SetVoice(voice, VOICE_MONO_16BIT_BE, pitch, 0, (void *)d.pcm, bytes, volume, volume, NULL);
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

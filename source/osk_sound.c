/*-------------------------------------------------------------
osk_sound.c -- the built-in sound effects (16-bit PCM)

Kept in its own object (with osk_sound_data.c) so that apps without
sound do not link the audio data.
-------------------------------------------------------------*/
#include <stdlib.h>
#ifdef GEKKO
#include <malloc.h>
#endif

#include "osk_internal.h"

static s16 *decoded[OSK_SOUND_COUNT];

/* buffers are 32 byte aligned and padded with silence, as ASND needs */
static unsigned osk_sound_bytes(const osk_sound_clip *c)
{
	return (c->samples * 2 + 31) & ~31u;
}

static s16 *decode(const osk_sound_clip *c)
{
	const u8 *in = &osk_sound_pcm[c->offset];
	unsigned size = osk_sound_bytes(c), i;
	s16 *out;

#ifdef GEKKO
	out = memalign(32, size);
#else
	out = malloc(size);
#endif
	if (!out)
		return NULL;
	for (i = 0; i < c->samples; i++)
		out[i] = (s16)(in[i * 2] << 8 | in[i * 2 + 1]);
	for (; i < size / 2; i++)
		out[i] = 0;
	return out;
}

bool osk_sound_load(osk_sound sound, osk_sound_data *out, bool *owned)
{
	const osk_sound_clip *c;
	s16 *pcm;

	if ((unsigned)sound >= OSK_SOUND_COUNT || !out)
		return false;
	c = &osk_sound_clips[sound];
	*owned = !decoded[sound];
	if (decoded[sound]) {
		pcm = decoded[sound];
	} else {
		pcm = decode(c);
		if (!pcm)
			return false;
	}
	out->pcm = pcm;
	out->samples = c->samples;
	out->rate = c->rate;
	out->jitter_cents = c->jitter_cents;
	return true;
}

bool OSK_GetSound(osk_sound sound, osk_sound_data *out)
{
	bool owned;

	if (!osk_sound_load(sound, out, &owned))
		return false;
	decoded[sound] = (s16 *)out->pcm;
	return true;
}

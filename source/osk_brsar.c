/*-------------------------------------------------------------
osk_brsar.c -- the keyboard's sound effects, played off its own
sound archive (wiiSoftwareKeyboardSound.brsar, packed into
osk_sound_data.c by tools/mkbrsar.py)

On first use the archive is unpacked and every sound is rendered
once: sequence sounds go through the NW4R sequencer and synthesizer
of osk_seqsynth.c (their RSEQ, RBNK and waves), wave sounds are
decoded from their RWSD. The sounds are mono at 48 kHz, scaled
together so that the relative loudness stays, and trimmed. The
archive is freed again afterwards.

Its own object: apps that use no sound do not link the synthesizer
or the archive.
-------------------------------------------------------------*/
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "osk_internal.h"
#include "osk_seqsynth.h"

#ifdef GEKKO
#include <malloc.h>
#define ALLOC32(n)	memalign(32, (n))
#else
#define ALLOC32(n)	malloc(n)
#endif

#define RATE		48000
#define MAX_SECONDS	4

/* archive sound name and random pitch bend in cents, by osk_sound */
static const struct {
	const char *name;
	u8 jitter;
} sounds[OSK_SOUND_COUNT] = {
	[OSK_SOUND_FOCUS]	= { "WIPL_SE_CHAR_FOCUS", 0 },
	[OSK_SOUND_INPUT]	= { "WIPL_SE_CHAR_INPUT", 50 },
	[OSK_SOUND_DELETE]	= { "WIPL_SE_CHAR_DELETE", 25 },
	[OSK_SOUND_ERROR]	= { "WIPL_SE_CHAR_DELETE_ERROR", 0 },
	[OSK_SOUND_CARET]	= { "WIPL_SE_CHAR_CURSOR", 0 },
	[OSK_SOUND_CARET_FIX]	= { "WIPL_SE_CHAR_CURSOR_FIX", 0 },
	[OSK_SOUND_CANDIDATE]	= { "WIPL_SE_CHAR_DECIDE", 38 },
	[OSK_SOUND_SCROLL]	= { "WIPL_SE_LINE_SCROLL", 0 },
	[OSK_SOUND_SHIFT_ON]	= { "WIPL_SE_CHAR_LARGER", 0 },
	[OSK_SOUND_SHIFT_OFF]	= { "WIPL_SE_CHAR_LOWER", 0 },
	[OSK_SOUND_MORE]	= { "WIPL_SE_SYMBOL_PAGE_OPEN", 0 },
	[OSK_SOUND_BACK]	= { "WIPL_SE_SK_PAGE_CHG", 0 },
	[OSK_SOUND_PREDICT_ON]	= { "WIPL_SE_SK_PREDICT_ON", 0 },
	[OSK_SOUND_PREDICT_OFF]	= { "WIPL_SE_SK_PREDICT_OFF", 0 },
	[OSK_SOUND_LETTERS]	= { "WIPL_SE_SK_SWITCHING_01", 0 },
	[OSK_SOUND_NUMPAD]	= { "WIPL_SE_SK_SWITCH_TO_KETAI", 0 },
	[OSK_SOUND_OPEN]	= { "WIPL_SE_SK_OPEN", 0 },
	[OSK_SOUND_OK]		= { "WIPL_SE_SK_DECIDE_CLOSE", 0 },
	[OSK_SOUND_CANCEL]	= { "WIPL_SE_SK_CANCEL_CLOSE", 0 },
};

static const u8 *arc;
static size_t arc_size;
static s16 *rendered[OSK_SOUND_COUNT];
static unsigned rendered_n[OSK_SOUND_COUNT];

static u32 be32(const u8 *p)
{
	return (u32)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

static u16 be16(const u8 *p)
{
	return (u16)(p[0] << 8 | p[1]);
}

/*-------------------------------------------------------------
  BRSAR tables (see osk_seqsynth.c for the sequence side)
-------------------------------------------------------------*/
typedef struct {
	const u8 *b;
	u32 n;
} blk;

static u32 bu32(const blk *k, uint64_t off)
{
	return off + 4 <= k->n ? be32(k->b + off) : 0xFFFFFFFFu;
}

static const char *sym_name(const blk *sy, u32 id)
{
	u32 tab = bu32(sy, 0), cnt = bu32(sy, tab), o;

	if (id == 0xFFFFFFFFu || tab == 0xFFFFFFFFu || id >= cnt)
		return NULL;
	o = bu32(sy, tab + 4 + (uint64_t)id * 4);
	if (o >= sy->n || !memchr(sy->b + o, 0, sy->n - o))
		return NULL;
	return (const char *)sy->b + o;
}

static bool mem_read(void *ctx, uint64_t off, void *buf, uint32_t len)
{
	if (off + len > arc_size)
		return false;
	memcpy(buf, arc + off, len);
	return true;
}

/* DSP-ADPCM, the NW4R way: `nibbles` includes the frame headers */
static bool adpcm_decode(const u8 *data, const u8 *param, u32 nibbles, s16 *out, u32 *count)
{
	s16 coef[16];
	int h1, h2, i, f;
	u32 frames = (nibbles + 15) / 16, n = 0;

	for (i = 0; i < 16; i++)
		coef[i] = (s16)be16(param + i * 2);
	h1 = (s16)be16(param + 36);
	h2 = (s16)be16(param + 38);
	for (f = 0; f < (int)frames; f++) {
		const u8 *p = data + (uint64_t)f * 8;
		int scale = 1 << (p[0] & 15), c1 = coef[(p[0] >> 4) * 2], c2 = coef[(p[0] >> 4) * 2 + 1];

		for (i = 0; i < 14; i++) {
			int nb = i % 2 == 0 ? p[1 + i / 2] >> 4 : p[1 + i / 2] & 15, s;

			if (nb >= 8)
				nb -= 16;
			s = (((nb * scale) << 11) + c1 * h1 + c2 * h2 + 1024) >> 11;
			s = s < -32768 ? -32768 : s > 32767 ? 32767 : s;
			h2 = h1;
			h1 = s;
			out[n++] = s;
		}
	}
	*count = nibbles - 2 * frames;
	if (*count > n)
		*count = n;
	return true;
}

/* A wave sound: one wave of an RWSD, played at its pitch and note volume. Returns mono s16 at
   RATE (malloc'ed). */
static s16 *wave_sound(const blk *in, u32 info_off, u32 file_id, u32 sound_volume, unsigned *out_n)
{
	u32 ftab = bu32(in, 0x1C), fcnt = bu32(in, ftab), gtab = bu32(in, 0x24), gcnt = bu32(in, gtab);
	u32 fe, pt, pe, gid, idx, ge, it, ic, ie, widx, dsec = 0, wsec = 0, k;
	uint64_t rwsd, wblk;
	const u8 *r;
	u32 ws, notes, note, wnum, wi, nch, fmt, end, tab, loc, rate, dn, i;
	float pitch, vol;
	s16 *mono = NULL, *chan[2] = { NULL, NULL };
	u32 cn[2] = { 0, 0 };
	float step, gain;
	unsigned n, o;
	s16 *out;

	if (file_id >= fcnt)
		return NULL;
	widx = bu32(in, info_off);
	fe = bu32(in, ftab + 8 + (uint64_t)file_id * 8);
	pt = bu32(in, fe + 0x18);
	if (!bu32(in, pt) || bu32(in, pt) == 0xFFFFFFFFu)
		return NULL;
	pe = bu32(in, pt + 8);
	gid = bu32(in, pe);
	idx = bu32(in, pe + 4);
	if (gid >= gcnt)
		return NULL;
	ge = bu32(in, gtab + 8 + (uint64_t)gid * 8);
	it = bu32(in, ge + 0x24);
	ic = bu32(in, it);
	if (idx >= ic || ic > in->n / 8)
		return NULL;
	ie = bu32(in, it + 8 + (uint64_t)idx * 8);
	rwsd = (uint64_t)bu32(in, ge + 0x10) + bu32(in, ie + 4);
	wblk = (uint64_t)bu32(in, ge + 0x18) + bu32(in, ie + 0xC);
	if (rwsd + 0x20 > arc_size || wblk >= arc_size || memcmp(arc + rwsd, "RWSD", 4))
		return NULL;
	r = arc + rwsd;
	for (k = 0; k < be16(r + 0xE) && k < 2; k++) {
		u32 so = be32(r + 0x10 + k * 8);

		if (rwsd + so + 8 > arc_size)
			return NULL;
		if (!memcmp(r + so, "DATA", 4))
			dsec = so;
		else if (!memcmp(r + so, "WAVE", 4))
			wsec = so;
	}
	if (!dsec || !wsec || rwsd + dsec + 0xC > arc_size || rwsd + wsec + 0xC > arc_size)
		return NULL;
	if (widx >= be32(r + dsec + 8) || rwsd + dsec + 0xC + (uint64_t)widx * 8 + 8 > arc_size)
		return NULL;
	ws = be32(r + dsec + 0xC + widx * 8 + 4);
	if (rwsd + dsec + 8 + ws + 0x18 > arc_size)
		return NULL;
	{
		u32 inf = be32(r + dsec + 8 + ws + 4), cur;
		union { u32 u; float f; } pu;

		if (rwsd + dsec + 8 + inf + 4 > arc_size)
			return NULL;
		pu.u = be32(r + dsec + 8 + inf);
		pitch = pu.f;
		notes = be32(r + dsec + 8 + ws + 0x14);
		if (rwsd + dsec + 8 + notes + 0xC > arc_size)
			return NULL;
		note = be32(r + dsec + 8 + notes + 8);
		cur = dsec + 8 + note;
		if (rwsd + cur + 14 > arc_size)
			return NULL;
		wnum = be32(r + cur);
		vol = r[cur + 13] / 127.0f;
	}
	if (wnum >= be32(r + wsec + 8) || rwsd + wsec + 0xC + (uint64_t)wnum * 4 + 4 > arc_size)
		return NULL;
	wi = wsec + be32(r + wsec + 0xC + wnum * 4);
	if (rwsd + wi + 0x18 > arc_size)
		return NULL;
	fmt = r[wi];
	nch = r[wi + 2];
	rate = r[wi + 3] << 16 | be16(r + wi + 4);
	end = be32(r + wi + 12);
	tab = be32(r + wi + 16);
	loc = be32(r + wi + 20);
	if (fmt > 2 || !nch || nch > 2 || !end || end > (4u << 20) || !rate)
		return NULL;

	for (i = 0; i < nch; i++) {
		u32 ci, doff;
		uint64_t data;

		if (rwsd + wi + tab + 4 * i + 4 > arc_size)
			goto fail;
		ci = wi + be32(r + wi + tab + 4 * i);
		if (rwsd + ci + 8 > arc_size)
			goto fail;
		doff = be32(r + ci);
		data = wblk + loc + doff;
		chan[i] = malloc((fmt == 2 ? (end + 15) / 16 * 14 : end) * sizeof(s16) + 2);
		if (!chan[i])
			goto fail;
		if (fmt == 2) {
			u32 ai = wi + be32(r + ci + 4);

			if (rwsd + ai + 0x28 > arc_size || data + ((uint64_t)end + 15) / 16 * 8 > arc_size)
				goto fail;
			adpcm_decode(arc + data, r + ai, end, chan[i], &cn[i]);
		} else if (fmt == 1) {
			if (data + (uint64_t)end * 2 > arc_size)
				goto fail;
			for (dn = 0; dn < end; dn++)
				chan[i][dn] = (s16)be16(arc + data + dn * 2);
			cn[i] = end;
		} else {
			if (data + end > arc_size)
				goto fail;
			for (dn = 0; dn < end; dn++)
				chan[i][dn] = (s16)((s8)arc[data + dn] * 256);
			cn[i] = end;
		}
	}
	dn = cn[0];
	if (nch == 2 && cn[1] < dn)
		dn = cn[1];
	mono = malloc((dn ? dn : 1) * sizeof(s16));
	if (!mono)
		goto fail;
	for (i = 0; i < dn; i++)
		mono[i] = nch == 2 ? (s16)(((int)chan[0][i] + chan[1][i]) / 2) : chan[0][i];
	free(chan[0]);
	free(chan[1]);
	chan[0] = chan[1] = NULL;

	/* to RATE, at the wave sound's pitch, with its note and sound volume */
	if (!(pitch > 0.01f && pitch < 100))
		pitch = 1;
	step = (float)rate * pitch / RATE;
	n = (unsigned)(dn / step);
	gain = vol * sound_volume / 127.0f;
	out = malloc((n ? n : 1) * sizeof(s16));
	if (!out) {
		free(mono);
		return NULL;
	}
	for (o = 0; o < n; o++) {
		float t = o * step, v;
		u32 a = (u32)t, b = a + 1 < dn ? a + 1 : a;

		v = mono[a] + (mono[b] - mono[a]) * (t - a);
		v *= gain;
		out[o] = v < -32768 ? -32768 : v > 32767 ? 32767 : (s16)v;
	}
	free(mono);
	*out_n = n;
	return out;
fail:
	free(chan[0]);
	free(chan[1]);
	free(mono);
	return NULL;
}

/*-------------------------------------------------------------
  render one sound
-------------------------------------------------------------*/
static s16 *render_sequence(const char *name, u32 sound_volume, unsigned *out_n)
{
	const char *err = NULL;
	oskseq_t *sq = oskseq_open(mem_read, NULL, arc_size, name, &err);
	s16 *stereo, *mono;
	unsigned frames = 0, cap = RATE * MAX_SECONDS, i;

	if (!sq)
		return NULL;
	stereo = malloc((size_t)cap * 4);
	mono = malloc((size_t)cap * sizeof(s16));
	if (!stereo || !mono) {
		free(stereo);
		free(mono);
		oskseq_close(sq);
		return NULL;
	}
	while (frames < cap) {
		u32 want = cap - frames > 2048 ? 2048 : cap - frames, got;

		got = oskseq_render(sq, stereo + (size_t)frames * 2, want);
		frames += got;
		if (got < want)
			break;
	}
	oskseq_close(sq);
	for (i = 0; i < frames; i++) {
		int v = ((int)stereo[i * 2] + stereo[i * 2 + 1]) / 2;

		v = v * (int)sound_volume / 127;
		mono[i] = (s16)v;
	}
	free(stereo);
	*out_n = frames;
	return mono;
}

/* trim silence at both ends and fade the end, like tools/mksound.py */
static unsigned trim(s16 *pcm, unsigned n, float scale, unsigned *start)
{
	float floor_ = 32767.0f / 1500;
	unsigned end = n, s = 0;

	while (end > 0 && fabsf(pcm[end - 1] * scale) < floor_)
		end--;
	while (s < end && fabsf(pcm[s] * scale) < floor_ / 4)
		s++;
	*start = s;
	return end - s;
}

static bool render_all(const u8 *b, size_t size)
{
	blk sy, in;
	u32 so, ss, io, is, stab, scnt, i;
	s16 *raw[OSK_SOUND_COUNT] = { NULL };
	unsigned raw_n[OSK_SOUND_COUNT] = { 0 };
	float peak = 1, scale;
	int s, ok = 0;

	if (size < 0x40 || memcmp(b, "RSAR", 4))
		return false;
	so = be32(b + 0x10);
	ss = be32(b + 0x14);
	io = be32(b + 0x18);
	is = be32(b + 0x1C);
	if (ss < 8 || is < 8 || (uint64_t)so + ss > size || (uint64_t)io + is > size ||
	    memcmp(b + so, "SYMB", 4) || memcmp(b + io, "INFO", 4))
		return false;
	arc = b;
	arc_size = size;
	sy = (blk){ b + so + 8, ss - 8 };
	in = (blk){ b + io + 8, is - 8 };
	stab = bu32(&in, 0x04);
	scnt = bu32(&in, stab);
	if (scnt > in.n / 8)
		scnt = 0;

	for (i = 0; i < scnt; i++) {
		u32 e = bu32(&in, stab + 8 + (uint64_t)i * 8), info, fid, vol;
		const char *name;

		if (e + 0x20ull > in.n)
			continue;
		name = sym_name(&sy, bu32(&in, e));
		if (!name)
			continue;
		for (s = 0; s < OSK_SOUND_COUNT; s++)
			if (!strcmp(name, sounds[s].name))
				break;
		if (s == OSK_SOUND_COUNT || raw[s])
			continue;
		fid = bu32(&in, e + 4);
		vol = in.b[e + 0x14];
		info = bu32(&in, e + 0x1C);
		if (in.b[e + 0x16] == 1)
			raw[s] = render_sequence(name, vol, &raw_n[s]);
		else if (in.b[e + 0x16] == 3)
			raw[s] = wave_sound(&in, info, fid, vol, &raw_n[s]);
	}
	for (s = 0; s < OSK_SOUND_COUNT; s++)
		for (i = 0; i < raw_n[s]; i++)
			if (fabsf((float)raw[s][i]) > peak)
				peak = fabsf((float)raw[s][i]);
	scale = 0.95f * 32767 / peak;
	for (s = 0; s < OSK_SOUND_COUNT; s++) {
		unsigned start, n, k, fade, padded;

		if (!raw[s] || !raw_n[s])
			continue;
		n = trim(raw[s], raw_n[s], scale, &start);
		if (!n)
			continue;
		padded = (n * 2 + 31) & ~31u;	/* 32 byte aligned and padded with silence, as ASND needs */
		rendered[s] = ALLOC32(padded);
		if (!rendered[s])
			continue;
		for (k = 0; k < n; k++) {
			float v = raw[s][start + k] * scale;

			rendered[s][k] = (s16)(v < -32768 ? -32768 : v > 32767 ? 32767 : v);
		}
		fade = n < RATE / 500 ? n : RATE / 500;
		for (k = 0; k < fade; k++)
			rendered[s][n - fade + k] = (s16)(rendered[s][n - fade + k] * (1.0f - (k + 1.0f) / (fade + 1)));
		for (k = n; k < padded / 2; k++)
			rendered[s][k] = 0;
		rendered_n[s] = n;
		ok++;
	}
	for (s = 0; s < OSK_SOUND_COUNT; s++)
		free(raw[s]);
	arc = NULL;
	arc_size = 0;
	return ok > 0;
}

bool osk_sound_load(osk_sound sound, osk_sound_data *out)
{
	static bool tried;

	if ((unsigned)sound >= OSK_SOUND_COUNT || !out)
		return false;
	if (!tried) {
		u32 size;
		u8 *b = osk_unpack(osk_sound_packed, osk_sound_packed_size, &size);

		tried = true;
		if (b) {
			render_all(b, size);
			free(b);
		}
	}
	if (!rendered[sound])
		return false;
	out->pcm = rendered[sound];
	out->samples = rendered_n[sound];
	out->rate = RATE;
	out->jitter_cents = sounds[sound].jitter;
	return true;
}

bool OSK_GetSound(osk_sound sound, osk_sound_data *out)
{
	return osk_sound_load(sound, out);
}

#!/usr/bin/env python3
"""
mksound.py -- render the Wii keyboard sound effects into wiiosk

    python3 tools/mksound.py wiiSoftwareKeyboardSound.brsar > source/osk_sound_data.c
    python3 tools/mksound.py wiiSoftwareKeyboardSound.brsar --wav out_dir

Every sound effect of the sound archive is rendered offline: sequence
sounds run through a small SEQ interpreter that drives a sampler over
the bank's instruments, wave sounds just play their wave. The result
is mono, trimmed, normalised per archive (not per sound, so the relative
loudness stays), stored at the lowest of 32000, 16000 and 10667 Hz that
keeps 98% of its energy, as 4-bit IMA ADPCM. Random pitch bends in a
sequence become a per-sound pitch jitter applied at playback.

IMA ADPCM, as stored: per sound one s16BE start sample and u8 start index,
then two samples per byte, low nibble first; the standard IMA step and
index tables decode it (see osk_sound.c).
"""

import math
import struct
import sys
from pathlib import Path

RATE = 32000

# archive sound name -> wiiosk sound id (enum osk_sound in wiiosk.h)
SOUNDS = [
	("WIPL_SE_CHAR_FOCUS", "FOCUS"),
	("WIPL_SE_CHAR_INPUT", "INPUT"),
	("WIPL_SE_CHAR_DELETE", "DELETE"),
	("WIPL_SE_CHAR_DELETE_ERROR", "ERROR"),
	("WIPL_SE_CHAR_CURSOR", "CARET"),
	("WIPL_SE_CHAR_CURSOR_FIX", "CARET_FIX"),
	("WIPL_SE_CHAR_DECIDE", "CANDIDATE"),
	("WIPL_SE_LINE_SCROLL", "SCROLL"),
	("WIPL_SE_CHAR_LARGER", "SHIFT_ON"),
	("WIPL_SE_CHAR_LOWER", "SHIFT_OFF"),
	("WIPL_SE_SYMBOL_PAGE_OPEN", "MORE"),
	("WIPL_SE_SK_PAGE_CHG", "BACK"),
	("WIPL_SE_SK_PREDICT_ON", "PREDICT_ON"),
	("WIPL_SE_SK_PREDICT_OFF", "PREDICT_OFF"),
	("WIPL_SE_SK_SWITCHING_01", "LETTERS"),
	("WIPL_SE_SK_SWITCH_TO_KETAI", "NUMPAD"),
	("WIPL_SE_SK_OPEN", "OPEN"),
	("WIPL_SE_SK_DECIDE_CLOSE", "OK"),
	("WIPL_SE_SK_CANCEL_CLOSE", "CANCEL"),
]


class Archive:
	def __init__(self, data):
		self.d = data
		assert data[:4] == b"RSAR"
		self.symb = self.u32(0x10)
		self.info = self.u32(0x18)
		self.names = self._names()
		base = self.info + 8
		self.base = base
		self.sound_table = base + self.ref(base)[2]
		self.group_table = base + self.ref(base + 4 * 8)[2]
		self.files = self._group_files()

	def u8(self, o):
		return self.d[o]

	def u16(self, o):
		return struct.unpack_from(">H", self.d, o)[0]

	def s16(self, o):
		return struct.unpack_from(">h", self.d, o)[0]

	def u32(self, o):
		return struct.unpack_from(">I", self.d, o)[0]

	def ref(self, o):
		return self.d[o], self.d[o + 1], self.u32(o + 4)

	def _names(self):
		sb = self.symb + 8
		nt = sb + self.u32(sb)
		return [self.d[sb + self.u32(nt + 4 + i * 4):].split(b"\0")[0].decode()
			for i in range(self.u32(nt))]

	def _group_files(self):
		"""file id -> (file offset, wave data offset) from group 0"""
		files = {}
		for g in range(self.u32(self.group_table)):
			go = self.base + self.ref(self.group_table + 4 + g * 8)[2]
			off, woff = self.u32(go + 16), self.u32(go + 24)
			items = self.base + self.ref(go + 32)[2]
			for i in range(self.u32(items)):
				io = self.base + self.ref(items + 4 + i * 8)[2]
				fid, o, _, wo, _ = [self.u32(io + k * 4) for k in range(5)]
				files.setdefault(fid, (off + o, woff + wo))
		return files

	def sounds(self):
		for i in range(self.u32(self.sound_table)):
			so = self.base + self.ref(self.sound_table + 4 + i * 8)[2]
			name = self.names[self.u32(so)]
			info = self.base + self.ref(so + 24)[2]
			yield name, {
				"file": self.u32(so + 4),
				"volume": self.u8(so + 20),
				"type": self.u8(so + 22),
				"offset": self.u32(info),		# SEQ: start, WAVE: index
				"bank": self.u32(info + 4),
			}

	def section(self, file_off, magic):
		for k in range(self.u16(file_off + 0xE)):
			so = self.u32(file_off + 0x10 + k * 8)
			if self.d[file_off + so:file_off + so + 4] == magic:
				return file_off + so
		raise KeyError(magic)

	# --- waves -----------------------------------------------------------
	def wave(self, wi, wave_block):
		"""decode the wave whose info starts at wi; mono float samples"""
		fmt, ch = self.u8(wi), self.u8(wi + 2)
		rate = self.u8(wi + 3) << 16 | self.u16(wi + 4)
		loop_end = self.u32(wi + 12)
		table = wi + self.u32(wi + 16)
		loc = self.u32(wi + 20)
		chans = []
		for c in range(ch):
			ci = wi + self.u32(table + c * 4)
			data = wave_block + loc + self.u32(ci)
			if fmt == 2:
				chans.append(self.dsp_adpcm(data, wi + self.u32(ci + 4), loop_end))
			elif fmt == 1:
				chans.append([self.s16(data + i * 2) / 32768 for i in range(loop_end)])
			else:
				chans.append([struct.unpack_from("b", self.d, data + i)[0] / 128 for i in range(loop_end)])
		n = min(len(c) for c in chans)
		return rate, [sum(c[i] for c in chans) / ch for i in range(n)]

	def dsp_adpcm(self, data, info, nibbles):
		coef = [self.s16(info + i * 2) for i in range(16)]
		h1, h2 = self.s16(info + 36), self.s16(info + 38)
		out = []
		frames = (nibbles + 15) // 16
		count = nibbles - 2 * frames	# header nibbles hold no samples
		for f in range(frames):
			ps = self.u8(data + f * 8)
			scale, c1, c2 = 1 << (ps & 15), coef[(ps >> 4) * 2], coef[(ps >> 4) * 2 + 1]
			for i in range(14):
				b = self.u8(data + f * 8 + 1 + i // 2)
				n = (b >> 4) if i % 2 == 0 else b & 15
				if n >= 8:
					n -= 16
				s = ((n * scale) << 11) + c1 * h1 + c2 * h2 + 1024 >> 11
				s = max(-32768, min(32767, s))
				h2, h1 = h1, s
				out.append(s / 32768)
		return out[:max(count, 0) or len(out)]

	def waves_of(self, file_off, wave_block, refs):
		sec = self.section(file_off, b"WAVE")
		n = self.u32(sec + 8)
		out = []
		for i in range(n):
			if refs:
				wi = sec + 8 + self.u32(sec + 12 + i * 8 + 4)
			else:
				wi = sec + self.u32(sec + 12 + i * 4)
			out.append(self.wave(wi, wave_block))
		return out

	# --- bank ------------------------------------------------------------
	def bank(self, file_id):
		off, wave_block = self.files[file_id]
		data = self.section(off, b"DATA")
		waves = self.waves_of(off, wave_block, True)
		inst = []
		for i in range(self.u32(data + 8)):
			t, dt, o = self.ref(data + 12 + i * 8)
			if dt != 1:
				inst.append(None)
				continue
			p = data + 8 + o
			inst.append({
				"wave": waves[self.u32(p)],
				"adsr": list(self.d[p + 4:p + 8]),
				"key": self.u8(p + 12),
				"volume": self.u8(p + 13),
				"tune": struct.unpack_from(">f", self.d, p + 16)[0],
			})
		return inst

	# --- wave sound ------------------------------------------------------
	def wave_sound(self, file_id, index):
		off, wave_block = self.files[file_id]
		data = self.section(off, b"DATA")
		ws = data + 8 + self.ref(data + 12 + index * 8)[2]
		info = data + 8 + self.ref(ws)[2]
		pitch = struct.unpack_from(">f", self.d, info)[0]
		notes = data + 8 + self.ref(ws + 16)[2]
		note = data + 8 + self.ref(notes + 4)[2]
		wave_index = self.u32(note)
		key, vol = self.u8(note + 12), self.u8(note + 13)
		waves = self.waves_of(off, wave_block, False)
		return waves[wave_index], pitch, key, vol


# --- envelope (NW4R style: a level in dB, updated every millisecond) --------
VOLUME_INIT = -90.4


def attack_rate(a):
	table = [0, 1, 5, 14, 26, 38, 51, 63, 73, 84, 92, 100, 109, 116, 123, 127, 132, 137, 143]
	if a >= 109:
		v = table[127 - a]
	else:
		v = 255 - a
	return (v / 256.0) ** 0.2


def decay_rate(r):
	if r == 127:
		return 65535.0
	if r == 126:
		return 120.0 / 5
	if r < 50:
		return (r * 2 + 1) / 128.0 / 5
	return 60.0 / (126 - r) / 5


def sustain_db(s):
	if s >= 127:
		return 0.0
	return 40 * math.log10(s / 127.0) if s else VOLUME_INIT


def envelope(adsr, key_off_ms, total_ms):
	"""linear gain per millisecond"""
	a, d, s, r = adsr
	level = VOLUME_INIT if a < 127 else 0.0
	state = "attack" if a < 127 else "decay"
	sus = sustain_db(s)
	out = []
	for ms in range(total_ms):
		if key_off_ms is not None and ms >= key_off_ms and state != "release":
			state = "release"
		if state == "attack":
			level *= attack_rate(a)
			if level > -0.01:
				level, state = 0.0, "decay"
		elif state == "decay":
			level -= decay_rate(d)
			if level <= sus:
				level, state = sus, "sustain"
		elif state == "release":
			level -= decay_rate(r)
		if level <= VOLUME_INIT:
			out.append(0.0)
			break
		out.append(10 ** (level / 20))
	return out


def play(wave, ratio, adsr, gain, key_off_ms):
	"""resample one note; stops at the end of the wave or the envelope"""
	rate, samples = wave
	step = ratio * rate / RATE
	total_ms = int(len(samples) / step / RATE * 1000) + 2
	env = envelope(adsr, key_off_ms, total_ms)
	out = []
	pos = 0.0
	while True:
		i = int(pos)
		if i + 1 >= len(samples):
			break
		ms = len(out) * 1000 // RATE
		if ms >= len(env):
			break
		f = pos - i
		out.append((samples[i] * (1 - f) + samples[i + 1] * f) * gain * env[ms])
		pos += step
	return out


# --- sequence interpreter ------------------------------------------------------
class Track:
	def __init__(self, pc):
		self.pc = pc
		self.wait = 0
		self.note_wait = True
		self.prg = 0
		self.volume = 127
		self.transpose = 0
		self.bend = 0
		self.bend_range = 2
		self.adsr = [None] * 4
		self.done = False
		self.stack = []


def varlen(d, pc):
	v = 0
	while True:
		b = d[pc]
		pc += 1
		v = v << 7 | (b & 0x7F)
		if not b & 0x80:
			return v, pc


def run_sequence(seq, start):
	"""returns a list of note events in ticks; random arguments take the
	middle of their range"""
	tracks = {0: Track(start)}
	notes = []
	jitter = [0.0]
	tempo = 120
	tick = 0
	while any(not t.done for t in tracks.values()) and tick < 10000:
		for tid in sorted(tracks):
			t = tracks[tid]
			while not t.done and t.wait == 0:
				cmd = seq[t.pc]
				t.pc += 1
				rnd = None
				if cmd == 0xA0:			# random: the last argument is a range
					cmd = seq[t.pc]
					t.pc += 1
					rnd = True
				if cmd < 0x80:
					vel = seq[t.pc]
					length, t.pc = varlen(seq, t.pc + 1)
					notes.append((tick, cmd, vel, length, t.prg, t.volume, t.transpose,
						      t.bend * t.bend_range / 128.0, list(t.adsr)))
					if t.note_wait:
						t.wait = length
					continue

				def arg8():
					if rnd:
						lo, hi = struct.unpack_from(">hh", seq, t.pc)
						t.pc += 4
						if cmd == 0xC4:		# random pitch bend: +-semitones
							jitter[0] = max(jitter[0], (hi - lo) / 2 * t.bend_range / 128.0)
						return (lo + hi) // 2
					v = seq[t.pc]
					t.pc += 1
					return v

				if cmd == 0x80:
					t.wait, t.pc = varlen(seq, t.pc)
				elif cmd == 0x81:
					t.prg, t.pc = varlen(seq, t.pc)
				elif cmd == 0x88:
					n = seq[t.pc]
					o = seq[t.pc + 1] << 16 | seq[t.pc + 2] << 8 | seq[t.pc + 3]
					t.pc += 4
					tracks[n] = Track(o)
				elif cmd == 0x89:
					t.pc = seq[t.pc] << 16 | seq[t.pc + 1] << 8 | seq[t.pc + 2]
				elif cmd == 0x8A:
					t.stack.append(t.pc + 3)
					t.pc = seq[t.pc] << 16 | seq[t.pc + 1] << 8 | seq[t.pc + 2]
				elif cmd == 0xFD:
					t.pc = t.stack.pop()
				elif cmd == 0xC1:
					t.volume = arg8()
				elif cmd == 0xC3:
					v = arg8()
					t.transpose = v - 256 if v > 127 else v
				elif cmd == 0xC4:
					v = arg8()
					t.bend = v - 256 if v > 127 else v
				elif cmd == 0xC5:
					t.bend_range = arg8()
				elif cmd == 0xC7:
					t.note_wait = arg8() != 0
				elif 0xD0 <= cmd <= 0xD3:
					t.adsr[cmd - 0xD0] = arg8()
				elif cmd == 0xE1:
					tempo = struct.unpack_from(">H", seq, t.pc)[0]
					t.pc += 2
				elif cmd == 0xFE:
					t.pc += 2
				elif cmd == 0xFF:
					t.done = True
				elif 0xB0 <= cmd <= 0xDF:
					arg8()			# pan, sends, priority, ...: no effect in mono
				elif cmd in (0xE0, 0xE3):
					t.pc += 2
				else:
					raise ValueError("SEQ command %02X at %X" % (cmd, t.pc - 1))
		for t in tracks.values():
			if t.wait > 0:
				t.wait -= 1
		tick += 1
	return notes, tempo, jitter[0]


def render_seq(arc, info, banks):
	off, _ = arc.files[info["file"]]
	data = arc.section(off, b"DATA")
	seq = arc.d[data + arc.u32(data + 8):data + arc.u32(data + 4)]
	inst = banks.setdefault(info["bank"], arc.bank_by_index(info["bank"]))
	notes, tempo, jitter = run_sequence(seq, info["offset"])
	tick_ms = 60000.0 / tempo / 48
	out = []
	for tick, key, vel, length, prg, vol, transpose, bend, adsr in notes:
		ins = inst[prg]
		if ins is None:
			continue
		env = [o if o is not None else i for o, i in zip(adsr, ins["adsr"])]
		semis = key - ins["key"] + transpose + bend
		gain = (vel / 127) ** 2 * (vol / 127) ** 2 * ins["volume"] / 127
		key_off = int(length * tick_ms) if length else None
		pcm = play(ins["wave"], ins["tune"] * 2 ** (semis / 12), env, gain, key_off)
		start = int(tick * tick_ms * RATE / 1000)
		if len(out) < start + len(pcm):
			out.extend([0.0] * (start + len(pcm) - len(out)))
		for i, v in enumerate(pcm):
			out[start + i] += v
	return [v * info["volume"] / 127 for v in out], jitter


def render_wave_sound(arc, info):
	wave, pitch, key, vol = arc.wave_sound(info["file"], info["offset"])
	pcm = play(wave, pitch, [127, 127, 127, 127], vol / 127, None)
	return [v * info["volume"] / 127 for v in pcm], 0.0


def bank_by_index(self, index):
	# the bank table lists file ids; this archive has a single bank
	bt = self.base + self.ref(self.base + 8)[2]
	bo = self.base + self.ref(bt + 4 + index * 8)[2]
	return self.bank(self.u32(bo + 4))


Archive.bank_by_index = bank_by_index


def lowpass(pcm, cutoff, taps=63):
	"""windowed sinc; cutoff as a fraction of RATE"""
	m = taps // 2
	h = []
	for i in range(taps):
		x = i - m
		v = 2 * cutoff if x == 0 else math.sin(2 * math.pi * cutoff * x) / (math.pi * x)
		w = 0.42 - 0.5 * math.cos(2 * math.pi * i / (taps - 1)) + 0.08 * math.cos(4 * math.pi * i / (taps - 1))
		h.append(v * w)
	n = len(pcm)
	padded = [0.0] * m + pcm + [0.0] * m
	return [sum(h[k] * padded[i + k] for k in range(taps)) for i in range(n)]


def downsample(pcm):
	"""(rate, samples) at the lowest rate that keeps 98% of the energy"""
	energy = sum(v * v for v in pcm) or 1.0
	for k in (3, 2):
		low = lowpass(pcm, 0.45 / k)
		lost = sum((a - b) ** 2 for a, b in zip(pcm, low)) / energy
		if lost < 0.02:
			return RATE // k, low[::k]
	return RATE, pcm


def trim(pcm, floor):
	end = len(pcm)
	while end > 0 and abs(pcm[end - 1]) < floor:
		end -= 1
	start = 0
	while start < end and abs(pcm[start]) < floor / 4:
		start += 1
	pcm = pcm[start:end]
	# 2 ms fade-out so the cut is inaudible
	fade = min(len(pcm), RATE // 500)
	for i in range(fade):
		pcm[len(pcm) - fade + i] *= 1 - (i + 1) / (fade + 1)
	return pcm


# --- IMA ADPCM -------------------------------------------------------------------
STEPS = [
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60,
	66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337,
	371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552,
	1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
	6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
	22385, 24623, 27086, 29794, 32767,
]
INDEX = [-1, -1, -1, -1, 2, 4, 6, 8]


def ima_step(pred, index, code):
	step = STEPS[index]
	diff = step >> 3
	if code & 1:
		diff += step >> 2
	if code & 2:
		diff += step >> 1
	if code & 4:
		diff += step
	pred = pred - diff if code & 8 else pred + diff
	pred = max(-32768, min(32767, pred))
	index = max(0, min(88, index + INDEX[code & 7]))
	return pred, index


def ima_encode(samples):
	"""samples: ints; returns (first sample, start index, codes)"""
	pred, index = samples[0], 0
	# pick the start index that fits the first difference
	if len(samples) > 1:
		d = abs(samples[1] - samples[0])
		while index < 88 and STEPS[index] < d:
			index += 1
	first, first_index = pred, index
	codes = []
	for s in samples[1:]:
		best = None
		for code in range(16):
			p, _ = ima_step(pred, index, code)
			e = abs(p - s)
			if best is None or e < best[0]:
				best = (e, code)
		pred, index = ima_step(pred, index, best[1])
		codes.append(best[1])
	return first, first_index, codes


def ima_decode(first, index, codes):
	out = [first]
	pred = first
	for c in codes:
		pred, index = ima_step(pred, index, c)
		out.append(pred)
	return out


def main():
	args = sys.argv[1:]
	if not args:
		sys.exit(__doc__)
	arc = Archive(Path(args[0]).read_bytes())
	infos = dict(arc.sounds())
	banks = {}
	rendered = []
	for name, ident in SOUNDS:
		info = infos[name]
		pcm, jitter = render_seq(arc, info, banks) if info["type"] == 1 else render_wave_sound(arc, info)
		rendered.append([name, ident, pcm, jitter])

	peak = max(max(abs(v) for v in r[2]) for r in rendered)
	for r in rendered:
		r[2] = [v * 0.95 / peak for v in r[2]]
		r[2] = trim(r[2], 1.0 / 1500)
		r.insert(2, 0)
		r[2], r[3] = downsample(r[3])
	scale = 1.0

	if len(args) > 2 and args[1] == "--wav":
		out = Path(args[2])
		out.mkdir(parents=True, exist_ok=True)
		for name, ident, rate, pcm, jitter in rendered:
			ints = [int(round(v * scale * 32767)) for v in pcm]
			first, index, codes = ima_encode(ints)
			ints = ima_decode(first, index, codes)
			body = struct.pack("<%dh" % len(ints), *ints)
			hdr = b"RIFF" + struct.pack("<I", 36 + len(body)) + b"WAVEfmt " + struct.pack(
				"<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16) + b"data" + struct.pack("<I", len(body))
			(out / f"{ident.lower()}.wav").write_bytes(hdr + body)
		return

	lines = [
		"/* Generated by tools/mksound.py from wiiSoftwareKeyboardSound.brsar.",
		"   Do not edit. Mono IMA ADPCM, see tools/mksound.py. */",
		'#include "osk_internal.h"',
		"",
	]
	table = []
	blob = bytearray()
	for name, ident, rate, pcm, jitter in rendered:
		ints = [int(round(v * scale * 32767)) for v in pcm]
		first, index, codes = ima_encode(ints)
		if len(codes) % 2:
			codes.append(0)
		table.append((ident, len(blob), len(ints), rate, round(jitter * 100), name))
		blob += struct.pack(">hB", first, index)
		for i in range(0, len(codes), 2):
			blob.append(codes[i] | codes[i + 1] << 4)
	lines.append("const u8 osk_sound_adpcm[%d] = {" % len(blob))
	for i in range(0, len(blob), 24):
		lines.append("\t" + ",".join(str(b) for b in blob[i:i + 24]) + ",")
	lines.append("};")
	lines.append("")
	lines.append("/* offset, samples, rate, pitch jitter in cents */")
	lines.append("const osk_sound_clip osk_sound_clips[OSK_SOUND_COUNT] = {")
	for ident, off, n, rate, cents, name in table:
		lines.append("\t[OSK_SOUND_%s] = { %d, %d, %d, %d },\t/* %s, %.0f ms */" % (
			ident, off, n, rate, cents, name, n * 1000 / rate))
	lines.append("};")
	print("\n".join(lines))
	total = sum(n * 1000 / rate for _, _, n, rate, _, _ in table)
	print("%d sounds, %.0f ms, %d bytes" % (len(table), total, len(blob)), file=sys.stderr)


if __name__ == "__main__":
	main()

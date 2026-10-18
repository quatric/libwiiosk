"""
rcpack.py -- the compression used for wiiosk's built-in data

An adaptive binary range coder (as in LZMA) with a context model per
kind of data. source/osk_unpack.c is the decoder; both must agree on
every detail below.

Stream: u32BE unpacked size, u8 model, u16BE width (MODEL_IMAGE only,
else 0), then the range coder bytes.

Range coder: 32-bit range, 12-bit probabilities that start at 2048
and move 1/16 of the way to the coded bit. A symbol is coded MSB first
through a bit tree (node = 1, then node * 2 + bit) under its context.

Models:
  MODEL_BYTES  8-bit symbols, context = the previous byte.
  MODEL_NIBBLE 4-bit symbols, low nibble of each byte first, context =
               the two previous nibbles.
  MODEL_IMAGE  4-bit pixels, high nibble first, rows of `width` pixels;
               context = left, up, up-left >> 2, up-right >> 2.
  MODEL_DICT   8-bit symbols of a .zsd (optionally followed by a .znd).
               Inside the word graph of record 0 each byte is a node head
               (context: the previous head), an extended character index
               (context: the head's flag class), the first byte of a child
               or sibling pointer (context: which, the head's flag class)
               or a further pointer byte (context: its position, the
               previous byte >> 2). Everything else uses MODEL_BYTES.
"""

import struct

MODEL_BYTES, MODEL_NIBBLE, MODEL_IMAGE, MODEL_DICT = 0, 1, 2, 3

PROB_BITS, PROB_INIT, SHIFT = 12, 2048, 4
TOP = 1 << 24

NODE_FLAGS = [0x10, 0x01, 0x03, 0x14, 0x05, 0x07, 0x1C, 0x0D,
	      0x0F, 0x11, 0x13, 0x15, 0x17, 0x1D, 0x1F, 0x35]

# MODEL_DICT context bases (each context holds a 256 entry bit tree)
D_OTHER, D_HEAD, D_EXT, D_PTR0, D_PTRN, D_COUNT = 0, 256, 512, 528, 560, 752


class Encoder:
	def __init__(self):
		self.low, self.range, self.cache, self.cache_size = 0, 0xFFFFFFFF, 0, 1
		self.out = bytearray()

	def shift_low(self):
		if self.low < 0xFF000000 or self.low >= 1 << 32:
			carry = self.low >> 32
			temp = self.cache
			while True:
				self.out.append((temp + carry) & 0xFF)
				temp = 0xFF
				self.cache_size -= 1
				if not self.cache_size:
					break
			self.cache = (self.low >> 24) & 0xFF
		self.cache_size += 1
		self.low = (self.low << 8) & 0xFFFFFFFF

	def bit(self, probs, i, bit):
		p = probs[i]
		bound = (self.range >> PROB_BITS) * p
		if bit:
			self.low += bound
			self.range -= bound
			probs[i] = p - (p >> SHIFT)
		else:
			self.range = bound
			probs[i] = p + (((1 << PROB_BITS) - p) >> SHIFT)
		while self.range < TOP:
			self.range = (self.range << 8) & 0xFFFFFFFF
			self.shift_low()

	def finish(self):
		for _ in range(5):
			self.shift_low()
		return bytes(self.out)


class Decoder:
	def __init__(self, data):
		self.data, self.pos = data, 5
		self.range = 0xFFFFFFFF
		self.code = int.from_bytes(data[1:5], "big")

	def bit(self, probs, i):
		p = probs[i]
		bound = (self.range >> PROB_BITS) * p
		if self.code < bound:
			self.range = bound
			probs[i] = p + (((1 << PROB_BITS) - p) >> SHIFT)
			b = 0
		else:
			self.code -= bound
			self.range -= bound
			probs[i] = p - (p >> SHIFT)
			b = 1
		while self.range < TOP:
			self.range = (self.range << 8) & 0xFFFFFFFF
			nxt = self.data[self.pos] if self.pos < len(self.data) else 0
			self.code = ((self.code << 8) | nxt) & 0xFFFFFFFF
			self.pos += 1
		return b


def symbol(coder, probs, base, nbits, value=None):
	node = 1
	for k in range(nbits - 1, -1, -1):
		if value is None:
			b = coder.bit(probs, base + node)
		else:
			b = (value >> k) & 1
			coder.bit(probs, base + node, b)
		node = node * 2 + b
	return node - (1 << nbits)


class DictContext:
	"""tracks the role of the next byte of a .zsd from the bytes so far"""

	def __init__(self):
		self.graph = self.end = None
		self.state = "other"
		self.prev_head = self.head = 0
		self.queue = []		# pointers still to come: 0 child, 1 sibling
		self.ptr_left = self.ptr_width = self.ptr_k = 0

	def context(self, i, out):
		prev = out[i - 1] if i else 0
		if self.graph is not None and self.graph <= i < self.end:
			if self.state == "head":
				return D_HEAD + self.prev_head
			if self.state == "ext":
				return D_EXT + (self.head >> 4)
			if self.state == "ptr0":
				return D_PTR0 + self.queue[0] * 16 + (self.head >> 4)
			combo = 0 if self.ptr_width == 2 else self.ptr_k
			return D_PTRN + combo * 64 + (prev >> 2)
		return D_OTHER + prev

	def after(self, i, out):
		if i == 9:
			off0 = out[7] << 16 | out[8] << 8 | out[9]
			self.off0 = off0
		if self.graph is None and i >= 9 and i == self.off0 + 3:
			size0 = out[4] << 16 | out[5] << 8 | out[6]
			region = out[self.off0 + 2] << 8 | out[self.off0 + 3]
			self.graph, self.end = self.off0 + 4 + region, self.off0 + size0
			self.state = "head"
			return
		if self.graph is None or not self.graph <= i < self.end:
			return
		b = out[i]
		if self.state == "head":
			self.prev_head, self.head = b, b
			f = NODE_FLAGS[b >> 4]
			self.queue = [w for w, (has, ex) in enumerate(((1, 2), (4, 8))) if f & has and f & ex]
			self.state = "ext" if (b & 15) == 15 else None
		elif self.state == "ext":
			self.state = None
		elif self.state == "ptr0":
			self.ptr_width = 3 if b & 0x80 else 2
			self.ptr_k = 1
			self.state = "ptrn"
			return
		elif self.state == "ptrn":
			self.ptr_k += 1
			if self.ptr_k < self.ptr_width:
				return
			self.queue.pop(0)
			self.state = None
		if self.state is None:
			self.state = "ptr0" if self.queue else "head"


def _code(coder, data, model, width, size):
	out = data if data is not None else bytearray()
	if model == MODEL_BYTES:
		probs = [PROB_INIT] * (256 * 256)
		for i in range(size):
			ctx = (out[i - 1] if i else 0) * 256
			v = symbol(coder, probs, ctx, 8, None if data is None else data[i])
			if data is None:
				out.append(v)
	elif model == MODEL_DICT:
		probs = [PROB_INIT] * (D_COUNT * 256)
		dc = DictContext()
		for i in range(size):
			ctx = dc.context(i, out) * 256
			v = symbol(coder, probs, ctx, 8, None if data is None else data[i])
			if data is None:
				out.append(v)
			dc.after(i, out)
	else:
		probs = [PROB_INIT] * (4096 * 16 if model == MODEL_IMAGE else 256 * 16)
		nibs = []
		for i in range(size * 2):
			if model == MODEL_IMAGE:
				g = lambda k: nibs[k] if k >= 0 else 0
				left = g(i - 1) if i % width else 0
				up = g(i - width)
				ul = g(i - width - 1) if i % width else 0
				ur = g(i - width + 1) if (i + 1) % width else 0
				ctx = left | up << 4 | (ul >> 2) << 8 | (ur >> 2) << 10
			else:
				ctx = (nibs[i - 2] if i > 1 else 0) << 4 | (nibs[i - 1] if i else 0)
			if data is None:
				v = symbol(coder, probs, ctx * 16, 4)
			else:
				b = data[i // 2]
				if model == MODEL_IMAGE:
					v = b >> 4 if i % 2 == 0 else b & 15
				else:
					v = b & 15 if i % 2 == 0 else b >> 4
				symbol(coder, probs, ctx * 16, 4, v)
			nibs.append(v)
			if data is None and i % 2:
				a, c = nibs[i - 1], nibs[i]
				out.append(a << 4 | c if model == MODEL_IMAGE else c << 4 | a)
	return out


def pack(data, model, width=0):
	enc = Encoder()
	_code(enc, bytes(data), model, width, len(data))
	return struct.pack(">IBH", len(data), model, width) + enc.finish()


def unpack(blob):
	size, model, width = struct.unpack_from(">IBH", blob, 0)
	return bytes(_code(Decoder(blob[7:]), None, model, width, size))


def c_array(name, blob, comment=""):
	lines = ["const u8 %s[%d] = {%s" % (name, len(blob), comment)]
	for i in range(0, len(blob), 24):
		lines.append("\t" + ",".join(str(b) for b in blob[i:i + 24]) + ",")
	lines.append("};")
	return "\n".join(lines)

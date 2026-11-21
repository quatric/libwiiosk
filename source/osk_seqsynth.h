#ifndef OSK_SEQSYNTH_H
#define OSK_SEQSYNTH_H

/* NW4R sequences (RSEQ) from a BRSAR sound archive, played through their instrument bank (RBNK)
 * and its waves: a small sequencer and a software synthesizer with the NW4R envelopes. The
 * formats match VGMTrans' RSAR reader (zlib license; see osk_seqsynth.c), and this is GodModeWii's
 * player cut down to what the keyboard's sound effects need.
 * Plain C, no console dependencies: it renders 48 kHz stereo 16-bit samples. */

#include <stdint.h>
#include <stdbool.h>

#define OSKSEQ_RATE 48000

/* Random access to the BRSAR file: false on a read error. */
typedef bool (*oskseq_read_fn)(void *ctx, uint64_t off, void *buf, uint32_t len);

typedef struct seq oskseq_t;

/* Opens the sequence sound called `name`. NULL with *err set on failure. */
oskseq_t *oskseq_open(oskseq_read_fn rd, void *ctx, uint64_t size, const char *name, const char **err);
void oskseq_close(oskseq_t *s);

/* Renders up to `frames` stereo frames into out (interleaved). Fewer, or 0, once the sequence
 * has finished; one with a loop never does. */
uint32_t oskseq_render(oskseq_t *s, int16_t *out, uint32_t frames);

#endif

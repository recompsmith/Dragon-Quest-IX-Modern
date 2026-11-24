/* Modern > Game speed and Battle speed: the game runs q/4 times as fast
 * (q in quarters: 5 = 1.25x ... 16 = 4x), so q/4 times the sound comes out.
 * The sound keeps its pitch: pieces of 62 ms are kept and the rest skipped,
 * each piece starting where the wave best matches the end of the last one
 * (WSOLA), and fading into it over 8 ms, so the joins don't click or hiccup.
 * One sample in, at most one out. Back at normal speed the sound may stay up
 * to 23 ms behind (the last join), never more. Header only: sound.c, tests/stretch. */
#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>

#define ST_RING  16384                 /* input kept, stereo samples (a power of 2) */
#define ST_GRAIN 2048                  /* one piece kept: 62 ms */
#define ST_FADE  256                   /* the join's fade: 8 ms */
#define ST_SEEK  256                   /* how far either way a join looks for a match */
#define ST_AHEAD (ST_SEEK + ST_FADE + 8)   /* the input written past the next piece's nominal start when it begins */

typedef struct {
    int32_t in[ST_RING][2];
    uint32_t w;                        /* input samples written */
    uint32_t rp;                       /* the input index read next */
    int q, want;                       /* the speed in quarters, and the one asked for */
    int acc;                           /* output due: +4 per sample in, -q per sample out */
    uint32_t next;                     /* the next piece's nominal start (input index) */
    int left;                          /* samples left in this piece */
    uint32_t from;                     /* a join: the last piece's natural continuation, fading out */
    int fade;                          /* where in the join's fade (ST_FADE: none) */
} Stretch;

static inline void st_reset(Stretch* s) { memset(s, 0, sizeof *s); s->q = s->want = 4; s->fade = ST_FADE; }
static inline void st_set(Stretch* s, int q) { s->want = q < 4 ? 4 : q; }

static inline int32_t st_mix(int32_t a, int32_t b, int t) { return (int32_t)(((int64_t)a * t + (int64_t)b * (ST_FADE - t)) / ST_FADE); }

/* a join: carry on from near `around`, where the wave looks most like what
 * would have come next (rp onwards); with nothing written there, just carry on */
static inline void st_join(Stretch* s, uint32_t around) {
    uint32_t cont = s->rp, best = cont;
    double best_score = -1e300;
    for (int d = -ST_SEEK; d <= ST_SEEK; d++) {
        uint32_t k = around + (uint32_t)d;
        if ((int32_t)(s->w - (k + ST_FADE)) < 0 || (int32_t)(s->w - k) > ST_RING - ST_FADE) continue;   /* not written yet / gone */
        double dot = 0, e = 1;
        for (int i = 0; i < ST_FADE; i++) {
            const int32_t* a = s->in[(k + (uint32_t)i) % ST_RING];
            const int32_t* b = s->in[(cont + (uint32_t)i) % ST_RING];
            double m = (double)a[0] + a[1];
            dot += m * ((double)b[0] + b[1]); e += m * m;
        }
        double score = dot / sqrt(e);
        if (score > best_score) { best_score = score; best = k; }
    }
    if (best != cont) { s->from = cont; s->fade = 0; s->rp = best; }
}

static inline int st_push(Stretch* s, int32_t l, int32_t r, int32_t out[2]) {
    s->in[s->w % ST_RING][0] = l; s->in[s->w % ST_RING][1] = r; s->w++;
    if (s->want != s->q) {
        if (s->want == 4) st_join(s, s->w - ST_FADE - ST_SEEK - 1);         /* back to normal: catch up with the input */
        else {
            if (s->q == 4) { s->left = ST_GRAIN; s->acc = 0; }
            s->next = s->w + (uint32_t)(s->left * s->want / 4) - ST_AHEAD;   /* where the next piece goes, at the new speed */
        }
        s->q = s->want;
    }
    if (s->q != 4) {
        s->acc += 4;
        if (s->acc < s->q) return 0;
        s->acc -= s->q;
        if (s->left == 0) {
            st_join(s, s->next);
            s->left = ST_GRAIN;
            s->next += (uint32_t)(ST_GRAIN * s->q / 4);
        }
        s->left--;
    }
    const int32_t* v = s->in[s->rp++ % ST_RING];
    out[0] = v[0]; out[1] = v[1];
    if (s->fade < ST_FADE) {
        const int32_t* o = s->in[s->from++ % ST_RING];
        out[0] = st_mix(v[0], o[0], s->fade); out[1] = st_mix(v[1], o[1], s->fade); s->fade++;
    }
    return 1;
}

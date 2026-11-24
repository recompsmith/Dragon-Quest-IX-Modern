/* The sound at Game speed / Battle speed (runtime/stretch.h): the game runs
 * faster, its music must come out at the same pitch, without clicks.
 * Built and run by tests/stretch/run.sh (gcc, no ROM needed). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "stretch.h"

static int pass, fail;
#define CHECK(name, cond) do { if (cond) { pass++; printf("PASS %s\n", name); } else { fail++; printf("FAIL %s\n", name); } } while (0)

#define RATE 32768.0
#define AMP 10000.0
#define N (32768 * 6)
static Stretch st;
static int32_t out[N];

/* a sine of hz from sample t0 on, through the stretcher at q quarters until
 * sample t1; returns the number of samples out (left channel kept in out[]) */
static int run(int q, double hz, int t0, int t1, int n0) {
    st_set(&st, q);
    int n = n0;
    for (int t = t0; t < t1; t++) {
        int32_t v = (int32_t)lrint(AMP * sin(2 * M_PI * hz * t / RATE)), o[2];
        if (st_push(&st, v, v, o)) { if (o[0] != o[1]) return -1; out[n++] = o[0]; }
    }
    return n;
}

/* the biggest step between two samples, against the sine's own steepest */
static double worst_step(int from, int to, double hz) {
    double m = 0;
    for (int i = from + 1; i < to; i++) { double d = fabs((double)out[i] - out[i - 1]); if (d > m) m = d; }
    return m / (AMP * 2 * M_PI * hz / RATE);
}

/* zero crossings per sample, against the sine's: the pitch */
static double pitch(int from, int to, double hz) {
    int z = 0;
    for (int i = from + 1; i < to; i++) z += (out[i - 1] < 0) != (out[i] < 0);
    return z / (double)(to - from) / (2 * hz / RATE);
}

/* the quietest peak over windows of 3 periods, against AMP: no dips where pieces join */
static double lowest_peak(int from, int to, double hz) {
    int w = (int)(3 * RATE / hz); double lo = 1e9;
    for (int s = from; s + w <= to; s += w / 2) {
        double p = 0; for (int i = s; i < s + w; i++) if (fabs((double)out[i]) > p) p = fabs((double)out[i]);
        if (p < lo) lo = p;
    }
    return lo / AMP;
}

int main(void) {
    char name[96];
    st_reset(&st);
    int n = run(4, 440, 0, N, 0);
    int same = n == N;
    for (int t = 0; same && t < N; t++) same = out[t] == (int32_t)lrint(AMP * sin(2 * M_PI * 440 * t / RATE));
    CHECK("normal speed: the sound goes through untouched", same);

    static const int qs[] = { 5, 6, 8, 12, 16 };              /* 1.25x 1.5x 2x 3x 4x */
    static const double hzs[] = { 110, 440, 1234 };
    for (int a = 0; a < 5; a++) for (int b = 0; b < 3; b++) {
        int q = qs[a]; double hz = hzs[b];
        st_reset(&st);
        n = run(q, hz, 0, N, 0);
        double want = N * 4.0 / q;
        snprintf(name, sizeof name, "%.2fx %gHz: %d samples out of %d (%.0f expected)", q / 4.0, hz, n, N, want);
        CHECK(name, n > 0 && fabs(n - want) < want * 0.01);
        int from = 4096;                                         /* past the start */
        double p = pitch(from, n, hz), s = worst_step(from, n, hz), lo = lowest_peak(from, n, hz);
        snprintf(name, sizeof name, "%.2fx %gHz: same pitch (%.3f)", q / 4.0, hz, p);
        CHECK(name, fabs(p - 1) < 0.01);
        snprintf(name, sizeof name, "%.2fx %gHz: no click (steepest step %.2f of the sine's)", q / 4.0, hz, s);
        CHECK(name, s < 1.15);
        snprintf(name, sizeof name, "%.2fx %gHz: no dip (quietest peak %.2f)", q / 4.0, hz, lo);
        CHECK(name, lo > 0.9);
    }

    /* switching on and off mid-sound (a battle begins and ends) */
    st_reset(&st);
    n = run(4, 440, 0, 20000, 0);
    n = run(6, 440, 20000, 60000, n);
    n = run(4, 440, 60000, 80000, n);
    n = run(8, 440, 80000, 120000, n);
    n = run(5, 440, 120000, 160000, n);
    n = run(4, 440, 160000, 180000, n);
    double s = worst_step(1, n, 440);
    snprintf(name, sizeof name, "switching speeds: no click (steepest step %.2f)", s);
    CHECK(name, s < 1.15);
    snprintf(name, sizeof name, "switching speeds: no dip (quietest peak %.2f)", lowest_peak(1, n, 440));
    CHECK(name, lowest_peak(1, n, 440) > 0.9);

    printf("%d passed, %d failed\n", pass, fail);
    return fail != 0;
}

/* Built-in Nintendo Wi-Fi Connection server: the five entry points of
 * runtime/nds.h, the server's state, its clock, its log and the frames
 * waiting for the DS.
 *
 * The DS's Ethernet frames come in through wfc_input (net.c, before libslirp);
 * answers queue here and leave on wfc_poll through net_deliver. Everything the
 * server sends depends only on what the DS sent, the game's clock and the
 * console's firmware: in a sealed session every PC sends the same bytes. */
#include <stdarg.h>
#include "wfc_ip.h"

static int g_on;
static char g_root[512] = "wfc";
static uint64_t g_rng;
static WfcBuf g_out;                      /* frames for the DS: [u16 length][bytes]... */

#define RNG_SEED 0x5746435345525645ull    /* any constant: the stream only has to repeat */

/* ---- byte buffer ---- */
void wfcb_put(WfcBuf* b, const void* p, size_t n) {
    if (b->failed || n == 0) return;
    if (b->n + n > b->cap) {
        size_t cap = b->cap ? b->cap : 256;
        while (cap < b->n + n) cap *= 2;
        uint8_t* q = realloc(b->p, cap);
        if (!q) { b->failed = 1; return; }
        b->p = q;
        b->cap = cap;
    }
    memcpy(b->p + b->n, p, n);
    b->n += n;
}
void wfcb_str(WfcBuf* b, const char* s) { wfcb_put(b, s, strlen(s)); }
void wfcb_fmt(WfcBuf* b, const char* fmt, ...) {
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(tmp, sizeof tmp, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    wfcb_put(b, tmp, (size_t)n < sizeof tmp ? (size_t)n : sizeof tmp - 1);
}
void wfcb_free(WfcBuf* b) {
    free(b->p);
    memset(b, 0, sizeof *b);
}

/* ---- log, clock, generator ---- */
void wfc_log(const char* fmt, ...) {
    char line[600];
    int k = snprintf(line, sizeof line, "wfc: ");
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line + k, sizeof line - (size_t)k - 1, fmt, ap);
    va_end(ap);
    strcat(line, "\n");
    report_log_line(line);
    if (getenv("DQIX_NETLOG") || wfc_debug()) fputs(line, stderr);
}

int wfc_debug(void) { return getenv("DQIX_WFCDEBUG") != NULL; }

const char* wfc_root(void) { return g_root; }

/* the game's clock: the RTC's start (local time, read as if it were UTC) plus
 * the frames since boot; never this PC's clock */
int64_t wfc_clock(void) { return rtc_base() + (int64_t)(g_frame / 60); }

/* splitmix64 (Steele, Lea, Flood 2014): integer only, same on every compiler */
uint64_t wfc_mix(uint64_t* state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

uint64_t wfc_random(void) { return wfc_mix(&g_rng); }

/* ---- frames to the DS ---- */
void wfc_send(const uint8_t* frame, int len) {
    uint8_t n[2] = { (uint8_t)(len >> 8), (uint8_t)len };
    wfcb_put(&g_out, n, 2);
    wfcb_put(&g_out, frame, (size_t)len);
}

/* ---- the five entry points ---- */
void wfc_enable(int on, const char* dir) {
    g_on = on != 0;
    snprintf(g_root, sizeof g_root, "%s", dir && *dir ? dir : "wfc");
    g_rng = RNG_SEED;
    wfcb_free(&g_out);
    wfc_ip_reset();
    wfc_dls_reset();
}

int wfc_enabled(void) { return g_on; }

void wfc_frame(void) {
    if (g_on && g_frame && g_frame % 8 == 0) wfc_nossl();   /* first pass at frame 8 */
}

void wfc_poll(void) {
    if (!g_out.n) return;
    if (!g_out.failed) {
        for (size_t i = 0; i + 2 <= g_out.n;) {
            int len = g_out.p[i] << 8 | g_out.p[i + 1];
            net_deliver(g_out.p + i + 2, len);
            i += 2 + (size_t)len;
        }
    }
    g_out.n = 0;
    g_out.failed = 0;
}

int wfc_input(const uint8_t* frame, int len) {
    if (!g_on || !frame || len < 14) return 0;
    return wfc_ip_input(frame, len);
}

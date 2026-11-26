/* DLS1 file format (see dlsfile.h). RC4: the public KSA/PRGA description,
 * checked against RFC 6229. */
#include "dlsfile.h"

#define SEAL_MOD 0xC2A030D4u

void dls_rc4(const uint8_t* key, size_t klen, uint8_t* d, size_t n) {
    uint8_t s[256];
    for (int i = 0; i < 256; i++) s[i] = (uint8_t)i;
    for (int i = 0, j = 0; i < 256; i++) {                    /* key schedule */
        j = (j + s[i] + key[(size_t)i % klen]) & 255;
        uint8_t t = s[i]; s[i] = s[j]; s[j] = t;
    }
    for (size_t k = 0, i = 0, j = 0; k < n; k++) {             /* keystream */
        i = (i + 1) & 255;
        j = (j + s[i]) & 255;
        uint8_t t = s[i]; s[i] = s[j]; s[j] = t;
        d[k] ^= s[(s[i] + s[j]) & 255];
    }
}

/* seal = (-E * 2^32) mod 0xC2A030D4, E = the encrypted bytes as one big-endian number */
uint32_t dls_seal(const uint8_t* enc, size_t n) {
    uint64_t r = 0;
    for (size_t i = 0; i < n; i++) r = (r << 8 | enc[i]) % SEAL_MOD;
    r = (r << 32) % SEAL_MOD;                                  /* r < 2^32: no overflow */
    return (uint32_t)((SEAL_MOD - r) % SEAL_MOD);
}

static void le32(uint8_t* p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

/* "YYYY/MM/DD hh:mm:ss\0YYMMDD\0" then 0xFF up to 32 bytes */
static void trailer(uint8_t out[32], int64_t t) {
    int64_t days = t >= 0 ? t / 86400 : -((-t + 86399) / 86400);
    int64_t sec = t - days * 86400;
    int64_t z = days + 719468;                                 /* civil date, H. Hinnant (public domain) */
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    int d = (int)(doy - (153 * mp + 2) / 5 + 1), mo = (int)(mp < 10 ? mp + 3 : mp - 9);
    int y = (int)(yoe + era * 400 + (mo <= 2));
    char s[40];
    memset(out, 0xFF, 32);
    int n = snprintf(s, sizeof s, "%04d/%02d/%02d %02d:%02d:%02d", y % 10000, mo, d,
                     (int)(sec / 3600), (int)(sec / 60 % 60), (int)(sec % 60));
    memcpy(out, s, (size_t)n + 1);
    n = snprintf(s, sizeof s, "%02d%02d%02d", y % 100, mo, d);
    memcpy(out + 20, s, (size_t)n + 1);
}

void dls_build(WfcBuf* out, uint32_t number, const uint8_t* elements, size_t n, int64_t t) {
    static const uint32_t k_head[6] = { 27, 2, 0x10065, 0, 0x10064, 20 };
    size_t start = out->n;
    uint8_t h[32], tr[32];
    le32(h, number);
    le32(h + 4, (uint32_t)(n + 32));
    for (int i = 0; i < 6; i++) le32(h + 8 + 4 * i, k_head[i]);
    trailer(tr, t);
    wfcb_put(out, h, 32);
    wfcb_put(out, elements, n);
    wfcb_put(out, tr, 32);
    if (out->failed) return;
    uint8_t* body = out->p + start;
    size_t len = out->n - start;
    dls_rc4((const uint8_t*)DLS_KEY, sizeof DLS_KEY - 1, body, len);
    uint32_t s = dls_seal(body, len);
    uint8_t be[4] = { (uint8_t)(s >> 24), (uint8_t)(s >> 16), (uint8_t)(s >> 8), (uint8_t)s };
    wfcb_put(out, be, 4);
}

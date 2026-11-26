/* DQVC stock of the day (placeholder: the day and its file, no sales yet). */
#include "dqvc.h"
#include "dlsfile.h"

#define DQVC_EPOCH 1277942400             /* 2010-07-01 00:00:00 UTC */

static int64_t floordiv(int64_t a, int64_t b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

int64_t dqvc_day(int64_t utc) { return floordiv(utc - 8 * 3600 - DQVC_EPOCH, 86400); }

int64_t dqvc_cycle_start(int year) { (void)year; return 0; }

void dqvc_today(DqvcStock* s) {
    memset(s, 0, sizeof *s);
    s->day = dqvc_day(wfc_clock() - rtc_utc_offset());
    s->number = (uint32_t)(1000 + s->day);
    snprintf(s->name, sizeof s->name, "dqvc_d%05lld.bin", (long long)s->day);
    s->count = DQVC_ITEMS;
}

static void le32(uint8_t* p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

void dqvc_file(const DqvcStock* s, WfcBuf* out) {
    uint8_t e[16 + 20 * DQVC_ITEMS];
    le32(e, 0x15030067); le32(e + 4, 2099); le32(e + 8, 12); le32(e + 12, 31);   /* sale ends 2099-12-31 */
    for (int i = 0; i < s->count; i++) {
        uint8_t* r = e + 16 + 20 * i;
        le32(r, 0x55043068); le32(r + 4, s->item[i]); le32(r + 8, s->price[i]); le32(r + 12, 1); le32(r + 16, s->qty[i]);
    }
    dls_build(out, s->number, e, 16 + 20 * (size_t)s->count, wfc_clock());
}

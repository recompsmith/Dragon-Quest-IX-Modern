/* Built-in server: HTTP/1.x (RFC 9112): the connection test, the login
 * server (NAS, POST /ac) and the way to the download server (POST /download).
 *
 * Every answer: status line, Date (the game's clock), Server, Content-Type,
 * the answer's own fields, Content-Length, "Connection: close". Form fields
 * (NAS and download server) are base64 (RFC 4648 § 4) with '*' for '=',
 * joined by '&'. */
#include "wfc_ip.h"

/* ---- request framing: the head ends with an empty line, then Content-Length bytes ---- */

static size_t head_len(const uint8_t* r, size_t n) {
    for (size_t i = 0; i + 3 < n; i++)
        if (r[i] == '\r' && r[i + 1] == '\n' && r[i + 2] == '\r' && r[i + 3] == '\n') return i + 4;
    return 0;
}

static size_t content_length(const uint8_t* r, size_t hl) {
    static const char key[] = "content-length:";
    for (size_t i = 0; i + sizeof key < hl; i++) {
        if (i && r[i - 1] != '\n') continue;
        size_t k = 0;
        while (key[k] && (r[i + k] | 0x20) == (key[k] | 0x20)) k++;
        if (key[k]) continue;
        size_t v = 0, j = i + k;
        while (j < hl && r[j] == ' ') j++;
        while (j < hl && r[j] >= '0' && r[j] <= '9' && v < 100000000) v = v * 10 + (size_t)(r[j++] - '0');
        return v;
    }
    return 0;
}

/* ---- base64 with '*' ---- */

static const char k_b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int b64val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '.' || c == ' ') return 62;   /* '.' and ' ': '+' as some clients send it */
    if (c == '/' || c == '-') return 63;
    return -1;
}

void wfc_form_put(WfcBuf* b, const char* key, const char* value) {
    if (b->n) wfcb_str(b, "&");
    wfcb_str(b, key);
    wfcb_str(b, "=");
    const uint8_t* v = (const uint8_t*)value;
    size_t n = strlen(value);
    for (size_t i = 0; i < n; i += 3) {
        uint32_t w = (uint32_t)v[i] << 16 | (i + 1 < n ? (uint32_t)v[i + 1] << 8 : 0) | (i + 2 < n ? v[i + 2] : 0);
        char q[4] = { k_b64[w >> 18 & 63], k_b64[w >> 12 & 63], i + 1 < n ? k_b64[w >> 6 & 63] : '*',
                      i + 2 < n ? k_b64[w & 63] : '*' };
        wfcb_put(b, q, 4);
    }
}

/* the decoded value of `key` in the form (NUL-terminated), 1 if present */
int wfc_form_get(const char* form, const char* key, char* out, size_t cap) {
    size_t kl = strlen(key);
    out[0] = 0;
    for (const char* p = form; *p;) {
        const char* e = strchr(p, '&');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        if (len > kl && !strncmp(p, key, kl) && p[kl] == '=') {
            size_t o = 0;
            uint32_t acc = 0;
            int bits = 0;
            for (size_t i = kl + 1; i < len; i++) {
                int v = b64val((unsigned char)p[i]);
                if (v < 0) continue;                       /* '*', '=', line breaks */
                acc = acc << 6 | (uint32_t)v;
                bits += 6;
                if (bits >= 8) {
                    bits -= 8;
                    if (o + 1 < cap) out[o++] = (char)(acc >> bits & 0xFF);
                }
            }
            out[o] = 0;
            return 1;
        }
        if (!e) break;
        p = e + 1;
    }
    return 0;
}

/* ---- the game's clock as text ---- */

typedef struct { int y, mo, d, h, mi, s, wd; } Civil;

/* days since 1970-01-01 -> date (Howard Hinnant, "chrono-Compatible Low-Level
 * Date Algorithms", public domain); integer only */
static Civil civil(int64_t t) {
    Civil c;
    int64_t days = t >= 0 ? t / 86400 : -((-t + 86399) / 86400);
    int64_t sec = t - days * 86400;
    c.h = (int)(sec / 3600); c.mi = (int)(sec / 60 % 60); c.s = (int)(sec % 60);
    c.wd = (int)(((days % 7) + 11) % 7);              /* 1970-01-01 was a Thursday (4) */
    int64_t z = days + 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    c.d = (int)(doy - (153 * mp + 2) / 5 + 1);
    c.mo = (int)(mp < 10 ? mp + 3 : mp - 9);
    c.y = (int)(yoe + era * 400 + (c.mo <= 2));
    return c;
}

void wfc_http_head(WfcBuf* b, const char* status, const char* ctype) {
    static const char* wd[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char* mo[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    Civil c = civil(wfc_clock());
    wfcb_fmt(b, "HTTP/1.1 %s\r\nDate: %s, %02d %s %04d %02d:%02d:%02d GMT\r\nServer: Nintendo Wii (http)\r\n"
             "Content-Type: %s\r\n", status, wd[c.wd], c.d, mo[c.mo - 1], c.y, c.h, c.mi, c.s, ctype);
}

void wfc_http_end(WfcBuf* b, const void* body, size_t n) {
    wfcb_fmt(b, "Content-Length: %lu\r\nConnection: close\r\n\r\n", (unsigned long)n);
    wfcb_put(b, body, n);
}

/* ---- NAS: POST /ac ---- */

static void token(char out[100]) {
    static const char a[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    memcpy(out, "NDS", 3);
    for (int i = 0; i < 96; i++) out[3 + i] = a[(wfc_random() >> 32) % 62];
    out[99] = 0;
}

static void nas(const char* form, WfcBuf* out) {
    char action[64], gamecd[16], svc[64], userid[64], tok[100], when[16];
    wfc_form_get(form, "action", action, sizeof action);
    wfc_form_get(form, "gamecd", gamecd, sizeof gamecd);
    if (!wfc_form_get(form, "svc", svc, sizeof svc) || !svc[0]) strcpy(svc, "-");
    Civil c = civil(wfc_clock());
    snprintf(when, sizeof when, "%04d%02d%02d%02d%02d%02d", c.y % 10000, c.mo, c.d, c.h, c.mi, c.s);
    WfcBuf body;
    memset(&body, 0, sizeof body);
    wfc_form_put(&body, "retry", "0");
    if (!strcmp(action, "acctcreate")) {
        wfc_form_get(form, "userid", userid, sizeof userid);
        wfc_form_put(&body, "returncd", "002");
        wfc_form_put(&body, "userid", userid);
    } else if (!strcmp(action, "login")) {
        token(tok);
        wfc_form_put(&body, "returncd", "001");
        wfc_form_put(&body, "locator", "gamespy.com");
        wfc_form_put(&body, "challenge", "DQIXPC00");
        wfc_form_put(&body, "token", tok);
    } else {                                     /* svcloc, and anything else */
        token(tok);
        wfc_form_put(&body, "returncd", "007");
        wfc_form_put(&body, "statusdata", "Y");
        wfc_form_put(&body, "svchost", "dls1.nintendowifi.net");
        wfc_form_put(&body, "servicetoken", tok);
    }
    wfc_form_put(&body, "datetime", when);
    wfcb_str(&body, "\r\n");
    wfc_log("login (%s, game %s, service %s) accepted", action, gamecd, svc);
    wfc_http_head(out, "200 OK", "text/plain");
    wfcb_str(out, "NODE: wifiappe1\r\n");
    wfc_http_end(out, body.p, body.n);
    wfcb_free(&body);
}

/* ---- dispatch ---- */

int wfc_http(const uint8_t* req, size_t n, WfcBuf* out) {
    size_t hl = head_len(req, n);
    if (!hl) return 0;
    size_t cl = content_length(req, hl);
    if (n < hl + cl) return 0;
    if (!out) return 1;

    char method[16], path[256];
    size_t i = 0, k = 0;
    while (i < hl && req[i] != ' ' && k + 1 < sizeof method) method[k++] = (char)req[i++];
    method[k] = 0;
    while (i < hl && req[i] == ' ') i++;
    k = 0;
    while (i < hl && req[i] != ' ' && req[i] != '\r' && k + 1 < sizeof path) path[k++] = (char)req[i++];
    path[k] = 0;
    char* q = strchr(path, '?');
    if (q) *q = 0;

    char* form = malloc(cl + 1);
    if (!form) return 1;
    memcpy(form, req + hl, cl);
    form[cl] = 0;
    if (wfc_debug()) fprintf(stderr, "wfc: request\n%.*s\n", (int)(hl + cl), (const char*)req);

    if (!strcmp(method, "GET")) {                   /* connection test: any host, any path */
        wfc_log("connection test answered");
        wfc_http_head(out, "200 OK", "text/html");
        wfcb_str(out, "X-Organization: Nintendo\r\nServer: BigIP\r\n");
        wfc_http_end(out, "ok", 2);
    } else if (!strcmp(method, "POST") && !strcmp(path, "/ac")) {
        nas(form, out);
    } else {
        wfc_log("%s %s: not handled", method, path);
        wfc_http_head(out, "404 Not Found", "text/plain");
        wfc_http_end(out, "", 0);
    }
    free(form);
    if (wfc_debug() && !out->failed) {
        size_t e = head_len(out->p, out->n);
        fprintf(stderr, "wfc: answer (%lu bytes)\n%.*s\n", (unsigned long)out->n, (int)(e ? e : out->n), (const char*)out->p);
    }
    return 1;
}

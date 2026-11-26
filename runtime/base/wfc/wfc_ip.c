/* Built-in server: Ethernet, IPv4 (RFC 791), UDP (RFC 768), DNS (RFC 1035)
 * and the server side of TCP (RFC 9293), just enough for the DS's HTTP
 * requests. Checksums: RFC 1071.
 *
 * Taken from the DS: UDP to port 53 at any address (DNS), TCP to the server's
 * address (port 80 answered, other ports reset). Everything else goes on to
 * libslirp (ARP and DHCP included). */
#include "wfc_ip.h"

static const uint8_t k_router_mac[6] = { 0x52, 0x54, 0x00, 0x12, 0x35, 0x02 };   /* libslirp's */
static const uint8_t k_srv_ip[4] = { WFC_IP0, WFC_IP1, WFC_IP2, WFC_IP3 };
static uint16_t g_ip_id;

#define WIN 8192
#define MAX_CONN 8
#define MAX_REQ (256 * 1024)

enum { C_FREE, C_OPEN, C_DONE };
typedef struct {
    int state;
    uint8_t mac[6], ip[4];
    uint16_t port;
    uint32_t iss;             /* our first sequence number */
    uint32_t snd_nxt;         /* next sequence number we send */
    uint32_t rcv_nxt;         /* next byte expected from the DS */
    uint32_t age;             /* for reuse: the oldest goes first */
    WfcBuf req;
} Conn;
static Conn g_conn[MAX_CONN];
static uint32_t g_age;

static uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] << 8 | p[1]); }
static uint32_t rd32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static void wr16(uint8_t* p, uint32_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }
static void wr32(uint8_t* p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

/* RFC 1071: one's complement sum of 16-bit words */
static uint32_t sum16(uint32_t s, const uint8_t* p, size_t n) {
    for (size_t i = 0; i + 1 < n; i += 2) s += (uint32_t)(p[i] << 8 | p[i + 1]);
    if (n & 1) s += (uint32_t)p[n - 1] << 8;
    return s;
}
static uint16_t fold(uint32_t s) {
    while (s >> 16) s = (s & 0xFFFF) + (s >> 16);
    return (uint16_t)~s;
}

/* sends Ethernet + IPv4 around an UDP or TCP segment already in f[34..],
 * filling both checksums */
static void send_ip(uint8_t* f, const uint8_t dst_mac[6], const uint8_t src_ip[4], const uint8_t dst_ip[4],
                    int proto, int seglen) {
    memcpy(f, dst_mac, 6);
    memcpy(f + 6, k_router_mac, 6);
    f[12] = 0x08; f[13] = 0x00;
    uint8_t* h = f + 14;
    h[0] = 0x45; h[1] = 0;
    wr16(h + 2, (uint32_t)(20 + seglen));
    wr16(h + 4, ++g_ip_id);
    wr16(h + 6, 0x4000);                          /* don't fragment */
    h[8] = 64; h[9] = (uint8_t)proto;
    wr16(h + 10, 0);
    memcpy(h + 12, src_ip, 4);
    memcpy(h + 16, dst_ip, 4);
    wr16(h + 10, fold(sum16(0, h, 20)));
    uint8_t* s = h + 20;
    uint8_t ph[12];                               /* pseudo-header */
    memcpy(ph, src_ip, 4); memcpy(ph + 4, dst_ip, 4);
    ph[8] = 0; ph[9] = (uint8_t)proto; wr16(ph + 10, (uint32_t)seglen);
    int at = proto == 17 ? 6 : 16;
    wr16(s + at, 0);
    uint16_t c = fold(sum16(sum16(0, ph, 12), s, (size_t)seglen));
    if (proto == 17 && c == 0) c = 0xFFFF;        /* RFC 768: 0 means "no checksum" */
    wr16(s + at, c);
    wfc_send(f, 34 + seglen);
}

/* ---- DNS ---- */

/* reads the question's name (labels, RFC 1035 § 4.1.4 pointers followed);
 * returns the offset just after it in the message, or -1 */
static int dns_name(const uint8_t* m, int n, int off, char* out, int cap) {
    int o = 0, end = -1, jumps = 0;
    out[0] = 0;
    for (;;) {
        if (off >= n) return -1;
        int l = m[off];
        if ((l & 0xC0) == 0xC0) {
            if (off + 1 >= n || ++jumps > 16) return -1;
            if (end < 0) end = off + 2;
            off = (l & 0x3F) << 8 | m[off + 1];
            continue;
        }
        if (l & 0xC0) return -1;
        if (l == 0) break;
        if (off + 1 + l > n) return -1;
        if (o + l + 2 > cap) return -1;
        if (o) out[o++] = '.';
        memcpy(out + o, m + off + 1, (size_t)l);
        o += l;
        out[o] = 0;
        off += 1 + l;
    }
    return end >= 0 ? end : off + 1;
}

/* *.nintendowifi.net (and the bare name) are ours; compared as the DS sends
 * them (lower case) */
static int ours(const char* name) {
    static const char dom[] = "nintendowifi.net";
    size_t n = strlen(name), d = sizeof dom - 1;
    if (n < d || strcmp(name + n - d, dom)) return 0;
    return n == d || name[n - d - 1] == '.';
}

static void dns(const uint8_t* f, const uint8_t* ih, const uint8_t* u, int ulen) {
    const uint8_t* m = u + 8;
    int n = ulen - 8;
    if (n < 12 || rd16(m + 4) < 1) return;
    char name[256];
    int q = dns_name(m, n, 12, name, sizeof name);
    if (q < 0 || q + 4 > n) return;
    q += 4;                                       /* type, class */
    int hit = ours(name);
    wfc_log("%s -> %s", name, hit ? "built-in server" : "not found");
    static uint8_t o[14 + 20 + 8 + 512 + 16];
    if (q > 512) return;
    uint8_t* d = o + 42;
    memcpy(d, m, (size_t)q);                      /* id and the question */
    wr16(d + 2, hit ? 0x8180 : 0x8183);           /* answer, recursion; NXDOMAIN */
    wr16(d + 4, 1); wr16(d + 6, hit ? 1 : 0); wr16(d + 8, 0); wr16(d + 10, 0);
    int len = q;
    if (hit) {
        static const uint8_t a[16] = { 0xC0, 0x0C, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4, WFC_IP0, WFC_IP1, WFC_IP2, WFC_IP3 };
        memcpy(d + len, a, sizeof a);
        len += (int)sizeof a;
    }
    uint8_t* s = o + 34;
    wr16(s, 53); wr16(s + 2, rd16(u)); wr16(s + 4, (uint32_t)(8 + len));
    send_ip(o, f + 6, ih + 16, ih + 12, 17, 8 + len);
}

/* ---- TCP ---- */

static void tcp_out(const Conn* c, uint32_t seq, uint32_t ack, int flags, const uint8_t* data, int n, int mss) {
    static uint8_t o[14 + 20 + 24 + WFC_MSS];
    uint8_t* t = o + 34;
    int hl = mss ? 24 : 20;
    wr16(t, 80); wr16(t + 2, c->port);
    wr32(t + 4, seq); wr32(t + 8, ack);
    t[12] = (uint8_t)(hl / 4) << 4; t[13] = (uint8_t)flags;
    wr16(t + 14, WIN); wr16(t + 16, 0); wr16(t + 18, 0);
    if (mss) { t[20] = 2; t[21] = 4; wr16(t + 22, WFC_MSS); }
    if (n) memcpy(t + hl, data, (size_t)n);
    send_ip(o, c->mac, k_srv_ip, c->ip, 6, hl + n);
}

/* reset for a segment we do not accept (RFC 9293 § 3.10.7.1) */
static void tcp_reset(const uint8_t* f, const uint8_t* ih, const uint8_t* t, int datalen) {
    Conn c;
    memset(&c, 0, sizeof c);
    memcpy(c.mac, f + 6, 6);
    memcpy(c.ip, ih + 12, 4);
    c.port = rd16(t);
    int fl = t[13];
    if (fl & 0x04) return;                        /* never answer a reset */
    static uint8_t o[14 + 20 + 20];
    uint8_t* s = o + 34;
    uint32_t seq = fl & 0x10 ? rd32(t + 8) : 0;
    uint32_t ack = rd32(t + 4) + (uint32_t)datalen + (fl & 0x02 ? 1 : 0) + (fl & 0x01 ? 1 : 0);
    wr16(s, rd16(t + 2)); wr16(s + 2, c.port);
    wr32(s + 4, seq); wr32(s + 8, ack);
    s[12] = 5 << 4; s[13] = fl & 0x10 ? 0x04 : 0x14;
    wr16(s + 14, 0); wr16(s + 16, 0); wr16(s + 18, 0);
    send_ip(o, c.mac, ih + 16, c.ip, 6, 20);
}

static Conn* find(const uint8_t* ip, uint16_t port) {
    for (int i = 0; i < MAX_CONN; i++)
        if (g_conn[i].state != C_FREE && g_conn[i].port == port && !memcmp(g_conn[i].ip, ip, 4)) return &g_conn[i];
    return NULL;
}

static Conn* open_conn(void) {
    Conn* best = &g_conn[0];
    for (int i = 0; i < MAX_CONN; i++) {
        if (g_conn[i].state == C_FREE) { best = &g_conn[i]; break; }
        if (g_conn[i].age < best->age) best = &g_conn[i];
    }
    wfcb_free(&best->req);
    memset(best, 0, sizeof *best);
    return best;
}

static void close_conn(Conn* c) {
    wfcb_free(&c->req);
    memset(c, 0, sizeof *c);
}

/* the whole request is in: the answer goes out at once (bare ACK, data, FIN) */
static void answer(Conn* c) {
    WfcBuf r;
    memset(&r, 0, sizeof r);
    if (!wfc_http(c->req.p, c->req.n, &r)) { wfcb_free(&r); return; }
    tcp_out(c, c->snd_nxt, c->rcv_nxt, 0x10, NULL, 0, 0);
    for (size_t i = 0; !r.failed && i < r.n; i += WFC_MSS) {
        int k = (int)(r.n - i < WFC_MSS ? r.n - i : WFC_MSS);
        tcp_out(c, c->snd_nxt, c->rcv_nxt, 0x18, r.p + i, k, 0);
        c->snd_nxt += (uint32_t)k;
    }
    tcp_out(c, c->snd_nxt, c->rcv_nxt, 0x11, NULL, 0, 0);
    c->snd_nxt++;
    c->state = C_DONE;
    wfcb_free(&r);
    wfcb_free(&c->req);
}

static void tcp(const uint8_t* f, const uint8_t* ih, const uint8_t* t, int tlen) {
    if (tlen < 20) return;
    int off = (t[12] >> 4) * 4;
    if (off < 20 || off > tlen) return;
    int fl = t[13];
    uint16_t sport = rd16(t), dport = rd16(t + 2);
    uint32_t seq = rd32(t + 4);
    const uint8_t* data = t + off;
    int n = tlen - off;
    if (dport != 80) {
        if ((fl & 0x12) == 0x02) wfc_log("refused a connection to port %d", dport);
        tcp_reset(f, ih, t, n);
        return;
    }
    Conn* c = find(ih + 12, sport);
    if ((fl & 0x17) == 0x02) {                    /* SYN */
        if (c && c->state == C_OPEN && c->rcv_nxt == seq + 1 && c->snd_nxt == c->iss + 1) {
            tcp_out(c, c->iss, c->rcv_nxt, 0x12, NULL, 0, 1);   /* our SYN-ACK was lost */
            return;
        }
        if (c) close_conn(c);
        c = open_conn();
        c->state = C_OPEN;
        memcpy(c->mac, f + 6, 6);
        memcpy(c->ip, ih + 12, 4);
        c->port = sport;
        c->iss = (uint32_t)(wfc_random() >> 32);
        c->snd_nxt = c->iss + 1;
        c->rcv_nxt = seq + 1;
        c->age = ++g_age;
        tcp_out(c, c->iss, c->rcv_nxt, 0x12, NULL, 0, 1);
        return;
    }
    if (!c) { tcp_reset(f, ih, t, n); return; }
    if (fl & 0x04) { close_conn(c); return; }
    if (c->state == C_OPEN && n > 0) {
        if (seq == c->rcv_nxt) {
            if (c->req.n + (size_t)n > MAX_REQ) {          /* not a request of the game's */
                tcp_reset(f, ih, t, n);
                close_conn(c);
                return;
            }
            wfcb_put(&c->req, data, (size_t)n);
            c->rcv_nxt += (uint32_t)n;
            if (c->req.failed) { tcp_reset(f, ih, t, n); close_conn(c); return; }
            if (!wfc_http(c->req.p, c->req.n, NULL)) {     /* more to come: acknowledge */
                tcp_out(c, c->snd_nxt, c->rcv_nxt, 0x10, NULL, 0, 0);
                return;
            }
            answer(c);
        } else {
            tcp_out(c, c->snd_nxt, c->rcv_nxt, 0x10, NULL, 0, 0);   /* out of order: say where we are */
        }
        return;
    }
    if (fl & 0x01) {                              /* the DS closes */
        if (seq + (uint32_t)n == c->rcv_nxt || seq == c->rcv_nxt) {
            c->rcv_nxt = seq + (uint32_t)n + 1;
            tcp_out(c, c->snd_nxt, c->rcv_nxt, 0x10, NULL, 0, 0);
            if (c->state == C_DONE) close_conn(c);
            else { tcp_out(c, c->snd_nxt, c->rcv_nxt, 0x11, NULL, 0, 0); c->snd_nxt++; c->state = C_DONE; }
        }
    }
}

/* ---- entry ---- */

void wfc_ip_reset(void) {
    for (int i = 0; i < MAX_CONN; i++) close_conn(&g_conn[i]);
    g_ip_id = 0;
    g_age = 0;
}

int wfc_ip_input(const uint8_t* f, int len) {
    if (f[12] != 0x08 || f[13] != 0x00 || len < 34) return 0;
    const uint8_t* ih = f + 14;
    if (ih[0] >> 4 != 4) return 0;
    int ihl = (ih[0] & 15) * 4;
    int tot = rd16(ih + 2);
    if (ihl < 20 || tot < ihl || 14 + tot > len) return 0;
    if (rd16(ih + 6) & 0x3FFF) return 0;          /* fragments: not the DS's requests */
    const uint8_t* l4 = ih + ihl;
    int n = tot - ihl;
    if (ih[9] == 17 && n >= 8 && rd16(l4 + 2) == 53) {
        int ul = rd16(l4 + 4);
        if (ul < 8 || ul > n) return 1;
        dns(f, ih, l4, ul);
        return 1;
    }
    if (ih[9] == 6 && !memcmp(ih + 16, k_srv_ip, 4)) {
        tcp(f, ih, l4, n);
        return 1;
    }
    return 0;
}

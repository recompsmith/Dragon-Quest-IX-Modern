/* Built-in Nintendo Wi-Fi Connection server: what its files share.
 * Nothing here is visible outside the server (ELF: hidden symbols). */
#pragma once
#include "nds.h"

#if defined(__ELF__) && defined(__GNUC__)
#pragma GCC visibility push(hidden)
#endif

/* the server's address (TEST-NET-1, RFC 5737), given to every *.nintendowifi.net */
#define WFC_IP0 192
#define WFC_IP1 0
#define WFC_IP2 2
#define WFC_IP3 80
#define WFC_MSS 1400                      /* largest segment sent to the DS */

/* a growing byte buffer (answers, file contents) */
typedef struct {
    uint8_t* p;
    size_t n, cap;
    int failed;                           /* out of memory: the answer is dropped */
} WfcBuf;
void wfcb_put(WfcBuf* b, const void* p, size_t n);
void wfcb_str(WfcBuf* b, const char* s);
void wfcb_fmt(WfcBuf* b, const char* fmt, ...);
void wfcb_free(WfcBuf* b);

/* wfc.c */
void wfc_log(const char* fmt, ...);       /* one "wfc: ..." line for the F10 report */
int wfc_debug(void);                      /* DQIX_WFCDEBUG: requests and answers on stderr */
const char* wfc_root(void);               /* folder with one sub-folder per game code */
int64_t wfc_clock(void);                  /* the game's clock, seconds (spec: no UTC offset) */
uint64_t wfc_mix(uint64_t* state);        /* splitmix64: the server's only generator */
uint64_t wfc_random(void);                /* the server's own stream (constant seed) */
void wfc_send(const uint8_t* frame, int len);   /* a frame to the DS (queued, out on wfc_poll) */

/* wfc_ip.c */
int wfc_ip_input(const uint8_t* frame, int len);
void wfc_ip_reset(void);

/* wfc_http.c: one HTTP request. Returns 0 while it is incomplete, 1 when the
 * answer is in *out. */
int wfc_http(const uint8_t* req, size_t n, WfcBuf* out);
/* form fields: base64 (RFC 4648) with '*' for '=', joined by '&' */
int wfc_form_get(const char* form, const char* key, char* out, size_t cap);
void wfc_form_put(WfcBuf* b, const char* key, const char* value);
/* the common head of an answer: status line, Date, Server, Content-Type */
void wfc_http_head(WfcBuf* b, const char* status, const char* ctype);
void wfc_http_end(WfcBuf* b, const void* body, size_t n);   /* Content-Length, Connection, body */

/* wfc_dls.c: POST /download */
void wfc_dls(const char* form, WfcBuf* out);

/* nossl.c */
void wfc_nossl(void);

#if defined(__ELF__) && defined(__GNUC__)
#pragma GCC visibility pop
#endif

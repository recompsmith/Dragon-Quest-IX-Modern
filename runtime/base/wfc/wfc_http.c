/* Built-in server: HTTP/1.x requests (RFC 9112): the head ends with an empty
 * line, then Content-Length bytes of body. */
#include "wfc_ip.h"

/* length of the head (through the empty line), or 0 while it is incomplete */
static size_t head_len(const uint8_t* r, size_t n) {
    for (size_t i = 0; i + 3 < n; i++)
        if (r[i] == '\r' && r[i + 1] == '\n' && r[i + 2] == '\r' && r[i + 3] == '\n') return i + 4;
    return 0;
}

/* Content-Length of the head (name case-insensitive), 0 when absent */
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

int wfc_http(const uint8_t* req, size_t n, WfcBuf* out) {
    size_t hl = head_len(req, n);
    if (!hl || n < hl + content_length(req, hl)) return 0;
    if (!out) return 1;
    wfcb_str(out, "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    return 1;
}

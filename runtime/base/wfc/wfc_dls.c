/* Built-in server: the download server (DLS1, POST /download).
 *
 * Files live in <root>/<game code>/. A game's list comes from its _list.txt
 * (DLS1 format: name, "", attr1, attr2, attr3, size, tab-separated), in
 * order; for YDQE and YDQP its "auction" line becomes the DQVC file of the
 * day (added when the list has none). Without _list.txt, YDQE and YDQP get
 * the built-in list: the quest file and the news when they are in the folder,
 * and the DQVC file of the day. Only listed names can be downloaded. */
#include "wfc_ip.h"
#include "dqvc.h"

#define MAX_FILES 64
#define MAX_LIST (64 * 1024)
#define MAX_FILE (8 * 1024 * 1024)

typedef struct {
    char name[64], attr1[32], size[16];
    int dqvc;                             /* the DQVC file of the day */
} Entry;

typedef struct {
    Entry e[MAX_FILES];
    int n;
    DqvcStock stock;
    WfcBuf file;                          /* the day's DQVC file, made once per request */
} List;

static char g_warned[4][16];               /* game codes already reported as empty */

static int valid_code(const char* gc) {
    size_t n = strlen(gc);
    if (n < 1 || n > 8) return 0;
    for (size_t i = 0; i < n; i++) {
        char c = gc[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) return 0;
    }
    return 1;
}

static int has_dqvc(const char* gc) { return !strcmp(gc, "YDQE") || !strcmp(gc, "YDQP"); }

/* a whole file of the game's folder; NULL when absent (or too large) */
static uint8_t* read_file(const char* gc, const char* name, size_t* len) {
    char path[700];
    snprintf(path, sizeof path, "%s/%s/%s", wfc_root(), gc, name);
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    uint8_t* p = NULL;
    long n = -1;
    if (!fseek(f, 0, SEEK_END)) n = ftell(f);
    if (n >= 0 && n <= MAX_FILE && !fseek(f, 0, SEEK_SET)) {
        p = malloc((size_t)n + 1);
        if (p && fread(p, 1, (size_t)n, f) != (size_t)n) { free(p); p = NULL; }
    }
    fclose(f);
    if (p) { p[n] = 0; *len = (size_t)n; }
    return p;
}

static void add(List* l, const char* name, const char* attr1, const char* size, int dqvc) {
    if (l->n >= MAX_FILES) return;
    Entry* e = &l->e[l->n++];
    snprintf(e->name, sizeof e->name, "%s", name);
    snprintf(e->attr1, sizeof e->attr1, "%s", attr1);
    snprintf(e->size, sizeof e->size, "%s", size);
    e->dqvc = dqvc;
}

static void add_dqvc(List* l) {
    if (!l->file.n) {
        dqvc_today(&l->stock);
        dqvc_file(&l->stock, &l->file);
    }
    char size[16];
    snprintf(size, sizeof size, "%lu", (unsigned long)l->file.n);
    add(l, l->stock.name, "auction", size, 1);
}

/* a file listed only when it is in the folder, with its real size */
static void add_present(List* l, const char* gc, const char* name, const char* attr1) {
    size_t n;
    uint8_t* p = read_file(gc, name, &n);
    if (!p) return;
    free(p);
    char size[16];
    snprintf(size, sizeof size, "%lu", (unsigned long)n);
    add(l, name, attr1, size, 0);
}

static void build_list(List* l, const char* gc) {
    if (!valid_code(gc)) return;
    size_t n;
    char* t = (char*)read_file(gc, "_list.txt", &n);
    if (t && n <= MAX_LIST) {
        int auction = 0;
        for (char* p = t; *p;) {
            char* e = strchr(p, '\n');
            if (e) *e = 0;
            size_t k = strlen(p);
            while (k && p[k - 1] == '\r') p[--k] = 0;          /* \n, \r\n, \r\r\n */
            char* f[6] = { 0 };
            int nf = 0;
            for (char* q = p; nf < 6;) {
                f[nf++] = q;
                char* tab = strchr(q, '\t');
                if (!tab) break;
                *tab = 0;
                q = tab + 1;
            }
            if (k && f[0][0]) {
                const char* a1 = nf > 2 ? f[2] : "";
                const char* size = nf > 5 ? f[5] : nf > 0 ? f[nf - 1] : "";
                if (has_dqvc(gc) && !strcmp(a1, "auction")) {
                    if (!auction++) add_dqvc(l);
                } else {
                    add(l, f[0], a1, size, 0);
                }
            }
            if (!e) break;
            p = e + 1;
        }
        if (has_dqvc(gc) && !auction) add_dqvc(l);
    } else if (has_dqvc(gc)) {
        add_dqvc(l);
        add_present(l, gc, !strcmp(gc, "YDQE") ? "infomes_47.bin" : "infomes_45.bin", "mes");
    } else {
        int i, free_slot = -1;
        for (i = 0; i < 4 && strcmp(g_warned[i], gc); i++)
            if (free_slot < 0 && !g_warned[i][0]) free_slot = i;
        if (i == 4) {
            wfc_log("no files in %s/%s (copy the game's files there)", wfc_root(), gc);
            if (free_slot >= 0) snprintf(g_warned[free_slot], sizeof g_warned[0], "%s", gc);
        }
    }
    free(t);
}

void wfc_dls_reset(void) { memset(g_warned, 0, sizeof g_warned); }

void wfc_dls(const char* form, WfcBuf* out) {
    static List l;                                   /* large: not on the stack */
    char action[32], gc[16], a1[32], a2[32], a3[32], name[128];
    wfc_form_get(form, "action", action, sizeof action);
    wfc_form_get(form, "gamecd", gc, sizeof gc);
    wfc_form_get(form, "attr1", a1, sizeof a1);
    wfc_form_get(form, "attr2", a2, sizeof a2);
    wfc_form_get(form, "attr3", a3, sizeof a3);
    wfc_form_get(form, "contents", name, sizeof name);
    wfcb_free(&l.file);
    memset(&l, 0, sizeof l);
    build_list(&l, gc);

    if (!strcmp(action, "contents")) {
        const Entry* e = NULL;
        for (int i = 0; i < l.n && !e; i++)
            if (!strcmp(l.e[i].name, name)) e = &l.e[i];
        uint8_t* data = NULL;
        size_t n = 0;
        if (e && e->dqvc) {
            data = l.file.p; n = l.file.n;
        } else if (e && !strchr(name, '/') && !strchr(name, '\\') && strcmp(name, "..") && strcmp(name, ".")) {
            data = read_file(gc, name, &n);
        }
        wfc_log("download %s %s (%s)", gc, name, data ? "sent" : "not found");
        if (data) {
            wfc_http_head(out, "200 OK", "application/x-dsdl");
            wfcb_fmt(out, "Content-Disposition: attachment; filename=\"%s\"\r\n", name);
            wfc_http_end(out, data, n);
            if (data != l.file.p) free(data);
        } else {
            wfc_http_head(out, "404 Not Found", "text/plain");
            wfcb_str(out, "X-DLS-Host: http://127.0.0.1/\r\n");
            wfc_http_end(out, "", 0);
        }
        wfcb_free(&l.file);
        return;
    }

    WfcBuf body;
    memset(&body, 0, sizeof body);
    int count = 0;
    for (int i = 0; i < l.n; i++) {
        if (a1[0] && strcmp(l.e[i].attr1, a1)) continue;
        count++;
        if (!strcmp(action, "list")) wfcb_fmt(&body, "%s\t\t%s\t\t\t%s\r\n", l.e[i].name, l.e[i].attr1, l.e[i].size);
    }
    if (!strcmp(action, "list") && !count) wfcb_str(&body, "\r\n");
    if (!strcmp(action, "count")) wfcb_fmt(&body, "%d", count);
    wfc_log("download %s %s attr %s/%s/%s: %d file%s", gc, action, a1, a2, a3, count, count == 1 ? "" : "s");
    wfc_http_head(out, "200 OK", "text/plain");
    wfcb_str(out, "X-DLS-Host: http://127.0.0.1/\r\n");
    wfc_http_end(out, body.p, body.n);
    wfcb_free(&body);
    wfcb_free(&l.file);
}

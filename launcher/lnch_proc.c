/* The tools the launcher runs (BUILD.bat / BUILD.sh, hd-pack.exe): no window,
 * stdout and stderr read line by line without blocking the screen. See lnch.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lnch.h"

#define BUF 8192
struct LProc {
    char buf[BUF]; int len;
    int eof, code;
#ifdef _WIN32
    void *proc, *pipe;
#else
    int pid, fd;
#endif
};

/* one line out of p->buf (or what is left once the tool has finished) */
static int take_line(LProc* p, char* line, size_t n) {
    char* nl = memchr(p->buf, '\n', (size_t)p->len);
    int k;
    if (nl) k = (int)(nl - p->buf) + 1;
    else if (p->len == BUF || (p->eof && p->len)) k = p->len;   /* a line too long, or the last one */
    else return 0;
    int c = k;
    while (c && (p->buf[c - 1] == '\n' || p->buf[c - 1] == '\r')) c--;
    size_t m = (size_t)c < n ? (size_t)c : n - 1;
    memcpy(line, p->buf, m); line[m] = 0;
    memmove(p->buf, p->buf + k, (size_t)(p->len - k));
    p->len -= k;
    return 1;
}

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static wchar_t* wide(const char* s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t* w = malloc((size_t)(n > 0 ? n : 1) * sizeof *w);
    if (w) { if (n > 0) MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n); else w[0] = 0; }
    return w;
}
/* argv as one command line, quoted the way the C runtime reads it back */
static void quote_arg(char* out, size_t cap, size_t* o, const char* a) {
    int q = !*a || strpbrk(a, " \t\"&|<>^()%!,;=") != NULL;   /* cmd.exe reads these too */
    if (q && *o < cap) out[(*o)++] = '"';
    for (const char* s = a; *s; s++) {
        int bs = 0;
        while (*s == '\\') { bs++; s++; }
        int dbl = !*s && q ? 2 : (*s == '"' ? 2 : 1);
        for (int i = 0; i < bs * dbl && *o < cap; i++) out[(*o)++] = '\\';
        if (!*s) break;
        if (*s == '"' && *o < cap) out[(*o)++] = '\\';
        if (*o < cap) out[(*o)++] = *s;
    }
    if (q && *o < cap) out[(*o)++] = '"';
}

/* every tool started joins this job: when the launcher closes (even killed),
 * Windows ends them and their children (the build's bash, make, gcc) with it;
 * a build or a download cut that way starts again next time */
static HANDLE job(void) {
    static HANDLE j;
    if (!j) {
        j = CreateJobObjectW(NULL, NULL);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li; memset(&li, 0, sizeof li);
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (j) SetInformationJobObject(j, JobObjectExtendedLimitInformation, &li, sizeof li);
    }
    return j;
}

LProc* proc_start(const char* const* argv, const char* const* env_add, const char* cwd) {
    char cmd[8192]; size_t o = 0;
    for (int i = 0; argv[i]; i++) { if (i && o < sizeof cmd) cmd[o++] = ' '; quote_arg(cmd, sizeof cmd - 1, &o, argv[i]); }
    cmd[o < sizeof cmd ? o : sizeof cmd - 1] = 0;
    /* the environment: env_add first, then ours without the names env_add sets (compared without case) */
    size_t cap = 1 << 16, n = 0;
    wchar_t* env = malloc(cap * sizeof *env);
    if (!env) return NULL;
    for (int i = 0; env_add && env_add[i]; i++) {
        wchar_t* w = wide(env_add[i]);
        size_t l = w ? wcslen(w) + 1 : 0;
        while (n + l + 2 > cap) { cap *= 2; wchar_t* e2 = realloc(env, cap * sizeof *env); if (!e2) { free(env); free(w); return NULL; } env = e2; }
        if (w) { memcpy(env + n, w, l * sizeof *env); n += l; }
        free(w);
    }
    wchar_t* cur = GetEnvironmentStringsW();
    for (wchar_t* e = cur; e && *e; e += wcslen(e) + 1) {
        size_t l = wcslen(e) + 1;
        int skip = 0;
        for (int i = 0; env_add && env_add[i]; i++) {
            const char* eq = strchr(env_add[i], '=');
            size_t nl = eq ? (size_t)(eq - env_add[i]) : 0;
            if (nl && l > nl && e[nl] == L'=') {
                skip = 1;
                for (size_t k = 0; k < nl; k++) if (towupper(e[k]) != towupper((wchar_t)(unsigned char)env_add[i][k])) { skip = 0; break; }
                if (skip) break;
            }
        }
        if (skip) continue;
        while (n + l + 2 > cap) { cap *= 2; wchar_t* e2 = realloc(env, cap * sizeof *env); if (!e2) { free(env); FreeEnvironmentStringsW(cur); return NULL; } env = e2; }
        memcpy(env + n, e, l * sizeof *env); n += l;
    }
    if (cur) FreeEnvironmentStringsW(cur);
    env[n++] = 0; env[n] = 0;

    SECURITY_ATTRIBUTES sa = {sizeof sa, NULL, TRUE};
    HANDLE rd, wr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) { free(env); return NULL; }
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si; memset(&si, 0, sizeof si); si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE); si.hStdOutput = wr; si.hStdError = wr;
    PROCESS_INFORMATION pi;
    wchar_t* wcmd = wide(cmd); wchar_t* wcwd = cwd ? wide(cwd) : NULL;
    BOOL ok = wcmd && CreateProcessW(NULL, wcmd, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT | CREATE_SUSPENDED, env, wcwd, &si, &pi);
    if (ok) {
        HANDLE j = job();
        if (j) AssignProcessToJobObject(j, pi.hProcess);
        ResumeThread(pi.hThread);
    }
    free(wcmd); free(wcwd); free(env);
    CloseHandle(wr);
    if (!ok) { CloseHandle(rd); return NULL; }
    CloseHandle(pi.hThread);
    LProc* p = calloc(1, sizeof *p);
    if (!p) { CloseHandle(rd); CloseHandle(pi.hProcess); return NULL; }
    p->proc = pi.hProcess; p->pipe = rd; p->code = -1;
    return p;
}

int proc_read_line(LProc* p, char* line, size_t n) {
    if (take_line(p, line, n)) return 1;
    while (!p->eof && p->len < BUF) {
        DWORD avail = 0, got = 0;
        if (!PeekNamedPipe(p->pipe, NULL, 0, NULL, &avail, NULL)) { p->eof = 1; break; }   /* closed: the tool has finished */
        if (!avail) break;
        if (!ReadFile(p->pipe, p->buf + p->len, (DWORD)(BUF - p->len) < avail ? (DWORD)(BUF - p->len) : avail, &got, NULL) || !got) { p->eof = 1; break; }
        p->len += (int)got;
        if (memchr(p->buf, '\n', (size_t)p->len)) break;
    }
    if (take_line(p, line, n)) return 1;
    if (!p->eof) {
        if (WaitForSingleObject(p->proc, 0) == WAIT_OBJECT_0) {
            DWORD avail = 0;                            /* gone: read what is left, then say finished */
            if (!PeekNamedPipe(p->pipe, NULL, 0, NULL, &avail, NULL) || !avail) p->eof = 1;
        }
        if (!p->eof) return 0;
    }
    if (take_line(p, line, n)) return 1;
    DWORD c = (DWORD)-1;
    WaitForSingleObject(p->proc, INFINITE);
    GetExitCodeProcess(p->proc, &c);
    p->code = (int)c;
    return -1;
}

void proc_free(LProc* p) {
    if (!p) return;
    CloseHandle(p->pipe); CloseHandle(p->proc);
    free(p);
}

#else
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/wait.h>

LProc* proc_start(const char* const* argv, const char* const* env_add, const char* cwd) {
    int fd[2];
    if (pipe(fd)) return NULL;
    int pid = fork();
    if (pid < 0) { close(fd[0]); close(fd[1]); return NULL; }
    if (pid == 0) {
        dup2(fd[1], 1); dup2(fd[1], 2);
        close(fd[0]); close(fd[1]);
        if (cwd && chdir(cwd)) _exit(127);
        for (int i = 0; env_add && env_add[i]; i++) putenv((char*)env_add[i]);
        execvp(argv[0], (char* const*)argv);
        _exit(127);
    }
    close(fd[1]);
    fcntl(fd[0], F_SETFL, fcntl(fd[0], F_GETFL) | O_NONBLOCK);
    LProc* p = calloc(1, sizeof *p);
    if (!p) { close(fd[0]); return NULL; }
    p->pid = pid; p->fd = fd[0]; p->code = -1;
    return p;
}

int proc_read_line(LProc* p, char* line, size_t n) {
    if (take_line(p, line, n)) return 1;
    while (!p->eof && p->len < BUF) {
        ssize_t got = read(p->fd, p->buf + p->len, (size_t)(BUF - p->len));
        if (got > 0) { p->len += (int)got; if (memchr(p->buf, '\n', (size_t)p->len)) break; continue; }
        if (got == 0) { p->eof = 1; break; }
        if (errno == EINTR) continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
        p->eof = 1;
    }
    if (take_line(p, line, n)) return 1;
    if (!p->eof) return 0;
    int st = 0;
    while (waitpid(p->pid, &st, 0) < 0 && errno == EINTR) {}
    p->code = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
    return -1;
}

void proc_free(LProc* p) {
    if (!p) return;
    close(p->fd);
    free(p);
}
#endif

int proc_exit_code(LProc* p) { return p->code; }

int parse_build_line(const char* line, float* frac, char* title, size_t n) {
    size_t l = strlen(line);
    while (l && (line[l - 1] == '\n' || line[l - 1] == '\r')) l--;
    if (!strncmp(line, "BUILD FAILED", 12)) {
        size_t m = l < n ? l : n - 1;
        memcpy(title, line, m); title[m] = 0;
        return 2;
    }
    if (strncmp(line, "progress ", 9)) return 0;
    char* e;
    float f = strtof(line + 9, &e);
    if (e == line + 9 || (*e != ' ' && (size_t)(e - line) != l) || f < 0 || f > 1) return 0;
    while (*e == ' ') e++;
    size_t m = l - (size_t)(e - line);
    if (m >= n) m = n - 1;
    memcpy(title, e, m); title[m] = 0;
    *frac = f;
    return 1;
}

int parse_pack_status(const char* line, PackStatus* s) {
    memset(s, 0, sizeof *s);
    char word[16];
    int used = 0;
    if (sscanf(line, "%15s%n", word, &used) != 1) return 0;
    if (!strcmp(word, "download")) {
        if (sscanf(line + used, "%lld %lld %d %d", &s->done, &s->total, &s->i, &s->n) != 4) return 0;
        s->kind = 'd';
    } else if (!strcmp(word, "install")) {
        if (sscanf(line + used, "%lld %lld", &s->done, &s->total) != 2) return 0;
        s->kind = 'i';
    } else if (!strcmp(word, "uptodate")) s->kind = 'u';
    else if (!strcmp(word, "done")) s->kind = 'D';
    else if (!strcmp(word, "failed")) {
        const char* r = line + used;
        while (*r == ' ') r++;
        size_t l = strlen(r);
        while (l && (r[l - 1] == '\n' || r[l - 1] == '\r')) l--;
        if (l >= sizeof s->reason) l = sizeof s->reason - 1;
        memcpy(s->reason, r, l); s->reason[l] = 0;
        s->kind = 'f';
    } else return 0;
    if ((s->kind == 'd' || s->kind == 'i') && (s->done < 0 || s->total < 0 || s->done > s->total)) return 0;
    return 1;
}

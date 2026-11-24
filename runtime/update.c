/* Updates from the project's GitHub repository: see update.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "update.h"

/* "v27" -> 27, 0, 9999; "v31.2" -> 31, 2, 9999; "v27-nightly9" -> 27, 0, 9.
 * Since v2.0 the numbers start again ("v2.0", "v2.1", "v3"...): a major
 * under 26 (older than any release before it) is one of those, after v47. */
static long long version_key(const char* v) {
    int major = 0, minor = 0, nightly = 9999;
    if (!v || (v[0] != 'v' && v[0] != 'V')) return -1;
    char* e;
    major = (int)strtol(v + 1, &e, 10);
    if (major < 26) major += 1000;
    if (*e == '.') minor = atoi(e + 1);
    const char* n = strstr(v, "nightly");
    if (n) nightly = atoi(n + 7);
    return ((long long)major * 1000 + minor) * 100000 + nightly;
}
int update_newer(const char* a, const char* b) { return version_key(a) > version_key(b); }

#ifdef _WIN32
/* the DQIX_VERSION line of a version.h: the one at a line's start (the one
 * in its comment is for the updaters of v47 and older, see version.h) */
static int parse_version(const char* text, char* out, size_t n) {
    const char* p = text ? strstr(text, "\n#define DQIX_VERSION \"") : NULL;
    if (!p) return 0;
    p += strlen("\n#define DQIX_VERSION \"");
    size_t i = 0;
    while (p[i] && p[i] != '"' && i + 1 < n) {
        char c = p[i];                                   /* it ends up in a command line: letters, digits, . _ - only */
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return 0;
        out[i] = c; i++;
    }
    out[i] = 0;
    return i > 1 && out[0] == 'v';
}

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>

/* GET https://host/path into memory (at most `max` bytes), or, with `f`,
 * into that file (on_bytes told how far it is: done, total or -1) */
static int https_fetch(const wchar_t* host, const wchar_t* path, char** out, size_t* len, FILE* f,
                       void (*on_bytes)(long long, long long), size_t max, char* err, size_t en) {
    if (out) { *out = NULL; *len = 0; }
    int ok = 0;
    HINTERNET s = WinHttpOpen(L"dqix-enhanced-updater", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET c = NULL, r = NULL;
    if (!s) { snprintf(err, en, "no internet access (WinHTTP %lu)", GetLastError()); goto done; }
    WinHttpSetTimeouts(s, 8000, 8000, 15000, 60000);
    c = WinHttpConnect(s, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!c) { snprintf(err, en, "can't reach GitHub (%lu)", GetLastError()); goto done; }
    r = WinHttpOpenRequest(c, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!r || !WinHttpSendRequest(r, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(r, NULL)) {
        snprintf(err, en, "can't reach GitHub (are you online? error %lu)", GetLastError()); goto done;
    }
    DWORD code = 0, sz = sizeof code;
    WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz, WINHTTP_NO_HEADER_INDEX);
    if (code != 200) { snprintf(err, en, code == 404 ? "not found on GitHub (is the repository public?)" : "GitHub answered %lu", code); goto done; }
    DWORD clen = 0; sz = sizeof clen;
    long long total = WinHttpQueryHeaders(r, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &clen, &sz, WINHTTP_NO_HEADER_INDEX) ? (long long)clen : -1;
    size_t cap = 1 << 16, n = 0;
    char* b = malloc(cap + 1);
    if (!b) { snprintf(err, en, "out of memory"); goto done; }
    for (;;) {
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(r, &avail)) { snprintf(err, en, "the download broke off (%lu)", GetLastError()); free(b); goto done; }
        if (!avail) break;
        if (n + avail > max) { snprintf(err, en, "the download is too big"); free(b); goto done; }
        size_t at = f ? 0 : n;                       /* a file: the buffer is reused */
        if (at + avail > cap) { while (at + avail > cap) cap *= 2; char* b2 = realloc(b, cap + 1); if (!b2) { snprintf(err, en, "out of memory"); free(b); goto done; } b = b2; }
        DWORD got = 0;
        if (!WinHttpReadData(r, b + at, avail, &got)) { snprintf(err, en, "the download broke off (%lu)", GetLastError()); free(b); goto done; }
        if (f && fwrite(b, 1, got, f) != got) { snprintf(err, en, "couldn't write the download (disk full?)"); free(b); goto done; }
        n += got;
        if (on_bytes) on_bytes((long long)n, total);
    }
    if (total >= 0 && (long long)n != total) { snprintf(err, en, "the download broke off (%lld of %lld bytes)", (long long)n, total); free(b); goto done; }
    if (f) { free(b); ok = 1; goto done; }
    b[n] = 0;
    *out = b; *len = n; ok = 1;
done:
    if (r) WinHttpCloseHandle(r);
    if (c) WinHttpCloseHandle(c);
    if (s) WinHttpCloseHandle(s);
    return ok;
}
static int https_get(const wchar_t* host, const wchar_t* path, char** out, size_t* len, size_t max, char* err, size_t en) {
    return https_fetch(host, path, out, len, NULL, NULL, max, err, en);
}

static void read_version(const char* branch, char* out, size_t n, char* err, size_t en) {
    wchar_t path[256];
    swprintf(path, 256, L"/%hs/%hs/runtime/version.h", UPDATE_REPO, branch);
    char* t; size_t len;
    out[0] = 0;
    const char* src = getenv("DQIX_UPDATE_SOURCE");       /* tests: <folder>/<branch>/runtime/version.h */
    if (src && *src) {
        char p[1024]; snprintf(p, sizeof p, "%s/%s/runtime/version.h", src, branch);
        FILE* f = fopen(p, "rb");
        if (!f) { snprintf(err, en, "can't reach GitHub (test source)"); return; }
        t = calloc(1, 1 << 16); len = t ? fread(t, 1, (1 << 16) - 1, f) : 0; fclose(f);
        if (t && !parse_version(t, out, n)) snprintf(err, en, "the %s version couldn't be read", branch);
        free(t);
        return;
    }
    if (!https_get(L"raw.githubusercontent.com", path, &t, &len, 1 << 16, err, en)) return;
    if (!parse_version(t, out, n)) snprintf(err, en, "the %s version on GitHub couldn't be read", branch);
    free(t);
}

void update_check_now(UpdateInfo* o) {
    memset(o, 0, sizeof *o);
    read_version("stable", o->stable, sizeof o->stable, o->error, sizeof o->error);
    if (!o->error[0]) read_version("main", o->nightly, sizeof o->nightly, o->error, sizeof o->error);
    o->ok = o->stable[0] && o->nightly[0] && !o->error[0];
}

static UpdateInfo g_async;
static volatile LONG g_async_state;            /* 0 idle, 1 running, 2 done */
static DWORD WINAPI check_thread(LPVOID u) { (void)u; update_check_now(&g_async); InterlockedExchange(&g_async_state, 2); return 0; }
void update_check_async(void) {
    if (InterlockedCompareExchange(&g_async_state, 1, 0) != 0) return;
    HANDLE h = CreateThread(NULL, 0, check_thread, NULL, 0, NULL);
    if (h) CloseHandle(h); else InterlockedExchange(&g_async_state, 0);
}
int update_poll(UpdateInfo* out) {
    if (g_async_state != 2) return 0;
    *out = g_async;
    InterlockedExchange(&g_async_state, 3);       /* reported once */
    return 1;
}

/* the project folder: one up from the game's, with BUILD.bat in it */
static wchar_t g_root[MAX_PATH];
static int exists_w(const wchar_t* dir, const wchar_t* name) {
    wchar_t p[MAX_PATH + 64]; swprintf(p, MAX_PATH + 64, L"%ls\\%ls", dir, name);
    return GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES;
}
static const wchar_t* project_dir_w(void) {
    if (g_root[0]) return g_root;
    wchar_t exe[MAX_PATH];
    DWORD l = GetModuleFileNameW(NULL, exe, MAX_PATH);
    if (!l || l >= MAX_PATH) return NULL;
    for (int up = 0; up < 2; up++) { wchar_t* s = wcsrchr(exe, L'\\'); if (!s) return NULL; *s = 0; }   /* ...\game\dqix.exe -> ... */
    if (!exists_w(exe, L"BUILD.bat") || !exists_w(exe, L"tools\\build-game.sh")) return NULL;
    wcscpy(g_root, exe);
    return g_root;
}
const char* update_project_dir(void) {
    static char a[MAX_PATH * 3];
    const wchar_t* w = project_dir_w();
    if (!w) return NULL;
    WideCharToMultiByte(CP_UTF8, 0, w, -1, a, sizeof a, NULL, NULL);
    return a;
}

/* The updater (update\apply.bat), run after the game closes: arguments are
 * the game's process id and the version. No paths inside: it works from
 * the folder above its own. */
static const char APPLY_BAT[] =
    "@echo off\r\n"
    "rem Written by the game to install an update. Safe to delete.\r\n"
    "setlocal\r\n"
    "rem some programs start others with NoDefaultCurrentDirectoryInExePath set: commands are then not\r\n"
    "rem looked for in this folder, so everything here is called with its path\r\n"
    "set NoDefaultCurrentDirectoryInExePath=\r\n"
    "cd /d \"%~dp0..\"\r\n"
    "title Dragon Quest IX - updating to %2\r\n"
    "echo Updating Dragon Quest IX to %2.\r\n"
    "echo Waiting for the game to close...\r\n"
    "powershell -NoProfile -Command \"try { Wait-Process -Id %1 -Timeout 120 -ErrorAction Stop } catch {}\"\r\n"
    "echo.\r\n"
    "echo == Keeping a copy of the current version in update\\previous\r\n"
    "if exist update\\previous rmdir /s /q update\\previous\r\n"
    "\"%SystemRoot%\\System32\\robocopy.exe\" . update\\previous /E /XJ /NFL /NDL /NJH /NJS /NP /XD game build rom extras update bookmarks reports release .git >nul\r\n"
    "if errorlevel 8 goto fail\r\n"
    "echo == Unpacking the new version\r\n"
    "if exist update\\new rmdir /s /q update\\new\r\n"
    "mkdir update\\new\r\n"
    "\"%SystemRoot%\\System32\\tar.exe\" -xf update\\download.zip -C update\\new\r\n"
    "if errorlevel 1 goto fail\r\n"
    "set \"SRC=\"\r\n"
    "for /d %%d in (update\\new\\*) do if not defined SRC set \"SRC=%%d\"\r\n"
    "if not defined SRC goto fail\r\n"
    "if not exist \"%SRC%\\BUILD.bat\" goto fail\r\n"
    "\"%SystemRoot%\\System32\\robocopy.exe\" \"%SRC%\" . /E /XJ /NFL /NDL /NJH /NJS /NP /XD game build rom update .git >nul\r\n"
    "if errorlevel 8 goto fail\r\n"
    "rmdir /s /q update\\new\r\n"
    "echo == Building the game again from your ROM (this can take a while)\r\n"
    "set DQIX_NOPAUSE=1\r\n"
    "call .\\BUILD.bat\r\n"
    "if errorlevel 1 goto buildfail\r\n"
    "del update\\download.zip >nul 2>&1\r\n"
    "echo.\r\n"
    "echo Done: Dragon Quest IX %2. Starting the game...\r\n"
    "start \"\" \"game\\dqix.exe\"\r\n"
    "exit /b 0\r\n"
    ":fail\r\n"
    "echo.\r\n"
    "echo The update could not be installed. Nothing was changed in your game folder;\r\n"
    "echo update\\rollback.bat puts the project files back as they were if needed.\r\n"
    "pause\r\n"
    "exit /b 1\r\n"
    ":buildfail\r\n"
    "echo.\r\n"
    "echo The new version did not build (the text above says why). Your saves are safe.\r\n"
    "echo To go back: double-click update\\rollback.bat (it restores the previous version and builds it).\r\n"
    "pause\r\n"
    "exit /b 1\r\n";

static const char ROLLBACK_BAT[] =
    "@echo off\r\n"
    "rem Puts back the version from before the last update and builds it.\r\n"
    "setlocal\r\n"
    "rem some programs start others with NoDefaultCurrentDirectoryInExePath set: commands are then not\r\n"
    "rem looked for in this folder, so everything here is called with its path\r\n"
    "set NoDefaultCurrentDirectoryInExePath=\r\n"
    "cd /d \"%~dp0..\"\r\n"
    "if not exist update\\previous\\BUILD.bat ( echo There is no previous version here. & pause & exit /b 1 )\r\n"
    "\"%SystemRoot%\\System32\\robocopy.exe\" update\\previous . /E /XJ /NFL /NDL /NJH /NJS /NP >nul\r\n"
    "if errorlevel 8 ( echo Copying back failed. & pause & exit /b 1 )\r\n"
    "set DQIX_NOPAUSE=1\r\n"
    "call .\\BUILD.bat\r\n"
    "if errorlevel 1 ( pause & exit /b 1 )\r\n"
    "start \"\" \"game\\dqix.exe\"\r\n";

static int write_file_w(const wchar_t* path, const void* d, size_t n) {
    FILE* f = _wfopen(path, L"wb");
    if (!f) return 0;
    int ok = fwrite(d, 1, n, f) == n;
    return fclose(f) == 0 && ok;
}

int update_https_get(const char* host, const char* path, char** out, size_t* len, size_t max, char* err, size_t en) {
    wchar_t h[128], p[512];
    if (!MultiByteToWideChar(CP_UTF8, 0, host, -1, h, 128) || !MultiByteToWideChar(CP_UTF8, 0, path, -1, p, 512)) { snprintf(err, en, "bad address"); return 0; }
    return https_get(h, p, out, len, max, err, en);
}

int update_download(const char* branch, const char* zip_path, void (*on_bytes)(long long done, long long total), char* err, size_t en) {
    const char* src = getenv("DQIX_UPDATE_SOURCE");       /* tests: <folder>/<branch>.zip */
    /* into <zip>.part, renamed when whole and checked: a cut download (launcher
     * closed, power cut) never leaves a zip the launcher would take as an update */
    char part[MAX_PATH + 8];
    if (snprintf(part, sizeof part, "%s.part", zip_path) >= (int)sizeof part) { snprintf(err, en, "path too long"); return 0; }
    FILE* f = fopen(part, "wb");
    if (!f) { snprintf(err, en, "couldn't write to the update folder"); return 0; }
    int ok;
    if (src && *src) {
        char p[1024]; snprintf(p, sizeof p, "%s/%s.zip", src, branch);
        FILE* in = fopen(p, "rb");
        ok = in != NULL;
        if (!in) snprintf(err, en, "can't reach GitHub (test source)");
        char b[65536]; size_t got; long long n = 0;
        while (in && (got = fread(b, 1, sizeof b, in)) > 0) {
            if (fwrite(b, 1, got, f) != got) { snprintf(err, en, "couldn't write the download (disk full?)"); ok = 0; break; }
            n += (long long)got; if (on_bytes) on_bytes(n, -1);
        }
        if (in) fclose(in);
    } else {
        wchar_t path[256];
        swprintf(path, 256, L"/%hs/zip/refs/heads/%hs", UPDATE_REPO, branch);
        ok = https_fetch(L"codeload.github.com", path, NULL, NULL, f, on_bytes, 256u << 20, err, en);
    }
    if (fclose(f) != 0 && ok) { snprintf(err, en, "couldn't write the download (disk full?)"); ok = 0; }
    if (ok) {                                             /* a zip, or nothing */
        FILE* z = fopen(part, "rb"); char pk[2] = {0, 0};
        if (!z || fread(pk, 1, 2, z) != 2 || memcmp(pk, "PK", 2)) { snprintf(err, en, "GitHub didn't send a zip file"); ok = 0; }
        if (z) fclose(z);
    }
    if (ok && !MoveFileExA(part, zip_path, MOVEFILE_REPLACE_EXISTING)) { snprintf(err, en, "couldn't write to the update folder (%lu)", GetLastError()); ok = 0; }
    if (!ok) remove(part);
    return ok;
}

const char* update_rollback_bat(void) { return ROLLBACK_BAT; }

int update_install(const char* branch, const char* version, char* err, size_t en) {
    const wchar_t* root = project_dir_w();
    if (!root) { snprintf(err, en, "This copy wasn't made with BUILD.bat, so it can't update itself. Download the new version instead."); return 0; }
    wchar_t dir[MAX_PATH + 32], p[MAX_PATH + 64];
    swprintf(dir, MAX_PATH + 32, L"%ls\\update", root);
    CreateDirectoryW(dir, NULL);
    wchar_t path[256];
    swprintf(path, 256, L"/%hs/zip/refs/heads/%hs", UPDATE_REPO, branch);
    char* zip; size_t len;
    if (!https_get(L"codeload.github.com", path, &zip, &len, 256u << 20, err, en)) return 0;
    if (len < 4 || memcmp(zip, "PK", 2)) { free(zip); snprintf(err, en, "GitHub didn't send a zip file"); return 0; }
    swprintf(p, MAX_PATH + 64, L"%ls\\download.zip", dir);
    int ok = write_file_w(p, zip, len);
    free(zip);
    swprintf(p, MAX_PATH + 64, L"%ls\\apply.bat", dir);
    ok = ok && write_file_w(p, APPLY_BAT, sizeof APPLY_BAT - 1);
    swprintf(p, MAX_PATH + 64, L"%ls\\rollback.bat", dir);
    ok = ok && write_file_w(p, ROLLBACK_BAT, sizeof ROLLBACK_BAT - 1);
    if (!ok) { snprintf(err, en, "couldn't write to the update folder"); return 0; }
    wchar_t cmd[MAX_PATH + 160];
    swprintf(cmd, MAX_PATH + 160, L"cmd.exe /c \"\"%ls\\apply.bat\" %lu %hs\"", dir, (unsigned long)GetCurrentProcessId(), version && version[0] ? version : branch);
    STARTUPINFOW si; memset(&si, 0, sizeof si); si.cb = sizeof si;
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, root, &si, &pi)) {
        snprintf(err, en, "couldn't start the updater (%lu)", GetLastError()); return 0;
    }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return 1;
}

#else  /* elsewhere: update by hand (git pull, or a new download) */
void update_check_now(UpdateInfo* o) { memset(o, 0, sizeof *o); snprintf(o->error, sizeof o->error, "updating from the game works on Windows"); }
void update_check_async(void) {}
int update_poll(UpdateInfo* out) { (void)out; return 0; }
const char* update_project_dir(void) { return NULL; }
int update_install(const char* branch, const char* version, char* err, size_t n) { (void)branch; (void)version; snprintf(err, n, "updating from the game works on Windows"); return 0; }
int update_https_get(const char* host, const char* path, char** out, size_t* len, size_t max, char* err, size_t n) { (void)host; (void)path; (void)max; *out = NULL; *len = 0; snprintf(err, n, "Windows only"); return 0; }
int update_download(const char* branch, const char* zip_path, void (*on_bytes)(long long, long long), char* err, size_t n) { (void)branch; (void)zip_path; (void)on_bytes; snprintf(err, n, "updating from the game works on Windows"); return 0; }
const char* update_rollback_bat(void) { return ""; }
#endif

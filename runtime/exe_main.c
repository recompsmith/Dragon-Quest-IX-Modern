/* The program's entry: the game itself is dqix_main (main.c), so the same
 * code can also run as a library (hosting.c).
 *
 * Started with no arguments (dqix.exe double-clicked, or a shortcut to it), it
 * hands over to the launcher next to it (hand_over_to_launcher), or plays the
 * game in its own folder like play.bat does. On Windows the console
 * window that would open with it is closed, and what the game prints goes to
 * last-run.log instead. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#endif

int dqix_main(int argc, char** argv);

#ifdef _WIN32
/* Laptops with two graphics chips (Intel + NVIDIA or AMD): the drivers start
 * an OpenGL program on the slow built-in one unless the .exe itself asks for
 * the fast one with these exported values (the NVIDIA panel's choice alone
 * didn't move it: 3D at 40 FPS on Intel graphics). */
__declspec(dllexport) DWORD NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
#endif

static void to_own_folder(const char* argv0) {
#ifdef _WIN32
    wchar_t wd[1024];                                  /* wide: a folder with any letters in its name */
    DWORD wn = GetModuleFileNameW(NULL, wd, 1024);
    if (wn == 0 || wn >= 1024) return;
    wchar_t* ws = wcsrchr(wd, L'\\');
    if (ws) { *ws = 0; SetCurrentDirectoryW(wd); }
    (void)argv0;
#else
    char dir[1024];
    snprintf(dir, sizeof dir, "%s", argv0);
    char* s = strrchr(dir, '/');
    if (s) { *s = 0; chdir(dir); }
#endif
}

/* Started with no arguments (a shortcut, RecompLauncher, an old version's
 * updater) next to the launcher: the launcher brings the game and its HD
 * pack up to date, then starts us again with DQIX_LAUNCHER set. 1 if it was
 * started (we then quit); 0 if there is none, it didn't start, or it ended
 * with an error within 3 s (a broken launcher never leaves no game): play. */
static int hand_over_to_launcher(void) {
    const char* e = getenv("DQIX_LAUNCHER");
    if (e && *e) return 0;
#ifdef _WIN32
    if (GetFileAttributesA("dqix-launcher.exe") == INVALID_FILE_ATTRIBUTES) return 0;
    char cmd[] = "\"dqix-launcher.exe\"";
    STARTUPINFOA si; memset(&si, 0, sizeof si); si.cb = sizeof si;
    PROCESS_INFORMATION pi;
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return 0;
    CloseHandle(pi.hThread);
    DWORD code = 0;
    int died = WaitForSingleObject(pi.hProcess, 3000) == WAIT_OBJECT_0 && GetExitCodeProcess(pi.hProcess, &code) && code != 0;
    CloseHandle(pi.hProcess);
    return !died;
#else
    if (access("dqix-launcher", X_OK) != 0) return 0;
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) { setsid(); execl("./dqix-launcher", "./dqix-launcher", (char*)NULL); _exit(127); }
    for (int i = 0; i < 30; i++) {                      /* 3 s: an exec that failed, or a launcher that died at once */
        int st = 0;
        pid_t r = waitpid(pid, &st, WNOHANG);
        if (r == pid) return !(WIFEXITED(st) && WEXITSTATUS(st) != 0) && !WIFSIGNALED(st);
        if (r < 0) break;
        usleep(100000);
    }
    return 1;
#endif
}

int main(int argc, char** argv) {
    if (argc == 1) {
        to_own_folder(argv[0]);
        if (hand_over_to_launcher()) return 0;
        FILE* f = fopen("data/dqix.nds", "rb");
        if (f) {
            fclose(f);
#ifdef _WIN32
            DWORD procs[2];
            if (GetConsoleProcessList(procs, 2) <= 1) {      /* a console of our own (not cmd's), or none */
                FreeConsole();
                freopen("last-run.log", "w", stdout);
                freopen("last-run.log", "a", stderr);
                setvbuf(stderr, NULL, _IONBF, 0);
            }
#endif
            static char* def[] = {NULL, "data/dqix.nds", "data/code", "0", "1", "dqix.sav", NULL};
            def[0] = argv[0];
            return dqix_main(6, def);
        }
    }
    return dqix_main(argc, argv);
}

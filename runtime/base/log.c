/* Log lines and fatal errors. Each line carries the frame, the video line
 * and the CPU it comes from; past LOG_FRAME_CAP lines in one frame the rest
 * is counted and a single notice says how many were not shown. Every shown
 * line is also handed to the crash report (report_log_line). */
#include <stdarg.h>
#include "base.h"

int g_log_level = 1;
int g_log_cpu = 9;

static uint64_t g_log_frame = ~0ull;   /* frame the counters below belong to */
static unsigned g_log_shown, g_log_hidden;

void log_flush_frame(void) {
    if (g_log_hidden) {
        char line[96];
        snprintf(line, sizeof line, "[f%llu] ... %u more log lines in that frame not shown\n",
                 (unsigned long long)g_log_frame, g_log_hidden);
        fputs(line, stderr);
        report_log_line(line);
    }
    g_log_hidden = 0;
    g_log_shown = 0;
}

static void emit(const char* body) {
    char line[1200];
    snprintf(line, sizeof line, "[f%llu l%u %d] %s", (unsigned long long)g_frame, (unsigned)g_vcount, g_log_cpu, body);
    size_t n = strlen(line);
    if (n == 0 || line[n - 1] != '\n') { if (n + 1 < sizeof line) { line[n] = '\n'; line[n + 1] = 0; } }
    fputs(line, stderr);
    fflush(stderr);
    report_log_line(line);
}

void rt_log(int level, const char* fmt, ...) {
    if (level > g_log_level) return;
    if (g_frame != g_log_frame) { log_flush_frame(); g_log_frame = g_frame; }
    if (g_log_shown >= LOG_FRAME_CAP) { g_log_hidden++; return; }
    g_log_shown++;
    char body[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof body, fmt, ap);
    va_end(ap);
    emit(body);
}

_Noreturn void rt_fatal(const char* fmt, ...) {
    char msg[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    size_t n = strlen(msg);
    while (n && msg[n - 1] == '\n') msg[--n] = 0;
    log_flush_frame();
    fprintf(stderr, "FATAL [frame %llu, ARM%d]: %s\n", (unsigned long long)g_frame, g_log_cpu, msg);
    fflush(stderr);
    char reason[1100];
    snprintf(reason, sizeof reason, "crash: %s", msg);
    report_write(reason);
    rt_exit(1);
}

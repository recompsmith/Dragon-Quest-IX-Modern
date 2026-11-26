/* Coroutines on ucontext (POSIX) or fibers (Windows). See coro.h. */
#include <stdlib.h>
#include <string.h>
#include "coro.h"
#include "base.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <ucontext.h>
#endif

struct Coro {
#ifdef _WIN32
    LPVOID fiber;
#else
    ucontext_t ctx;
    void* stack;
#endif
    void (*fn)(void*);
    void* arg;
    Coro* creator;
    int is_main, finished;
};

static Coro* g_main;
static Coro* g_cur;

static Coro* coro_alloc(void) {
    Coro* c = (Coro*)calloc(1, sizeof *c);
    if (!c) rt_fatal("out of memory (coroutine)");
    return c;
}

Coro* coro_main(void) {
    if (g_main) return g_main;
    g_main = coro_alloc();
    g_main->is_main = 1;
#ifdef _WIN32
    /* a host program may already have made this thread a fiber: never convert twice */
    g_main->fiber = IsThreadAFiber() ? GetCurrentFiber() : ConvertThreadToFiber(NULL);
    if (!g_main->fiber) rt_fatal("cannot make the main thread a fiber");
#endif
    /* POSIX: the context is filled by the first swapcontext away from it */
    g_cur = g_main;
    return g_main;
}

Coro* coro_current(void) { return g_cur ? g_cur : coro_main(); }
int coro_finished(Coro* c) { return c->finished; }

static void coro_body(Coro* self) {
    self->fn(self->arg);
    self->finished = 1;
    for (;;) coro_switch(self->creator ? self->creator : g_main);   /* never back into a dead stack */
}

#ifdef _WIN32
static VOID CALLBACK coro_entry(LPVOID p) { coro_body((Coro*)p); }
#else
static void coro_entry(void) { coro_body(g_cur); }   /* g_cur is the new coroutine at its first switch */
#endif

#ifndef _WIN32
/* getcontext returns twice: kept apart so no local of coro_new is live across it */
static void posix_make(Coro* c, size_t stack) {
    c->stack = malloc(stack);              /* large blocks are mapped lazily by the C library */
    if (!c->stack || getcontext(&c->ctx) != 0) rt_fatal("cannot create a coroutine (%u KiB stack)", (unsigned)(stack >> 10));
    c->ctx.uc_stack.ss_sp = c->stack;
    c->ctx.uc_stack.ss_size = stack;
    c->ctx.uc_link = NULL;
    makecontext(&c->ctx, coro_entry, 0);
}
#endif

Coro* coro_new(void (*fn)(void*), void* arg, size_t stack) {
    Coro* from = coro_current();
    Coro* c = coro_alloc();
    c->fn = fn; c->arg = arg; c->creator = from;
    if (stack < 64 * 1024) stack = 64 * 1024;
#ifdef _WIN32
    /* reserve the whole stack, commit as it grows */
    c->fiber = CreateFiberEx(64 * 1024, stack, FIBER_FLAG_FLOAT_SWITCH, coro_entry, c);
    if (!c->fiber) rt_fatal("cannot create a fiber (%u KiB stack)", (unsigned)(stack >> 10));
#else
    posix_make(c, stack);
#endif
    return c;
}

void coro_switch(Coro* to) {
    Coro* from = coro_current();
    if (to == from) return;
    g_cur = to;
#ifdef _WIN32
    SwitchToFiber(to->fiber);
#else
    swapcontext(&from->ctx, &to->ctx);
#endif
}

void coro_free(Coro* c) {
    if (!c || c == g_cur || c->is_main) return;
#ifdef _WIN32
    DeleteFiber(c->fiber);
#else
    free(c->stack);
#endif
    free(c);
}

void coro_reset_for_tests(void) {
    if (g_main && g_cur == g_main) { free(g_main); g_main = NULL; g_cur = NULL; }
}

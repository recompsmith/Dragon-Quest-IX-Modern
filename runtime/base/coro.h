/* Coroutines: separate C stacks that hand control to one another
 * explicitly. The game's threads and the whole ARM7 run on them.
 * Linux and other POSIX systems: ucontext (getcontext / makecontext /
 * swapcontext). Windows: fibers (ConvertThreadToFiber, CreateFiberEx,
 * SwitchToFiber). Single-threaded; switching order is the caller's. */
#pragma once
#include <stddef.h>

typedef struct Coro Coro;

/* The coroutine we are running in when the console starts: the thread's
 * own stack, or the fiber / context a host program started us in. */
Coro* coro_main(void);
/* A new coroutine that will run fn(arg) on a stack of `stack` bytes, from
 * its first coro_switch. If fn returns, control goes back to the coroutine
 * that created it and the new one is finished (coro_finished). */
Coro* coro_new(void (*fn)(void*), void* arg, size_t stack);
void coro_switch(Coro* to);
Coro* coro_current(void);
int coro_finished(Coro* c);
/* frees a coroutine that is not running */
void coro_free(Coro* c);
/* forget coro_main (tests: a new host context follows) */
void coro_reset_for_tests(void);

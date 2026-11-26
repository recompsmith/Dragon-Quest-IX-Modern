/* Built-in Nintendo Wi-Fi Connection server: the five entry points of
 * runtime/nds.h. The DS's Ethernet frames come in through wfc_input (before
 * libslirp); answers go back through net_deliver. */
#include "nds.h"

static int g_on;

void wfc_enable(int on, const char* dir) {
    (void)dir;
    g_on = on != 0;
}

int wfc_enabled(void) { return g_on; }

void wfc_frame(void) {}

void wfc_poll(void) {}

int wfc_input(const uint8_t* frame, int len) {
    (void)frame; (void)len;
    return 0;
}

/* Built-in server: "no SSL". The game reaches the login and download servers
 * at https:// addresses (the DS's own SSL); the built-in server only speaks
 * HTTP and refuses port 443. Every 8 frames this pass looks through the
 * overlay area of main RAM (0x02200000-0x022FFFFF, where the game loads the
 * overlay holding these addresses) and turns each NUL-terminated string that
 * starts with "https://" and names nintendowifi.net or /download into the
 * same address with "http://": one byte shorter, the freed byte set to 0. */
#include "wfc_ip.h"

#define ZONE_START 0x02200000u
#define ZONE_END   0x02300000u            /* exclusive */
#define RAM_BASE   0x02000000u

static int contains(const uint8_t* p, size_t n, const char* s) {
    size_t k = strlen(s);
    for (size_t i = 0; i + k <= n; i++)
        if (!memcmp(p + i, s, k)) return 1;
    return 0;
}

void wfc_nossl(void) {
    uint8_t* z = g_main_ram + (ZONE_START - RAM_BASE);
    size_t n = ZONE_END - ZONE_START;
    for (size_t i = 0; i + 8 <= n; i++) {
        if (z[i] != 'h' || memcmp(z + i, "https://", 8)) continue;
        const uint8_t* end = memchr(z + i, 0, n - i);
        if (!end) break;                                   /* runs past the zone: left alone */
        size_t len = (size_t)(end - (z + i));
        if (!contains(z + i, len, "nintendowifi.net") && !contains(z + i, len, "/download")) continue;
        memmove(z + i + 4, z + i + 5, len - 5);            /* drop the 's' */
        z[i + len - 1] = 0;
        wfc_log("%08x now %s (built-in server: no SSL)", (unsigned)(ZONE_START + i), (const char*)(z + i));
        i += 3;
    }
}

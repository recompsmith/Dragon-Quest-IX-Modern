#include "text.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Hide system fonts so an installed desktop font cannot mask a missing
 * bundled-font fallback on the machine running the test. */
FILE* __real_fopen(const char* path, const char* mode);
FILE* __wrap_fopen(const char* path, const char* mode) {
    return path[0] == '/' ? NULL : __real_fopen(path, mode);
}

int main(int argc, char** argv) {
    assert(argc == 2);
    int expected = atoi(argv[1]);
    uint32_t pixels[640 * 80] = {0};
    assert(text_ready() == expected);
    int width = text_draw(pixels, 640, 80, 8, 4, 24, 0xFFFFFF,
                          "Journal - Caractéristiques - Équipement");
    int ink = 0;
    for (int i = 0; i < 640 * 80; i++) ink += pixels[i] != 0;
    assert((width > 0) == expected);
    assert((ink > 0) == expected);
    return 0;
}

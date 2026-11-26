}

/* the game's letters: their capitals' rows (from its H: top, baseline under
 * the last); at this size, DS pixels -> pixels (the capitals as tall as the
 * font's) */
static int game_caps(int* top, int* base) {
    uint16_t rows[12]; int w;
    *top = 3; *base = 10;
    if (!hdtext_game_letter('H', rows, &w)) return 0;
    int t = -1, b = -1;
    for (int y = 0; y < 12; y++) if (rows[y]) { if (t < 0) t = y; b = y; }
    if (t >= 0) { *top = t; *base = b + 1; }
    return 1;
}
static float pixel_k(float px, float scale) {
    int x0, y0, x1, y1, top, base;
    game_caps(&top, &base);
    if (g_state > 0 && stbtt_GetCodepointBox(&g_font, 'H', &x0, &y0, &x1, &y1) && y1 > y0) return (float)(y1 - y0) * scale / (float)(base - top);
    return px / 13.0f;
}

static int measure(float px, const char* s, const char* end) {
    float scale = stbtt_ScaleForPixelHeight(&g_font, px), k = pixel_k(px, scale);
    float x = 0; int prev = 0, prev_game = 0;
    while (s < end && *s) {
        int c = utf8_next(&s);
        uint16_t rows[12]; int gw;
        if (g_pixel && c == ' ' && hdtext_game_letter('H', rows, &gw)) { x += 4 * k; prev = c; prev_game = 0; continue; }
        if (g_pixel && hdtext_game_letter(c, rows, &gw)) {
            if (prev_game) x += k * (float)hdtext_game_kern(prev, c);
            x += k * (float)(gw + 1); prev = c; prev_game = 1;
            continue;
        }
        int adv, lsb; stbtt_GetCodepointHMetrics(&g_font, c, &adv, &lsb);
        if (prev && !prev_game) x += scale * (float)stbtt_GetCodepointKernAdvance(&g_font, prev, c);
        x += scale * (float)adv; prev = c; prev_game = 0;
    }
    return (int)(x + 0.5f);
}

static void blend(uint32_t* buf, int w, int h, int X, int Y, uint32_t color, unsigned a) {
    if (X < 0 || Y < 0 || X >= w || Y >= h || !a) return;
    if (a > 255) a = 255;
    uint32_t d = buf[Y * w + X];
    uint32_t r = (((color >> 16) & 255) * a + ((d >> 16) & 255) * (255 - a)) / 255;
    uint32_t g = (((color >> 8) & 255) * a + ((d >> 8) & 255) * (255 - a)) / 255;
    uint32_t b = ((color & 255) * a + (d & 255) * (255 - a)) / 255;
    buf[Y * w + X] = r << 16 | g << 8 | b;
}

/* one of the game's letters, its pixels k x k (their edges' pixels in part) */
static void draw_game(uint32_t* buf, int w, int h, float x, float top, float k, uint32_t color, const uint16_t rows[12], int gw) {
    int X0 = (int)floorf(x), Y0 = (int)floorf(top), X1 = (int)ceilf(x + gw * k), Y1 = (int)ceilf(top + 12 * k);
    for (int Y = Y0; Y < Y1; Y++) {
        if (Y < 0 || Y >= h) continue;
        float fy0 = ((float)Y - top) / k, fy1 = ((float)Y + 1 - top) / k;
        for (int X = X0; X < X1; X++) {
            if (X < 0 || X >= w) continue;
            float fx0 = ((float)X - x) / k, fx1 = ((float)X + 1 - x) / k, cov = 0;
            for (int gy = (int)floorf(fy0); gy <= (int)floorf(fy1) && gy < 12; gy++) {
                if (gy < 0) continue;
                float oy = fminf(fy1, (float)gy + 1) - fmaxf(fy0, (float)gy);
                if (oy <= 0) continue;
                for (int gx = (int)floorf(fx0); gx <= (int)floorf(fx1) && gx < gw; gx++) {
                    if (gx < 0 || !((rows[gy] >> gx) & 1)) continue;
                    float ox = fminf(fx1, (float)gx + 1) - fmaxf(fx0, (float)gx);
                    if (ox > 0) cov += ox * oy;
                }
            }
            cov *= k * k;                                   /* (the part of the pixel covered) */
            if (cov > 0.02f) blend(buf, w, h, X, Y, color, (unsigned)(fminf(cov, 1) * 255 + 0.5f));
        }
    }
}

static int draw_n(uint32_t* buf, int w, int h, int x0, int y0, float px, uint32_t color, const char* s, const char* end) {
    if (!load()) return 0;
    float scale = stbtt_ScaleForPixelHeight(&g_font, px), k = pixel_k(px, scale);
    int asc, desc, gap; stbtt_GetFontVMetrics(&g_font, &asc, &desc, &gap);
    int base = y0 + (int)(asc * scale + 0.5f);
    float x = (float)x0; int prev = 0, prev_game = 0;
    while (s < end && *s) {
        int c = utf8_next(&s);
        uint16_t rows[12]; int gw;
        if (g_pixel && c == ' ' && hdtext_game_letter('H', rows, &gw)) { x += 4 * k; prev = c; prev_game = 0; continue; }   /* (the game's space: a gap) */
        if (g_pixel && hdtext_game_letter(c, rows, &gw)) {       /* the game's letter: its baseline on the font's */
            int ct, cb; game_caps(&ct, &cb);
            if (prev_game) x += k * (float)hdtext_game_kern(prev, c);
            draw_game(buf, w, h, x, (float)base - (float)cb * k, k, color, rows, gw);
            x += k * (float)(gw + 1); prev = c; prev_game = 1;
            continue;
        }
        if (prev && !prev_game) x += scale * (float)stbtt_GetCodepointKernAdvance(&g_font, prev, c);
        int adv, lsb; stbtt_GetCodepointHMetrics(&g_font, c, &adv, &lsb);
        int bw = 0, bh = 0, bx = 0, by = 0;
        unsigned char* bm = stbtt_GetCodepointBitmapSubpixel(&g_font, scale, scale, x - (float)(int)x, 0, c, &bw, &bh, &bx, &by);
        for (int j = 0; j < bh; j++) for (int i = 0; i < bw; i++) blend(buf, w, h, (int)x + bx + i, base + by + j, color, bm[j * bw + i]);
        stbtt_FreeBitmap(bm, NULL);
        x += scale * (float)adv; prev = c; prev_game = 0;
    }
    return (int)(x + 0.5f) - x0;
}

int text_draw(uint32_t* buf, int w, int h, int x, int y, float px, uint32_t color, const char* s) {
    return draw_n(buf, w, h, x, y, px, color, s, s + strlen(s));
}

int text_wrap(uint32_t* buf, int w, int h, int x, int y, int max_w, float px, uint32_t color, const char* s) {
    if (!load()) return y;
    int line = (int)(px * 1.15f + 0.5f);
    while (*s) {
        if (*s == '\n') { s++; y += line; continue; }
        /* the longest run of whole words that fits */
        const char* end = s; const char* fit = NULL;
        for (;;) {
            const char* e = end;
            while (*e && *e != ' ' && *e != '\n') e++;
            if (measure(px, s, e) > max_w && fit) break;
            fit = e;
            if (!*e || *e == '\n') break;
            end = e + 1;
        }
        draw_n(buf, w, h, x, y, px, color, s, fit);
        y += line;
        s = fit;
        if (*s == '\n') s++;
        while (*s == ' ') s++;
    }
    return y;
}

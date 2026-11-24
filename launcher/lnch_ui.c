/* See lnch_ui.h. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include "lnch_ui.h"

static SDL_Renderer* R;
static unsigned char* g_ttf[FONT_COUNT];

/* the characters drawn: Latin-1, and the few others the texts use */
static const int EXTRA[] = {0x2026, 0x2192, 0x25BE, 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2039, 0x203A, 0x0152, 0x0153};
#define NEXTRA (int)(sizeof EXTRA / sizeof EXTRA[0])
#define ATLAS 1024

typedef struct {
    int font, px;
    SDL_Texture* tex;
    stbtt_packedchar latin[224], extra[NEXTRA];
    float ascent;
} Face;
static Face g_faces[32];
static int g_nfaces;

static unsigned char* read_all(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char* b = n > 0 && n < (32 << 20) ? malloc((size_t)n) : NULL;
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    fclose(f);
    return b;
}

int ui_init(SDL_Renderer* r) {
    R = r;
    SDL_SetRenderDrawBlendMode(R, SDL_BLENDMODE_BLEND);
    g_ttf[FONT_TITLE] = read_all("fonts/Marcellus.ttf");
    g_ttf[FONT_TEXT] = read_all("fonts/Nunito.ttf");
    /* the first launch (no build yet, so no game/fonts): the project's fonts/ */
    if (!g_ttf[FONT_TITLE]) g_ttf[FONT_TITLE] = read_all("../fonts/Marcellus.ttf");
    if (!g_ttf[FONT_TEXT]) g_ttf[FONT_TEXT] = read_all("../fonts/Nunito.ttf");
    g_ttf[FONT_BOLD] = g_ttf[FONT_TEXT];
    return g_ttf[FONT_TITLE] && g_ttf[FONT_TEXT];
}

void ui_forget_fonts(void) {
    for (int i = 0; i < g_nfaces; i++) SDL_DestroyTexture(g_faces[i].tex);
    g_nfaces = 0;
}

static Face* face(int font, float pxf) {
    int px = (int)(pxf + 0.5f);
    if (px < 6) px = 6;
    if (px > 160) px = 160;
    for (int i = 0; i < g_nfaces; i++) if (g_faces[i].font == font && g_faces[i].px == px) return &g_faces[i];
    if (g_nfaces == (int)(sizeof g_faces / sizeof g_faces[0])) ui_forget_fonts();
    Face* f = &g_faces[g_nfaces];
    memset(f, 0, sizeof *f);
    f->font = font; f->px = px;
    unsigned char* a = calloc(ATLAS, ATLAS);
    if (!a) return NULL;
    stbtt_pack_context pc;
    stbtt_PackBegin(&pc, a, ATLAS, ATLAS, 0, 1, NULL);
    stbtt_PackSetOversampling(&pc, px < 40 ? 2 : 1, 1);
    stbtt_pack_range rg[2];
    memset(rg, 0, sizeof rg);
    rg[0].font_size = (float)px; rg[0].first_unicode_codepoint_in_range = 32; rg[0].num_chars = 224; rg[0].chardata_for_range = f->latin;
    rg[1].font_size = (float)px; rg[1].array_of_unicode_codepoints = (int*)EXTRA; rg[1].num_chars = NEXTRA; rg[1].chardata_for_range = f->extra;
    stbtt_PackFontRanges(&pc, g_ttf[font], 0, rg, 2);
    stbtt_PackEnd(&pc);
    stbtt_fontinfo fi;
    stbtt_InitFont(&fi, g_ttf[font], 0);
    int asc, desc, gap;
    stbtt_GetFontVMetrics(&fi, &asc, &desc, &gap);
    f->ascent = asc * stbtt_ScaleForPixelHeight(&fi, (float)px);
    Uint32* px32 = malloc((size_t)ATLAS * ATLAS * 4);
    if (!px32) { free(a); return NULL; }
    for (int i = 0; i < ATLAS * ATLAS; i++) px32[i] = 0x00FFFFFFu | ((Uint32)a[i] << 24);
    free(a);
    f->tex = SDL_CreateTexture(R, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, ATLAS, ATLAS);
    if (f->tex) {
        SDL_UpdateTexture(f->tex, NULL, px32, ATLAS * 4);
        SDL_SetTextureBlendMode(f->tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(f->tex, SDL_ScaleModeLinear);
    }
    free(px32);
    if (!f->tex) return NULL;
    g_nfaces++;
    return f;
}

static int utf8_next(const char** s) {
    const unsigned char* p = (const unsigned char*)*s;
    int c = *p, n = 0;
    if (c < 0x80) n = 0;
    else if ((c & 0xE0) == 0xC0) { c &= 0x1F; n = 1; }
    else if ((c & 0xF0) == 0xE0) { c &= 0x0F; n = 2; }
    else if ((c & 0xF8) == 0xF0) { c &= 0x07; n = 3; }
    else { (*s)++; return '?'; }
    p++;
    for (int i = 0; i < n; i++, p++) {
        if ((*p & 0xC0) != 0x80) { *s = (const char*)p; return '?'; }
        c = (c << 6) | (*p & 0x3F);
    }
    *s = (const char*)p;
    return c;
}
static stbtt_packedchar* glyph(Face* f, int c) {
    if (c >= 32 && c < 256) return &f->latin[c - 32];
    for (int i = 0; i < NEXTRA; i++) if (EXTRA[i] == c) return &f->extra[i];
    return &f->latin['?' - 32];
}

float ui_text_width(int font, float px, const char* s) {
    Face* f = face(font, px);
    if (!f || !s) return 0;
    float w = 0;
    while (*s) w += glyph(f, utf8_next(&s))->xadvance;
    return w;
}

float ui_text(int font, float px, float x, float y, Col c, const char* s, float maxw, int align) {
    Face* f = face(font, px);
    if (!f || !s) return 0;
    char cut[512];
    float w = ui_text_width(font, px, s);
    if (maxw > 0 && w > maxw) {                         /* shortened, ending in "…" */
        float ell = ui_text_width(font, px, "\xE2\x80\xA6");
        size_t n = 0;
        const char* p = s;
        float acc = 0;
        while (*p) {
            const char* q = p;
            float a = glyph(f, utf8_next(&q))->xadvance;
            if (acc + a + ell > maxw || (size_t)(q - s) + 4 >= sizeof cut) break;
            acc += a; p = q;
        }
        n = (size_t)(p - s);
        memcpy(cut, s, n);
        while (n && cut[n - 1] == ' ') n--;
        strcpy(cut + n, "\xE2\x80\xA6");
        s = cut;
        w = ui_text_width(font, px, s);
    }
    float cx = align == 1 ? x - w / 2 : align == 2 ? x - w : x, cy = y + f->ascent;
    SDL_SetTextureColorMod(f->tex, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(f->tex, c.a);
    /* FONT_BOLD: Nunito is a variable font and stb_truetype draws only its regular
     * weight, so bold is the same letters drawn again a little to the right */
    float bold = font == FONT_BOLD ? (f->px / 26.0f < 0.6f ? 0.6f : f->px / 26.0f) : 0;
    for (int pass = 0; pass < (bold > 0 ? 2 : 1); pass++) {
        const char* p = s;
        float px = cx + pass * bold, py = cy;
        while (*p) {
            stbtt_aligned_quad q;
            stbtt_packedchar* g = glyph(f, utf8_next(&p));
            int idx = (int)(g - (g >= f->extra ? f->extra : f->latin));
            stbtt_GetPackedQuad(g >= f->extra && g < f->extra + NEXTRA ? f->extra : f->latin, ATLAS, ATLAS, idx, &px, &py, &q, 0);
            SDL_Rect src = {(int)(q.s0 * ATLAS), (int)(q.t0 * ATLAS), (int)((q.s1 - q.s0) * ATLAS + 0.5f), (int)((q.t1 - q.t0) * ATLAS + 0.5f)};
            SDL_FRect dst = {q.x0, q.y0, q.x1 - q.x0, q.y1 - q.y0};
            if (src.w > 0 && src.h > 0) SDL_RenderCopyF(R, f->tex, &src, &dst);
        }
    }
    return w + bold;
}

static SDL_Color sc(Col c) { SDL_Color s = {c.r, c.g, c.b, c.a}; return s; }
static Col mix(Col a, Col b, float t) {
    Col c = {(Uint8)(a.r + (b.r - a.r) * t), (Uint8)(a.g + (b.g - a.g) * t), (Uint8)(a.b + (b.b - a.b) * t), (Uint8)(a.a + (b.a - a.a) * t)};
    return c;
}

/* the outline of a rounded rectangle, clockwise: 4 corners of 10 points */
#define CP 10
static int outline(float x, float y, float w, float h, float rad, SDL_FPoint* pt) {
    if (rad > w / 2) rad = w / 2;
    if (rad > h / 2) rad = h / 2;
    const float cx[4] = {x + w - rad, x + w - rad, x + rad, x + rad}, cy[4] = {y + rad, y + h - rad, y + h - rad, y + rad};
    int n = 0;
    for (int k = 0; k < 4; k++)
        for (int i = 0; i < CP; i++) {
            float a = 3.14159265f * (-0.5f + 0.5f * k + 0.5f * i / (CP - 1));
            pt[n].x = cx[k] + cosf(a) * rad; pt[n].y = cy[k] + sinf(a) * rad; n++;
        }
    return n;
}

void ui_round_rect(float x, float y, float w, float h, float rad, Col top, Col bottom) {
    SDL_FPoint pt[4 * CP];
    int n = outline(x, y, w, h, rad, pt);
    SDL_Vertex v[4 * CP + 1];
    int idx[4 * CP * 3];
    v[0].position.x = x + w / 2; v[0].position.y = y + h / 2; v[0].color = sc(mix(top, bottom, 0.5f));
    for (int i = 0; i < n; i++) { v[i + 1].position = pt[i]; v[i + 1].color = sc(mix(top, bottom, (pt[i].y - y) / h)); v[i + 1].tex_coord.x = v[i + 1].tex_coord.y = 0; }
    v[0].tex_coord.x = v[0].tex_coord.y = 0;
    for (int i = 0; i < n; i++) { idx[i * 3] = 0; idx[i * 3 + 1] = i + 1; idx[i * 3 + 2] = (i + 1) % n + 1; }
    SDL_RenderGeometry(R, NULL, v, n + 1, idx, n * 3);
}

void ui_round_border(float x, float y, float w, float h, float rad, float t, Col c) {
    SDL_FPoint a[4 * CP], b[4 * CP];
    int n = outline(x, y, w, h, rad, a);
    outline(x + t, y + t, w - 2 * t, h - 2 * t, rad > t ? rad - t : 0, b);
    SDL_Vertex v[8 * CP];
    int idx[4 * CP * 6];
    for (int i = 0; i < n; i++) {
        v[2 * i].position = a[i]; v[2 * i + 1].position = b[i];
        v[2 * i].color = v[2 * i + 1].color = sc(c);
        v[2 * i].tex_coord.x = v[2 * i].tex_coord.y = v[2 * i + 1].tex_coord.x = v[2 * i + 1].tex_coord.y = 0;
        int j = (i + 1) % n;
        int* q = idx + i * 6;
        q[0] = 2 * i; q[1] = 2 * j; q[2] = 2 * i + 1; q[3] = 2 * i + 1; q[4] = 2 * j; q[5] = 2 * j + 1;
    }
    SDL_RenderGeometry(R, NULL, v, 2 * n, idx, n * 6);
}

static void quad(float x, float y, float w, float h, Col c00, Col c10, Col c01, Col c11) {
    SDL_Vertex v[4] = {
        {{x, y}, sc(c00), {0, 0}}, {{x + w, y}, sc(c10), {0, 0}},
        {{x, y + h}, sc(c01), {0, 0}}, {{x + w, y + h}, sc(c11), {0, 0}}};
    int idx[6] = {0, 1, 2, 2, 1, 3};
    SDL_RenderGeometry(R, NULL, v, 4, idx, 6);
}
void ui_triangle(float x1, float y1, float x2, float y2, float x3, float y3, Col c) {
    SDL_Vertex v[3] = {{{x1, y1}, sc(c), {0, 0}}, {{x2, y2}, sc(c), {0, 0}}, {{x3, y3}, sc(c), {0, 0}}};
    SDL_RenderGeometry(R, NULL, v, 3, NULL, 0);
}
void ui_hgrad(float x, float y, float w, float h, Col l, Col r) { quad(x, y, w, h, l, r, l, r); }
void ui_vgrad(float x, float y, float w, float h, Col t, Col b) { quad(x, y, w, h, t, t, b, b); }
void ui_fill(float x, float y, float w, float h, Col c) { quad(x, y, w, h, c, c, c, c); }

SDL_Texture* ui_load_picture(const char* path, int* w, int* h) {
    int n;
    unsigned char* px = stbi_load(path, w, h, &n, 4);
    if (!px) return NULL;
    SDL_Texture* t = SDL_CreateTexture(R, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, *w, *h);
    if (t) { SDL_UpdateTexture(t, NULL, px, *w * 4); SDL_SetTextureScaleMode(t, SDL_ScaleModeBest); }
    stbi_image_free(px);
    return t;
}

void ui_picture_cover(SDL_Texture* t, int tw, int th, float x, float y, float w, float h) {
    float s = fmaxf(w / tw, h / th);                    /* fills the area, the picture's middle kept */
    float sw = w / s, sh = h / s;
    SDL_Rect src = {(int)((tw - sw) / 2), (int)((th - sh) / 2), (int)sw, (int)sh};
    SDL_FRect dst = {x, y, w, h};
    SDL_RenderCopyF(R, t, &src, &dst);
}

void ui_stars(float x, float y, float w, float h) {
    ui_vgrad(x, y, w, h * 0.75f, col(0x0C1A44, 255), col(0x1B3A7A, 255));
    ui_vgrad(x, y + h * 0.75f, w, h * 0.25f + 1, col(0x1B3A7A, 255), col(0x2B4C8A, 255));
    unsigned s = 12345;
    for (int i = 0; i < 140; i++) {                     /* the same sky every time */
        s = s * 1103515245u + 12345u; float fx = (s >> 8) % 10000 / 10000.0f;
        s = s * 1103515245u + 12345u; float fy = (s >> 8) % 10000 / 10000.0f;
        s = s * 1103515245u + 12345u; int big = (s >> 8) % 7 == 0;
        float r = big ? 2.0f : 1.2f;
        ui_fill(x + fx * w, y + fy * h * 0.8f, r, r, big ? col(0xFFE9A8, 230) : col(0xFFFFFF, 200));
    }
}

int ui_save_png(const char* path, int w, int h) {
    unsigned char* px = malloc((size_t)w * h * 4);
    if (!px) return 0;
    int ok = SDL_RenderReadPixels(R, NULL, SDL_PIXELFORMAT_ABGR8888, px, w * 4) == 0
          && stbi_write_png(path, w, h, 4, px, w * 4);
    free(px);
    return ok;
}

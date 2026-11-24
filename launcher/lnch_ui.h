/* The launcher's drawing on an SDL_Renderer: text (stb_truetype), rounded
 * panels with gradients (SDL_RenderGeometry), pictures (stb_image). */
#pragma once
#include <SDL.h>

typedef struct { Uint8 r, g, b, a; } Col;
static inline Col col(unsigned rgb, Uint8 a) { Col c = {(Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb, a}; return c; }

enum { FONT_TITLE, FONT_TEXT, FONT_BOLD, FONT_COUNT };   /* Marcellus; Nunito (its variable font, regular weight) */

int   ui_init(SDL_Renderer* r);                 /* 0 if a font is missing (fonts/Marcellus.ttf, fonts/Nunito.ttf, or the project's ../fonts/) */
void  ui_forget_fonts(void);                    /* after a resize: glyphs made again at the new sizes */
float ui_text_width(int font, float px, const char* s);
/* align 0 left, 1 centre, 2 right of x; a text wider than maxw (> 0) ends in "…"; returns its width */
float ui_text(int font, float px, float x, float y, Col c, const char* s, float maxw, int align);
void  ui_round_rect(float x, float y, float w, float h, float rad, Col top, Col bottom);
void  ui_round_border(float x, float y, float w, float h, float rad, float t, Col c);
void  ui_triangle(float x1, float y1, float x2, float y2, float x3, float y3, Col c);
void  ui_hgrad(float x, float y, float w, float h, Col left, Col right);
void  ui_vgrad(float x, float y, float w, float h, Col top, Col bottom);
void  ui_fill(float x, float y, float w, float h, Col c);
SDL_Texture* ui_load_picture(const char* path, int* w, int* h);   /* NULL if missing or unreadable */
void  ui_picture_cover(SDL_Texture* t, int tw, int th, float x, float y, float w, float h);
void  ui_stars(float x, float y, float w, float h);                /* the night sky without the picture */
int   ui_save_png(const char* path, int w, int h);                 /* the window as it is now */

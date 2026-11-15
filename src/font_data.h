#ifndef FONT_DATA_H
#define FONT_DATA_H
typedef struct { short x, y, w, h, xoff, yoff; float adv; } Glyph;
typedef struct { int aw, ah, lineh; Glyph g[95]; const unsigned char *px; } FontData;
extern const FontData font_data[4];
#endif

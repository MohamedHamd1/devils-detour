/* Procedural pixel art: every sprite, tile and backdrop is painted in code at start-up,
 * then drawn with nearest-neighbour scaling so every art pixel is a crisp 2x2 block. */
#ifndef ART_H
#define ART_H

typedef Uint32 Px;
static Px PXC(int r, int g, int b, int a) {
  if (r < 0) r = 0; if (r > 255) r = 255; if (g < 0) g = 0; if (g > 255) g = 255;
  if (b < 0) b = 0; if (b > 255) b = 255; if (a < 0) a = 0; if (a > 255) a = 255;
  return ((Uint32)a << 24) | ((Uint32)r << 16) | ((Uint32)g << 8) | (Uint32)b;
}
static Px PXF(const float *c, float k, int a) { return PXC((int)(c[0] * k * 255), (int)(c[1] * k * 255), (int)(c[2] * k * 255), a); }
static Px pxmix(Px a, Px b, float t) {
  int ar = (a >> 16) & 255, ag = (a >> 8) & 255, ab = a & 255, aa = a >> 24;
  int br = (b >> 16) & 255, bg = (b >> 8) & 255, bb = b & 255, ba = b >> 24;
  return PXC((int)(ar + (br - ar) * t), (int)(ag + (bg - ag) * t), (int)(ab + (bb - ab) * t), (int)(aa + (ba - aa) * t));
}
static Px pxk(Px a, float k) {
  return PXC((int)(((a >> 16) & 255) * k), (int)(((a >> 8) & 255) * k), (int)((a & 255) * k), (int)(a >> 24));
}
static unsigned hsh(int x, int y, int s) {
  unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + (unsigned)s * 2147483647u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

static SDL_Texture *mktex(const Px *px, int w, int h, int linear, int add) {
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, linear ? "1" : "0");
  SDL_Texture *t = SDL_CreateTexture(RD, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, w, h);
  SDL_UpdateTexture(t, NULL, px, w * 4);
  SDL_SetTextureBlendMode(t, add ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  return t;
}

typedef struct {
  int world;
  SDL_Texture *sky, *far_, *mid, *top[4], *fill[4], *crTop, *crFill, *door[5];
  SDL_Texture *spike, *coin[4], *player[7], *glow, *vig;
  SDL_Texture *sgExit, *sgChev, *sgDecoy, *sgQ, *torch[3], *crystal, *icon[36];
  int ready;
} Art;
static Art A;

/* ---------------------------------------------------------------- static sprites */
static void gen_spike(void) {
  Px b[256]; memset(b, 0, sizeof b);
  Px lite = PXC(246, 248, 255, 255), mid = PXC(194, 200, 220, 255), dk = PXC(128, 134, 160, 255), ol = PXC(46, 50, 72, 255);
  for (int y = 2; y < 16; y++) {
    double hw = 1.0 + (y - 2) * 6.5 / 13.0;
    for (int x = 0; x < 16; x++) {
      double dx = x + 0.5 - 8.0;
      if (fabs(dx) > hw) continue;
      Px c;
      if (fabs(dx) > hw - 1.0 || y == 15) c = ol;
      else if (dx < -0.9) c = lite;
      else if (dx < 0.9) c = mid;
      else c = dk;
      b[y * 16 + x] = c;
    }
  }
  b[3 * 16 + 7] = PXC(255, 255, 255, 255);
  A.spike = mktex(b, 16, 16, 0, 0);
}

static void gen_coin(void) {
  static const int fw[4] = { 12, 9, 4, 9 };
  Px ol = PXC(122, 74, 6, 255), g1 = PXC(255, 214, 54, 255), g2 = PXC(255, 176, 28, 255), g3 = PXC(255, 244, 150, 255);
  for (int f = 0; f < 4; f++) {
    Px b[256]; memset(b, 0, sizeof b);
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
      double dx = (x + 0.5 - 8.0) / (fw[f] / 2.0), dy = (y + 0.5 - 8.0) / 6.0;
      double r2 = dx * dx + dy * dy;
      if (r2 > 1.0) continue;
      Px c;
      if (r2 > 0.72) c = ol;
      else if (dx < -0.35 && dy < 0.1) c = g3;
      else if (dx > 0.35) c = g2;
      else c = g1;
      if (fw[f] >= 9 && fabs(dx) < 0.16 && fabs(dy) < 0.5) c = g2;
      b[y * 16 + x] = c;
    }
    A.coin[f] = mktex(b, 16, 16, 0, 0);
  }
}

#define PAW 12
#define PAH 14
static void gen_player(void) {
  Px ol = PXC(30, 24, 46, 255), y0 = PXC(255, 219, 89, 255), hi = PXC(255, 244, 168, 255), sh = PXC(222, 168, 46, 255);
  Px wh = PXC(255, 255, 255, 255), pu = PXC(30, 24, 46, 255), sc = PXC(232, 72, 84, 255), sd = PXC(168, 40, 62, 255);
  static const int legA[5] = { 3, 2, 4, 5, 4 }, legB[5] = { 7, 8, 6, 5, 7 };
  for (int f = 0; f < 7; f++) {
    Px b[PAW * PAH]; memset(b, 0, sizeof b);
#define P(x, y, c) do { if ((x) >= 0 && (x) < PAW && (y) >= 0 && (y) < PAH) b[(y) * PAW + (x)] = (c); } while (0)
    /* body 1..10 x 0..10 */
    for (int y = 0; y <= 10; y++) for (int x = 1; x <= 10; x++) {
      if ((y == 0 || y == 10) && (x == 1 || x == 10)) continue;
      Px c = y0;
      int edge = (x == 1 || x == 10 || y == 0 || y == 10);
      if (edge) c = ol;
      else if (y == 1 || (x == 2 && y < 6)) c = hi;
      else if (y == 9 || x == 9) c = sh;
      P(x, y, c);
    }
    /* eyes looking right */
    for (int y = 3; y <= 5; y++) { P(5, y, wh); P(6, y, wh); P(8, y, wh); P(9, y, wh); }
    P(6, 4, pu); P(6, 5, pu); P(9, 4, pu); P(9, 5, pu);
    /* scarf */
    for (int x = 2; x <= 9; x++) P(x, 8, sc);
    for (int x = 2; x <= 9; x += 2) P(x, 9 - 0, sd);
    int ty = (f >= 1 && f <= 4) ? (f % 2 ? 7 : 9) : 8;
    P(0, ty, sc); P(1, ty, sc); P(0, ty + 1, sd);
    /* legs */
    if (f == 5) { for (int x = 4; x <= 7; x++) { P(x, 11, sh); P(x, 12, ol); } }
    else if (f == 6) { P(2, 11, sh); P(1, 12, ol); P(2, 12, ol); P(9, 11, sh); P(9, 12, ol); P(10, 12, ol); }
    else {
      int a = f == 0 ? 3 : legA[f - 1], c2 = f == 0 ? 7 : legB[f - 1];
      int liftA = (f == 1 || f == 3), liftB = (f == 2 || f == 4);
      P(a, 11, sh); P(a + 1, 11, sh);
      P(c2, 11, sh); P(c2 + 1, 11, sh);
      int ya = liftA ? 12 : 13, yb = liftB ? 12 : 13;
      P(a, ya, ol); P(a + 1, ya, ol); P(a - 1, ya, ol);
      P(c2, yb, ol); P(c2 + 1, yb, ol); P(c2 + 2, yb, ol);
      if (ya == 13) P(a, 12, ol);
      if (yb == 13) P(c2 + 1, 12, ol);
    }
#undef P
    A.player[f] = mktex(b, PAW, PAH, 0, 0);
  }
}

/* ---------------------------------------------------------------- signs and icons */
#define NICON 36
#define ICON_BULB 20
#define ICON_GENERIC 21
#define ICON_FLAG F_NUMB
#define ICON_DIE 15

static Px icol(char c) {
  switch (c) {
    case 'W': return PXC(255, 255, 255, 255); case 'w': return PXC(205, 210, 230, 255);
    case 'G': return PXC(130, 136, 162, 255); case 'K': return PXC(38, 42, 68, 255);
    case 'R': return PXC(236, 70, 82, 255);   case 'Y': return PXC(255, 212, 62, 255);
    case 'O': return PXC(246, 150, 52, 255);  case 'B': return PXC(96, 156, 255, 255);
    case 'g': return PXC(92, 222, 124, 255);  default: return 0;
  }
}
static SDL_Texture *ascii_tex(const char *const *rows, int w, int h) {
  Px b[16 * 16]; memset(b, 0, sizeof b);
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) b[y * w + x] = icol(rows[y][x]);
  return mktex(b, w, h, 0, 0);
}
static void gen_icons(void) {
  static const char *const I[NICON][8] = {
    /* 0 POP */    { "...Y....", "..YYY...", "...ww...", "..wwwG..", ".wwwwwG.", "KKKKKKKK", "KKKKKKKK", "........" },
    /* 1 ARROW */  { "........", "..g..g..", ".gg..gg.", "gggggggg", ".gg..gg.", "..g..g..", "........", "........" },
    /* 2 INVERT */ { "..GGG...", "..GWG...", "GGGWGGG.", "GWWWWWG.", "GGGWGGG.", "..GWG...", "..GGG...", "........" },
    /* 3 COINS */  { "..RRRR..", ".RWRROR.", "RWRRRROR", "RWRKKROR", "RWRKKROR", "RRRRRROR", ".RRRROR.", "..RRRR.." },
    /* 4 CRUMBLE */{ "GGGGGGGG", "GwGGKGGG", "GGGGKGwG", "GGKKGGGG", "GGGKGGGG", "GGKGGGGG", "GGGGGGwG", "GGGGGGGG" },
    /* 5 GRAVITY */{ ".B....B.", "BBB...B.", ".B....B.", ".B....B.", ".B....B.", ".B...BBB", ".B....B.", "........" },
    /* 6 FLEE */   { "....OOOO", "G...OYYO", "GG..OYYO", "....OYYO", "GG..OYOO", "G...OYYO", "....OOOO", "........" },
    /* 7 DECOY */  { ".OOOOOO.", ".OYYYYO.", ".OYRYRO.", ".OYYRYO.", ".OYRYRO.", ".OYYYYO.", ".OOOOOO.", "........" },
    /* 8 INVIS */  { "G.G.G.G.", "........", "G......G", "........", "G......G", "........", "G.G.G.G.", "........" },
    /* 9 SPEED */  { "....YY..", "...YY...", "..YY....", ".YYYYYY.", "....YY..", "...YY...", "..Y.....", "........" },
    /* 10 FAKES */ { "R..ww...", ".R.ww...", "..RwwG..", "..wRwG..", ".wwwRwG.", ".wwwwRG.", "wwwwwwRG", "KKKKKKKK" },
    /* 11 SPIKES */{ "...ww...", "...ww...", "..wwwG..", "..wwwG..", ".wwwwwG.", ".wwwwwG.", "wwwwwwwG", "KKKKKKKK" },
    /* 12 PITS */  { "gggg..gg", "KKKK..KK", "KKKK..KK", "KKKK..KK", "KKKK..KK", "KKKK..KK", "....RR..", "....RR.." },
    /* 13 WIN */   { "YYYYYYYY", "Y.YYYY.Y", "Y.YYYY.Y", ".YYYYYY.", "..YYYY..", "...YY...", "...YY...", "..YYYY.." },
    /* 14 TIME */  { "..WWWW..", ".WKKKKW.", "WKKKWKKW", "WKKKWKKW", "WKKKWWKW", "WKKKKKKW", ".WKKKKW.", "..WWWW.." },
    /* 15 DIE */   { ".WWWWWW.", "WWWWWWWW", "WKKWWKKW", "WKKWWKKW", "WWWKKWWW", ".WWWWWW.", ".WKWKWK.", "..WWWWW." },
    /* 16 JUMP */  { "...g....", "..ggg...", ".ggggg..", "...g....", "...g....", "...g....", "gggggggg", "KKKKKKKK" },
    /* 17 CTR */   { "WWWWWWWW", "WKKKKKKW", "WKggKggW", "WKKKKKKW", "WWWWWWWW", "W.W.W.W.", "........", "........" },
    /* 18 NUMB */  { "WRRR....", "WRRRR...", "WRRRRR..", "WRRRR...", "WRRR....", "W.......", "W.......", "WW......" },
    /* 19 COUNT */ { "........", ".w..w..w", ".w..w..w", "ww.ww.ww", "ww.ww.ww", "wwwwwwww", "KKKKKKKK", "........" },
    /* 20 BULB */  { "..YYYY..", ".YWYYYY.", ".YYYYYY.", ".YYYYYY.", "..YYYY..", "..GGGG..", "..GKKG..", "...GG..." },
    /* 21 ? */     { ".WWWWWW.", "WWWWWWWW", "WWKWKWKW", "WWWWWWWW", ".WWWWWW.", "..WW....", ".W......", "........" },
    /* 22 BUTTON */{ "..gggg..", ".gggggg.", "gggWWggg", "ggWggWgg", "ggWWWWgg", "ggWggWgg", ".gggggg.", "..gggg.." },
    /* 23 START */ { "........", "WWWWWWWW", "WKKWKKKW", "WKKWWKKW", "WKKWKKKW", "WWWWWWWW", "........", "........" },
    /* 24 HEART */ { ".RR..RR.", "RRRRRRRR", "RWRRRRRR", "RRRRRRRR", ".RRRRRR.", "..RRRR..", "...RR...", "........" },
    /* 25 SOUND */ { "...W....", "..WW..B.", "WWWW.B.B", "WWWW.B.B", "WWWW.B.B", "..WW..B.", "...W....", "........" },
    /* 26 PLATFORM */{ "..YY....", ".YYYY...", ".YKYK...", ".YYYY...", "........", "gggggggg", "KKKKKKKK", "KKKKKKKK" },
    /* 27 QMARK */ { "..WWWW..", ".WW..WW.", "....WW..", "...WW...", "...WW...", "........", "...WW...", "........" },
    /* 28 RESET */ { "..BBBB..", ".B....BB", "B....BBB", "B.....B.", "B.......", ".B....B.", "..BBBB..", "........" },
    /* 29 STAR */  { "...YY...", "...YY...", "YYYYYYYY", ".YYYYYY.", "..YYYY..", ".YY..YY.", "YY....YY", "........" },
    /* 30 HIGH */ { "......OO", "......OO", ".....WWW", "....WWWK", "..WWWKKK", "WWWKKKKK", "KKKKKKKK", "KKKKKKKK" },
    /* 31 TORCH */ { "...RR...", "..ROOR..", "..OYYO..", "...YY...", "...WW...", "...GG...", "...GG...", "..GGGG.." },
    /* 32 CRYST */ { "...BB...", "..BWBB..", ".BWBBBB.", "BBWBBBBB", ".BBBBBB.", "..BBBB..", "...BB...", "........" },
    /* 33 COINS */ { "........", "..YYY...", ".YWYYO..", ".YYYOYYY", "..OOYWYO", "...YYYYO", "....OOO.", "........" },
    /* 34 JK */    { "wwwwwwww", ".w.w.w.w", "...g....", "..ggg...", ".ggggg..", "...g....", "gggggggg", "KKKKKKKK" },
    /* 35 PORTAL */{ "..BBBB..", ".BwwwwB.", "BwBBBBwB", "BwBYYBwB", "BwBYYBwB", "BwBBBBwB", ".BwwwwB.", "..BBBB.." },
  };
  for (int i = 0; i < NICON; i++) A.icon[i] = ascii_tex(I[i], 8, 8);
}

static void gen_props(void) {
  static const char *T[3][16] = {
    { "................", "................", "......RR........", ".....RYYR.......", "....RYWWYR......", "....RYWWYR......", ".....ROOR.......", "......OO........", ".....GGGG.......", ".....GWWG.......", "......GG........", "......GG........", "......GG........", "....GGGGGG......", "....GGGGGG......", "................" },
    { "................", "......R.........", "......RR........", ".....RYYR.......", "....RYWWYR......", "....RYWWYR......", ".....ROOR.......", "......OO........", ".....GGGG.......", ".....GWWG.......", "......GG........", "......GG........", "......GG........", "....GGGGGG......", "....GGGGGG......", "................" },
    { "................", ".......R........", "......RR........", "......RYR.......", ".....RYWYR......", "....RYWWYR......", ".....ROOR.......", "......OO........", ".....GGGG.......", ".....GWWG.......", "......GG........", "......GG........", "......GG........", "....GGGGGG......", "....GGGGGG......", "................" } };
  for (int f = 0; f < 3; f++) {
    Px b[256]; for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) b[y * 16 + x] = icol(T[f][y][x]);
    A.torch[f] = mktex(b, 16, 16, 0, 0);
  }
  static const char *Cr[16] = { "................", "......BB........", ".....BWBB.......", "....BWBBBB......", "...BWBBBBBB.....", "...BWBBBBPB.....", "..BBWBBBBPPB....", "..BBWBBBPPPB....", "..BBBBBBPPPB....", "...BBBBBPPB.....", "...BBBBBPB......", "....BBBBB.......", ".....BBB........", "......B.........", "................", "................" };
  Px b[256];
  for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
    char ch = Cr[y][x]; Px c = 0;
    if (ch == 'B') c = PXC(60, 200, 240, 255); else if (ch == 'W') c = PXC(220, 255, 255, 255); else if (ch == 'P') c = PXC(30, 110, 190, 255);
    b[y * 16 + x] = c;
  }
  A.crystal = mktex(b, 16, 16, 0, 0);
}

static void panel_px(Px *b, int w, int h, Px edge, Px in, Px hi) {
  for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
    Px c = in;
    if (x == 0 || y == 0 || x == w - 1 || y == h - 1) c = edge;
    else if (y == 1) c = hi;
    if ((x == 0 || x == w - 1) && (y == 0 || y == h - 1)) c = 0;
    b[y * w + x] = c;
  }
}
static void man_px(Px *b, int w, int ox, int oy, Px c) {
  static const char *m[9] = { "...XX...", "...XX...", "..XXXXX.", ".X.XX.X.", "X..XX...", "...XXX..", "..XX.XX.", ".XX...XX", "XX....X." };
  for (int y = 0; y < 9; y++) for (int x = 0; x < 8; x++) if (m[y][x] == 'X') b[(oy + y) * w + ox + x] = c;
}
static void gen_signs(void) {
  Px edge = PXC(235, 255, 240, 255), in = PXC(24, 150, 72, 255), hi = PXC(70, 200, 112, 255), white = PXC(255, 255, 255, 255);
  { /* exit sign with arrow: 44 x 18 */
    static Px b[44 * 18]; memset(b, 0, sizeof b);
    panel_px(b, 44, 18, edge, in, hi);
    man_px(b, 44, 5, 5, white);
    for (int x = 17; x <= 32; x++) for (int y = 8; y <= 10; y++) b[y * 44 + x] = white;
    for (int x = 33; x <= 38; x++) { int half = 5 - (x - 33); for (int y = 9 - half; y <= 9 + half; y++) b[y * 44 + x] = white; }
    A.sgExit = mktex(b, 44, 18, 0, 0);
  }
  { /* chevrons: 28 x 10 */
    static Px b[28 * 10]; memset(b, 0, sizeof b);
    Px c = PXC(110, 240, 150, 255), d = PXC(40, 150, 80, 255);
    for (int k = 0; k < 3; k++) for (int y = 0; y < 9; y++) {
      int dd = y <= 4 ? y : 8 - y, x = 1 + k * 9 + dd;
      b[y * 28 + x] = c; b[y * 28 + x + 1] = c; b[y * 28 + x + 2] = d;
    }
    A.sgChev = mktex(b, 28, 10, 0, 0);
  }
  { /* decoy exit sign (no arrow): 28 x 18 */
    static Px b[28 * 18]; memset(b, 0, sizeof b);
    panel_px(b, 28, 18, PXC(255, 240, 200, 255), PXC(24, 150, 72, 255), PXC(70, 200, 112, 255));
    man_px(b, 28, 10, 5, white);
    A.sgDecoy = mktex(b, 28, 18, 0, 0);
  }
  { /* question sign: 16 x 16 */
    static Px b[16 * 16]; memset(b, 0, sizeof b);
    panel_px(b, 16, 16, PXC(190, 196, 220, 255), PXC(58, 64, 96, 255), PXC(90, 98, 136, 255));
    static const char *q[7] = { ".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.." };
    for (int y = 0; y < 7; y++) for (int x = 0; x < 5; x++) if (q[y][x] == '#') b[(4 + y) * 16 + 5 + x] = white;
    A.sgQ = mktex(b, 16, 16, 0, 0);
  }
}

static void gen_glow_vig(void) {
  Px g[64 * 64];
  for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
    double dx = (x + 0.5 - 32) / 32.0, dy = (y + 0.5 - 32) / 32.0, d = sqrt(dx * dx + dy * dy);
    double a = d >= 1 ? 0 : (1 - d) * (1 - d);
    g[y * 64 + x] = PXC(255, 255, 255, (int)(a * 255));
  }
  A.glow = mktex(g, 64, 64, 1, 1);
  static Px v[160 * 120];
  for (int y = 0; y < 120; y++) for (int x = 0; x < 160; x++) {
    double dx = (x + 0.5 - 80) / 80.0, dy = (y + 0.5 - 60) / 60.0, d = sqrt(dx * dx * 0.7 + dy * dy * 0.9);
    double a = (d - 0.55) * 0.95; if (a < 0) a = 0; if (a > 0.7) a = 0.7;
    v[y * 160 + x] = PXC(0, 0, 8, (int)(a * 255));
  }
  A.vig = mktex(v, 160, 120, 1, 0);
}

/* ---------------------------------------------------------------- per-world art */
static void gen_tiles(const Pal *pl) {
  Px body = PXF(pl->g1, 1.0f, 255), dark = PXF(pl->g1, 0.74f, 255), light = PXF(pl->g1, 1.2f, 255), deep = PXF(pl->g1, 0.5f, 255);
  Px top = PXF(pl->g2, 1.0f, 255), toph = PXF(pl->g2, 1.25f, 255), tops = PXF(pl->g2, 0.78f, 255), topd = PXF(pl->g2, 0.6f, 255);
  Px orange = PXC(255, 224, 150, 255);
  for (int v = 0; v < 4; v++) {
    Px f[256], t[256];
    for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
      unsigned h = hsh(x, y, v + 10) % 100;
      Px c = body;
      if (h < 12) c = dark; else if (h < 19) c = light;
      f[y * 16 + x] = c;
    }
    /* stones */
    for (int s = 0; s < 3; s++) {
      int sx = 1 + hsh(s, v, 3) % 11, sy = 2 + hsh(s, v, 4) % 11;
      for (int dy = 0; dy < 3; dy++) for (int dx = 0; dx < 3; dx++) {
        if ((dx == 2 && dy == 0) || (dx == 0 && dy == 2)) continue;
        Px c = (dx + dy <= 1) ? light : dark;
        if (dx == 1 && dy == 1) c = body;
        f[(sy + dy) * 16 + sx + dx] = c;
      }
    }
    /* block edges give a chunky tiled look */
    for (int i = 0; i < 16; i++) { f[15 * 16 + i] = deep; f[i * 16 + 15] = dark; f[i * 16 + 0] = pxmix(f[i * 16 + 0], light, 0.35f); }
    memcpy(t, f, sizeof t);
    for (int x = 0; x < 16; x++) {
      int drip = 4 + (int)(hsh(x, v, 21) % 4);
      if (hsh(x, v, 22) % 3 == 0) drip += 1;
      for (int y = 0; y < drip; y++) t[y * 16 + x] = (y == 0) ? toph : (y < 3 ? top : tops);
      t[drip * 16 + x] = topd;
      if (hsh(x, v, 23) % 5 == 0) t[1 * 16 + x] = toph;
    }
    for (int y = 0; y < 16; y++) { t[y * 16 + 15] = pxmix(t[y * 16 + 15], deep, 0.5f); }
    A.fill[v] = mktex(f, 16, 16, 0, 0);
    A.top[v] = mktex(t, 16, 16, 0, 0);
    if (v == 0) {
      Px cf[256], ct[256];
      memcpy(cf, f, sizeof cf); memcpy(ct, t, sizeof ct);
      for (int i = 0; i < 256; i++) { cf[i] = pxmix(cf[i], orange, 0.28f); ct[i] = pxmix(ct[i], orange, 0.28f); }
      static const int cr[14][2] = { {8,0},{7,1},{7,2},{6,3},{6,4},{7,5},{8,6},{8,7},{9,8},{9,9},{10,10},{10,11},{11,12},{11,13} };
      for (int k2 = 0; k2 < 14; k2++) { cf[cr[k2][1] * 16 + cr[k2][0]] = deep; ct[cr[k2][1] * 16 + cr[k2][0]] = deep; }
      for (int k2 = 3; k2 < 12; k2++) { if (k2 % 2) { cf[(k2) * 16 + 3] = deep; ct[(k2) * 16 + 3] = deep; } }
      for (int i = 0; i < 16; i += 4) { cf[i] = PXC(255, 200, 120, 255); ct[15 * 16 + i] = deep; }
      A.crFill = mktex(cf, 16, 16, 0, 0); A.crTop = mktex(ct, 16, 16, 0, 0);
    }
  }
  /* five door looks; which one is the real exit is decided per level */
  static const int DP[5][3][3] = {
    { {150, 96, 56}, {190, 130, 80}, {66, 38, 24} },      /* wood */
    { {250, 172, 44}, {255, 214, 110}, {140, 78, 10} },   /* gold */
    { {96, 120, 160}, {150, 176, 210}, {34, 44, 72} },    /* steel */
    { {40, 150, 120}, {110, 230, 190}, {10, 60, 50} },    /* portal ring */
    { {150, 50, 70}, {210, 100, 120}, {60, 16, 30} },     /* crimson */
  };
  for (int d = 0; d < 5; d++) {
    Px wood = PXC(DP[d][0][0], DP[d][0][1], DP[d][0][2], 255);
    Px woodl = PXC(DP[d][1][0], DP[d][1][1], DP[d][1][2], 255);
    Px frame = PXC(DP[d][2][0], DP[d][2][1], DP[d][2][2], 255);
    Px knob = PXC(255, 236, 120, 255);
    Px b[16 * 24]; memset(b, 0, sizeof b);
    if (d == 0) {   /* the exit flag: checkered finish flag on a pole with a gold ball and a stone base */
      static const char *DW[24] = {
        "...yYy..........",
        "...yYy..........",
        "....PHHBBHHBBHH.",
        "....PHHBBHHBBHH.",
        "....PBBHHBBHHBB.",
        "....PBBHHBBHHBB.",
        "....PHHBBHHBBHH.",
        "....PHHBBHHBBHH.",
        "....PBBHHBBHHBB.",
        "....PBBHHBBHHBB.",
        "....P...........",
        "....P...........",
        "....P...........",
        "....P...........",
        "....P...........",
        "....P...........",
        "....P...........",
        "....P...........",
        "....P...........",
        "....P...........",
        "....P...........",
        ".LLLPlll........",
        "llllllMMM.......",
        "lllllllMMM......" };
      for (int y = 0; y < 24; y++) for (int x = 0; x < 16; x++) {
        Px c = 0;
        switch (DW[y][x]) {
          case 'L': c = PXC(206, 200, 226, 255); break; case 'l': c = PXC(140, 132, 170, 255); break;
          case 'M': c = PXC(80, 72, 106, 255); break;   case 'P': c = PXC(236, 238, 248, 255); break;
          case 'H': c = PXC(252, 250, 255, 255); break; case 'B': c = PXC(34, 24, 62, 255); break;
          case 'Y': c = PXC(255, 240, 140, 255); break; case 'y': c = PXC(230, 170, 40, 255); break;
          default: break;
        }
        b[y * 16 + x] = c;
      }
    } else
    if (d == 3) {   /* oval portal */
      for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
        double dx = (x - 7.5) / 6.5, dy = (y - 8.0) / 7.8, r = dx * dx + dy * dy;
        if (r > 1.0) continue;
        Px c = r > 0.74 ? frame : (r > 0.58 ? woodl : wood);
        if (r < 0.58 && (((x + y) & 3) == 0)) c = woodl;
        if (r < 0.12) c = PXC(220, 255, 245, 255);
        b[y * 16 + x] = c;
      }
    } else {
      for (int y = 1; y < 16; y++) for (int x = 1; x < 15; x++) {
        if (y == 1 && (x < 4 || x > 11)) continue;
        if (y == 2 && (x < 2 || x > 13)) continue;
        Px c = wood;
        int edge = (x == 1 || x == 14 || y == 1);
        if (edge) c = frame;
        else if (x == 2 || y == 2) c = woodl;
        else if (d == 2 && ((x == 4 || x == 11) && (y == 5 || y == 9 || y == 13))) c = woodl;   /* rivets */
        else if (d == 4 && (y == 7 || y == 8) && x > 3 && x < 12) c = pxk(wood, 0.75f);        /* crossbar */
        else if (d != 2 && (x == 5 || x == 10)) c = pxk(wood, 0.8f);
        b[y * 16 + x] = c;
      }
      for (int x = 3; x < 13; x++) b[3 * 16 + x] = frame;
      b[9 * 16 + 11] = knob; b[9 * 16 + 12] = knob; b[10 * 16 + 11] = pxk(knob, 0.7f); b[10 * 16 + 12] = pxk(knob, 0.7f);
      for (int x = 2; x < 14; x++) b[15 * 16 + x] = frame;
    }
    A.door[d] = mktex(b, 16, d == 0 ? 24 : 16, 0, 0);
  }
}

static void dither_fill(Px *buf, int w, int h, Px top, Px bot) {
  static const int bay[4][4] = { {0,8,2,10}, {12,4,14,6}, {3,11,1,9}, {15,7,13,5} };
  for (int y = 0; y < h; y++) {
    double v = (double)y / (h - 1) * 7.0; int step = (int)v; double fr = v - step;
    for (int x = 0; x < w; x++) {
      int s = step + (fr > (bay[y & 3][x & 3] + 0.5) / 16.0 ? 1 : 0);
      if (s > 7) s = 7;
      buf[y * w + x] = pxmix(top, bot, s / 7.0f);
    }
  }
}

static void gen_world_art(int world) {
  const Pal *pl = &PALS[world - 1];
  for (int v = 0; v < 4; v++) { if (A.top[v]) SDL_DestroyTexture(A.top[v]); if (A.fill[v]) SDL_DestroyTexture(A.fill[v]); }
  if (A.sky) SDL_DestroyTexture(A.sky); if (A.far_) SDL_DestroyTexture(A.far_); if (A.mid) SDL_DestroyTexture(A.mid);
  if (A.crTop) SDL_DestroyTexture(A.crTop); if (A.crFill) SDL_DestroyTexture(A.crFill);
  for (int d = 0; d < 5; d++) if (A.door[d]) SDL_DestroyTexture(A.door[d]);
  gen_tiles(pl);
  static Px sky[320 * 240], lay[352 * 240];
  {   /* three-stop sunset gradient with a light ordered dither */
    static const int bay[4][4] = { {0,8,2,10}, {12,4,14,6}, {3,11,1,9}, {15,7,13,5} };
    for (int y = 0; y < 240; y++) {
      double v = y / 187.0; if (v > 1) v = 1;
      Px c = v < 0.5 ? pxmix(PXF(pl->sk[0], 1, 255), PXF(pl->sk[1], 1, 255), (float)(v / 0.5)) : pxmix(PXF(pl->sk[1], 1, 255), PXF(pl->sk[2], 1, 255), (float)((v - 0.5) / 0.5));
      for (int x = 0; x < 320; x++) {
        int d = (int)((((bay[y & 3][x & 3] + 0.5) / 16.0) - 0.5) * 8);
        sky[y * 320 + x] = PXC((int)((c >> 16) & 255) + d, (int)((c >> 8) & 255) + d, (int)(c & 255) + d, 255);
      }
    }
  }
  /* stars */
  for (int s = 0; s < 70; s++) {
    int x = hsh(s, world, 1) % 320, y = hsh(s, world, 2) % 130;
    int br = 120 + hsh(s, world, 3) % 135;
    Px c = PXC(br, br, br + 20, 255);
    sky[y * 240 / 240 * 320 + x] = c;
    if (s % 9 == 0) {
      sky[y * 320 + (x + 1) % 320] = pxmix(sky[y * 320 + (x + 1) % 320], c, 0.5f);
      if (x > 0) sky[y * 320 + x - 1] = pxmix(sky[y * 320 + x - 1], c, 0.5f);
      if (y > 0) sky[(y - 1) * 320 + x] = pxmix(sky[(y - 1) * 320 + x], c, 0.5f);
      sky[(y + 1) * 320 + x] = pxmix(sky[(y + 1) * 320 + x], c, 0.5f);
    }
  }
  /* big striped sun low on the horizon, with a dithered halo */
  {
    int mx = 60 + (int)(hsh(world, 9, 9) % 200), my = 104 + (int)(hsh(world, 8, 8) % 24), r = 30 + world % 3 * 4;
    Px s0 = PXF(pl->sun[0], 1, 255), s1 = PXF(pl->sun[1], 1, 255);
    for (int y = my - r * 2; y <= my + r * 2; y++) for (int x = mx - r * 2; x <= mx + r * 2; x++) {
      if (x < 0 || x >= 320 || y < 0 || y >= 240) continue;
      double d = sqrt((double)((x - mx) * (x - mx) + (y - my) * (y - my)));
      if (d <= r) {
        double t = (y - (my - r)) / (2.0 * r);
        if (t > 0.52 && fmod(y, 5.0) < (t - 0.5) * 7.0) continue;
        sky[y * 320 + x] = pxmix(s0, s1, (float)t);
      } else if (d <= r * 1.8 && ((x + y) & 1) == 0 && d < r * 1.8 - (hsh(x, y, 7) % 6)) {
        sky[y * 320 + x] = pxmix(sky[y * 320 + x], s1, 0.22f);
      }
    }
  }
  A.sky = mktex(sky, 320, 240, 0, 0);
  /* far layer: ridge + skyline or pines */
  Px farc = PXF(pl->mt[0], 1, 255);
  Px farl = pxmix(PXF(pl->mt[0], 1.6f, 255), PXF(pl->sun[1], 1, 255), 0.2f);
  memset(lay, 0, sizeof lay);
  for (int x = 0; x < 352; x++) {
    double hgt = 104 + 30 * sin(x * 0.021 + world) + 18 * sin(x * 0.047 + world * 2.0) + 8 * sin(x * 0.11 + 1.0);
    int top = (int)hgt;
    int bTop = top;
    if (world % 2 == 0) { /* skyline */
      int bi = x / 14; int bh = 22 + (int)(hsh(bi, world, 11) % 46);
      bTop = 150 - bh; if (bTop > top) bTop = top; else bTop = bTop;
      if ((x % 14) >= 12 && hsh(bi, world, 12) % 3 == 0) bTop = top;
    }
    for (int y = bTop; y < 240; y++) {
      Px c = farc;
      if (y == bTop) c = farl;
      else if (world % 2 == 0 && y > bTop + 2 && y < 150 && (x % 4) == 1 && (y % 5) == 0 && hsh(x / 4, y / 5, world) % 100 < 22) c = PXF(pl->sun[0], 1, 255);
      lay[y * 352 + x] = c;
    }
    if (world % 2 == 1 && x % 11 == 5) { /* pines on the ridge */
      int ph = 9 + (int)(hsh(x, world, 4) % 5);
      for (int k2 = 0; k2 < ph; k2++) for (int dx = -(k2 / 3); dx <= k2 / 3; dx++) {
        int xx = x + dx, yy = top - ph + k2; if (xx >= 0 && xx < 352 && yy >= 0) lay[yy * 352 + xx] = farc;
      }
    }
  }
  A.far_ = mktex(lay, 352, 240, 0, 0);
  /* mid layer: nearer hills, fog toward the bottom */
  Px midc = PXF(pl->mt[1], 0.85f, 255);
  memset(lay, 0, sizeof lay);
  for (int x = 0; x < 352; x++) {
    double hgt = 150 + 18 * sin(x * 0.034 + world * 1.7) + 9 * sin(x * 0.09 + world);
    for (int y = (int)hgt; y < 240; y++) {
      Px c = midc;
      if (y <= (int)hgt + 1) c = pxmix(PXF(pl->mt[1], 1.5f, 255), PXF(pl->sun[1], 1, 255), 0.45f);
      else if (y > (int)hgt + 3 && ((x + y) & 1) == 0 && hsh(x, y, 6) % 9 == 0) c = pxk(midc, 1.25f);
      lay[y * 352 + x] = c;
    }
  }
  A.mid = mktex(lay, 352, 240, 0, 0);
  A.world = world;
}

static void art_init(void) {
  gen_spike(); gen_coin(); gen_player(); gen_glow_vig(); gen_icons(); gen_signs(); gen_props();
  A.ready = 1;
}
static void art_world(int world) {
  if (world < 1) world = 1; if (world > WORLDS) world = WORLDS;
  if (A.world != world) gen_world_art(world);
}

#endif

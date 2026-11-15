/* DEVIL'S DETOUR - every level lies to you.  C / SDL2 port for PortMaster (muOS). */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "font_data.h"
#include "world.h"

/* ============================================================== globals */
static SDL_Renderer *RD;
static SDL_Texture *FT[4];
static int audioOK = 0;
static SDL_AudioDeviceID adev;

typedef struct { int left, right, up, down, jump, start, select, a, b, l1, r1; } Keys;
static Keys held, prevh, pr;
static double rep[4];

enum { ST_TITLE, ST_SELECT, ST_BRIEF, ST_PLAY, ST_CARD, ST_END };
static int state = ST_TITLE;
static int idx = 1, unlocked = 1, deaths = 0, coinsHeld = 0;   /* coins you carry until you die */
static Level level;
static World *world;
static int menuSel = 0, selCursor = 1, confirmNew = 0;
static double quitT = 0, deadT = 0, cardT = 0, shake = 0, briefT = 0;
static int paused = 0, levelDeaths = 0;
static Claim lastClaims[16]; static int lastN = 0, lastIdx = 0;   /* the previous level's claims, for the R1 guide */
static double flashT = 0, lvlT = 0;
static double revealT = 0, toastT = 0; static int toastClaim = 0;   /* crystal effects: door reveal, claim verdict */
static int helpOpen = 0; static double helpScroll = 0, helpTotal = 0;
static void help_update(double dt); static void help_pass(int draw);   /* the icon guide (R1) */
static int quitFlag = 0;

typedef struct { double x, y, vx, vy, life, max, size, g; float c[3]; } Part;
static Part parts[160]; static int partNext = 0;
static double gtime = 0, squash = 0, prevVy = 0, runT = 0;
static int wasGround = 1;

/* ============================================================== sound */
enum { SND_JUMP, SND_DIE, SND_WIN, SND_TICK, SND_HURT, SND_POP, SND_COIN, SND_N };
static Sint16 *snd[SND_N]; static int sndLen[SND_N];

static void make_tone(int id, double freq, double dur, double vol, double slide, int noise) {
  int rate = 22050, n = (int)(rate * dur);
  Sint16 *b = (Sint16 *)malloc(sizeof(Sint16) * n);
  double ph = 0;
  for (int i = 0; i < n; i++) {
    double t = (double)i / n;
    ph += (freq + slide * t) / rate;
    double v = (fmod(ph, 1.0) < 0.5) ? 1.0 : -1.0;
    if (noise) v = ((double)rand() / RAND_MAX) * 2 - 1;
    b[i] = (Sint16)(v * vol * (1 - t) * 32000.0);
  }
  snd[id] = b; sndLen[id] = n;
}
static void sound_init(void) {
  SDL_AudioSpec want, have;
  SDL_zero(want);
  want.freq = 22050; want.format = AUDIO_S16SYS; want.channels = 1; want.samples = 1024;
  adev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (!adev || have.freq != 22050 || have.format != AUDIO_S16SYS || have.channels != 1) { audioOK = 0; return; }
  make_tone(SND_JUMP, 300, 0.12, 0.10, 380, 0);
  make_tone(SND_DIE, 200, 0.30, 0.16, -150, 1);
  make_tone(SND_WIN, 520, 0.30, 0.12, 420, 0);
  make_tone(SND_TICK, 900, 0.04, 0.08, 0, 0);
  make_tone(SND_HURT, 150, 0.16, 0.12, -40, 0);
  make_tone(SND_POP, 700, 0.10, 0.10, -300, 0);
  make_tone(SND_COIN, 1100, 0.09, 0.09, 700, 0);
  audioOK = 1;
  SDL_PauseAudioDevice(adev, 0);
}
static void sfx(int id) {
  if (!audioOK) return;
  SDL_ClearQueuedAudio(adev);
  SDL_QueueAudio(adev, snd[id], sndLen[id] * 2);
}

#include "art.h"

/* ============================================================== drawing */
static void setc(float r, float g, float b, float a) {
  SDL_SetRenderDrawColor(RD, (Uint8)(r * 255), (Uint8)(g * 255), (Uint8)(b * 255), (Uint8)(a * 255));
}
static void frect(double x, double y, double w, double h) {
  SDL_Rect r = { (int)floor(x + 0.5), (int)floor(y + 0.5), (int)floor(w + 0.5), (int)floor(h + 0.5) };
  SDL_RenderFillRect(RD, &r);
}
static void orect(double x, double y, double w, double h) {
  SDL_Rect r = { (int)floor(x + 0.5), (int)floor(y + 0.5), (int)floor(w + 0.5), (int)floor(h + 0.5) };
  SDL_RenderDrawRect(RD, &r);
}
static void fcircle(double cx, double cy, double rad) {
  for (int dy = (int)-rad; dy <= (int)rad; dy++) {
    double hw = sqrt(rad * rad - (double)dy * dy);
    SDL_RenderDrawLine(RD, (int)(cx - hw), (int)(cy + dy), (int)(cx + hw), (int)(cy + dy));
  }
}
static void ocircle(double cx, double cy, double rad) {
  int px = (int)(cx + rad), py = (int)cy;
  for (int a = 1; a <= 40; a++) {
    double t = a * 3.14159265 * 2 / 40;
    int x = (int)(cx + cos(t) * rad), y = (int)(cy + sin(t) * rad);
    SDL_RenderDrawLine(RD, px, py, x, y); px = x; py = y;
  }
}
/* filled isoceles triangle: apex at (ax,ay), base y=by from bx0..bx1 */
static void ftri(double ax, double ay, double bx0, double bx1, double by) {
  int ya = (int)floor(ay), yb = (int)floor(by);
  int lo = ya < yb ? ya : yb, hi = ya < yb ? yb : ya;
  for (int y = lo; y <= hi; y++) {
    double f = (yb == ya) ? 1 : (double)(y - ya) / (double)(yb - ya);
    double l = ax + (bx0 - ax) * f, r = ax + (bx1 - ax) * f;
    SDL_RenderDrawLine(RD, (int)floor(l + 0.5), y, (int)floor(r + 0.5), y);
  }
}
static void otri(double ax, double ay, double bx0, double bx1, double by) {
  SDL_RenderDrawLine(RD, (int)ax, (int)ay, (int)bx0, (int)by);
  SDL_RenderDrawLine(RD, (int)ax, (int)ay, (int)bx1, (int)by);
  SDL_RenderDrawLine(RD, (int)bx0, (int)by, (int)bx1, (int)by);
}

static void fonts_init(void) {
  for (int f = 0; f < 4; f++) {
    const FontData *fd = &font_data[f];
    Uint32 *px = (Uint32 *)malloc(sizeof(Uint32) * fd->aw * fd->ah);
    for (int i = 0; i < fd->aw * fd->ah; i++) px[i] = ((Uint32)fd->px[i] << 24) | 0x00FFFFFFu;
    FT[f] = SDL_CreateTexture(RD, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, fd->aw, fd->ah);
    SDL_UpdateTexture(FT[f], NULL, px, fd->aw * 4);
    SDL_SetTextureBlendMode(FT[f], SDL_BLENDMODE_BLEND);
    free(px);
  }
}
static double text_w(int f, const char *s) {
  double w = 0;
  for (; *s; s++) if (*s >= 32 && *s <= 126) w += font_data[f].g[*s - 32].adv;
  return w;
}
static void text_pass(int f, const char *s, double x, double y, const float *c, double a);
static void text_at(int f, const char *s, double x, double y, const float *c, double a) {
  static const float blk[3] = { 0.02f, 0.02f, 0.06f };
  double so = f >= 2 ? 2 : 1;
  text_pass(f, s, x + so, y + so, blk, a * 0.7);
  text_pass(f, s, x, y, c, a);
}
static void text_pass(int f, const char *s, double x, double y, const float *c, double a) {
  const FontData *fd = &font_data[f];
  SDL_SetTextureColorMod(FT[f], (Uint8)(c[0] * 255), (Uint8)(c[1] * 255), (Uint8)(c[2] * 255));
  SDL_SetTextureAlphaMod(FT[f], (Uint8)(a * 255));
  for (; *s; s++) {
    if (*s < 32 || *s > 126) continue;
    const Glyph *g = &fd->g[*s - 32];
    if (g->w > 0 && g->h > 0) {
      SDL_Rect src = { g->x, g->y, g->w, g->h };
      SDL_Rect dst = { (int)floor(x + g->xoff + 0.5), (int)floor(y + g->yoff + 0.5), g->w, g->h };
      SDL_RenderCopy(RD, FT[f], &src, &dst);
    }
    x += g->adv;
  }
}
/* align: 0 left, 1 center, 2 right, within box [x, x+w] */
static void text_box(int f, const char *s, double x, double y, double w, int align, const float *c, double a) {
  double tw = text_w(f, s);
  double ox = x;
  if (align == 1) ox = x + (w - tw) / 2; else if (align == 2) ox = x + w - tw;
  text_at(f, s, ox, y, c, a);
}
static const float WHITE[3] = { 1, 1, 1 };
static const float GREYT[3] = { 0.6f, 0.6f, 0.68f };
static const float YEL[3] = { 1.0f, 0.86f, 0.35f };
static const float REDT[3] = { 1.0f, 0.5f, 0.5f };
static void center(const char *s, double y, int f, const float *c, double a) { text_box(f, s, 0, y, 640, 1, c, a); }

/* ============================================================== effects / pixel-art drawing */
static const Pal *curpal;
static void emit(double x, double y, double vx, double vy, double life, double size, double g, const float *c) {
  Part *p = &parts[partNext++ % 160];
  p->x = x; p->y = y; p->vx = vx; p->vy = vy; p->life = p->max = life; p->size = size; p->g = g;
  p->c[0] = c[0]; p->c[1] = c[1]; p->c[2] = c[2];
}
static double rnd(void) { return (double)rand() / RAND_MAX; }
static void parts_update(double dt) {
  for (int i = 0; i < 160; i++) {
    Part *p = &parts[i];
    if (p->life <= 0) continue;
    p->x += p->vx * dt; p->y += p->vy * dt; p->vy += p->g * dt; p->life -= dt;
  }
}
static void parts_draw(void) {
  for (int i = 0; i < 160; i++) {
    const Part *p = &parts[i];
    if (p->life <= 0) continue;
    double a = p->life / p->max; if (a > 1) a = 1;
    setc(p->c[0], p->c[1], p->c[2], (float)(a < 0.35 ? a / 0.35 : 1));
    double s = p->size * (a < 0.5 ? 0.5 + a : 1);
    s = floor(s / 2 + 0.5) * 2; if (s < 2) s = 2;
    frect(floor(p->x / 2) * 2, floor(p->y / 2) * 2, s, s);
  }
}
static void fellipse(double cx, double cy, double rx, double ry) {
  for (int dy = (int)-ry; dy <= (int)ry; dy++) {
    double hw = rx * sqrt(1.0 - (double)(dy * dy) / (ry * ry + 0.01));
    SDL_RenderDrawLine(RD, (int)(cx - hw), (int)(cy + dy), (int)(cx + hw), (int)(cy + dy));
  }
}
static void glow(double x, double y, double r, const float *c, double a) {
  SDL_SetTextureColorMod(A.glow, (Uint8)(c[0] * 255), (Uint8)(c[1] * 255), (Uint8)(c[2] * 255));
  SDL_SetTextureAlphaMod(A.glow, (Uint8)(a * 255));
  SDL_Rect d = { (int)(x - r), (int)(y - r), (int)(r * 2), (int)(r * 2) };
  SDL_RenderCopy(RD, A.glow, NULL, &d);
}
static void panel(double x, double y, double w, double h, const float *acc) {
  setc(0.03f, 0.035f, 0.08f, 0.84f); frect(x + 2, y + 2, w - 4, h - 4);
  setc(acc[0], acc[1], acc[2], 1);
  frect(x + 2, y, w - 4, 2); frect(x + 2, y + h - 2, w - 4, 2); frect(x, y + 2, 2, h - 4); frect(x + w - 2, y + 2, 2, h - 4);
  setc(acc[0] * 0.55f, acc[1] * 0.55f, acc[2] * 0.55f, 1);
  frect(x + 2, y + 2, 2, 2); frect(x + w - 4, y + 2, 2, 2); frect(x + 2, y + h - 4, 2, 2); frect(x + w - 4, y + h - 4, 2, 2);
  setc(1, 1, 1, 0.06f); frect(x + 4, y + 4, w - 8, 2);
}
static const float ACC_BLUE[3] = { 0.36f, 0.42f, 0.7f };
static const float ACC_OK[3] = { 0.3f, 0.75f, 0.4f }, ACC_BAD[3] = { 0.8f, 0.3f, 0.3f };

static void draw_bg(const Pal *pl, double camx_, int motes, double dim) {
  SDL_Rect s = { 0, 0, 640, 480 };
  SDL_RenderCopy(RD, A.sky, NULL, &s);
  double o = camx_ - 320;
  SDL_Rect f = { (int)(-32 - o * 0.04) & ~1, 0, 704, 480 };
  SDL_RenderCopy(RD, A.far_, NULL, &f);
  SDL_Rect m = { (int)(-32 - o * 0.10) & ~1, 0, 704, 480 };
  SDL_RenderCopy(RD, A.mid, NULL, &m);
  if (motes) {
    SDL_SetRenderDrawBlendMode(RD, SDL_BLENDMODE_ADD);
    for (int k = 0; k < 26; k++) {
      double x = fmod((double)(hsh(k, 1, 1) % 640) + gtime * (5 + (k % 5) * 3), 640.0);
      double y = 70 + (double)(hsh(k, 2, 2) % 320) + sin(gtime * 0.8 + k) * 12;
      double a = 0.25 + 0.25 * sin(gtime * 2 + k * 1.7);
      setc(pl->g2[0], pl->g2[1], pl->g2[2], (float)a);
      frect(floor(x / 2) * 2, floor(y / 2) * 2, 2, 2);
    }
    SDL_SetRenderDrawBlendMode(RD, SDL_BLENDMODE_BLEND);
  }
  if (dim > 0) { setc(0, 0, 0.03f, (float)dim); frect(0, 0, 640, 480); }
}

static void draw_sign(const Sign *s) {
  static const float grn[3] = { 0.3f, 1.0f, 0.5f }, gld[3] = { 1.0f, 0.8f, 0.3f };
  SDL_Texture *tx = A.sgExit; int w = 88, h = 36;
  if (s->kind == SG_CHEV) { tx = A.sgChev; w = 56; h = 20; }
  else if (s->kind == SG_DECOY) { tx = A.sgDecoy; w = 56; h = 36; }
  else if (s->kind == SG_Q) { tx = A.sgQ; w = 32; h = 32; }
  SDL_Rect d = { (int)(s->cx - w / 2), (int)s->y, w, h };
  SDL_SetRenderDrawBlendMode(RD, SDL_BLENDMODE_ADD);
  glow(s->cx, s->y + h / 2, 52, s->kind == SG_DECOY ? gld : grn, s->kind == SG_Q ? 0.0 : 0.22 + 0.06 * sin(gtime * 3));
  SDL_SetRenderDrawBlendMode(RD, SDL_BLENDMODE_BLEND);
  SDL_RenderCopyEx(RD, tx, NULL, &d, 0, NULL, s->flip ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}
static void icon(int id, double x, double y, int sc) {
  if (id < 0 || id >= NICON) id = ICON_GENERIC;
  SDL_Rect d = { (int)x, (int)y, 8 * sc, 8 * sc };
  SDL_RenderCopy(RD, A.icon[id], NULL, &d);
}

/* ---- picture grammar for claims: icon + green tick (it happens) or red slash (it does not) */
static const float C_RED[3] = { 0.93f, 0.28f, 0.32f }, C_GRN[3] = { 0.36f, 0.88f, 0.48f };
static void thick_line(double x0, double y0, double x1, double y1, int th) {
  for (int k = -(th / 2); k <= th / 2; k++) {
    SDL_RenderDrawLine(RD, (int)x0 + k, (int)y0, (int)x1 + k, (int)y1);
    SDL_RenderDrawLine(RD, (int)x0, (int)y0 + k, (int)x1, (int)y1 + k);
  }
}
static void draw_tick(double x, double y, double s, const float *c) {
  setc(c[0], c[1], c[2], 1);
  int th = s >= 20 ? 4 : 2;
  thick_line(x + s * 0.12, y + s * 0.55, x + s * 0.40, y + s * 0.84, th);
  thick_line(x + s * 0.40, y + s * 0.84, x + s * 0.90, y + s * 0.18, th);
}
static void draw_cross(double x, double y, double s, const float *c) {
  setc(c[0], c[1], c[2], 1);
  int th = s >= 20 ? 4 : 2;
  thick_line(x + s * 0.15, y + s * 0.15, x + s * 0.85, y + s * 0.85, th);
  thick_line(x + s * 0.85, y + s * 0.15, x + s * 0.15, y + s * 0.85, th);
}
static void badge(int ass, double x, double y, double s) {
  if (!ass) {   /* red "no" sign over the picture */
    double cx = x + s / 2, cy = y + s / 2, r = s * 0.56;
    setc(C_RED[0], C_RED[1], C_RED[2], 0.95f);
    for (int k = -1; k <= 1; k++) ocircle(cx, cy, r + k);
    thick_line(cx - r * 0.72, cy - r * 0.72, cx + r * 0.72, cy + r * 0.72, 4);
  } else {      /* green tick badge in the corner */
    double rad = s * 0.27, bx = x + s * 0.80, by = y + s * 0.80;
    setc(0.02f, 0.1f, 0.05f, 1); fcircle(bx, by, rad + 2);
    setc(C_GRN[0], C_GRN[1], C_GRN[2], 1); fcircle(bx, by, rad);
    static const float wh[3] = { 1, 1, 1 };
    draw_tick(bx - rad * 0.85, by - rad * 0.85, rad * 1.7, wh);
  }
}
static double glyph_w(int id, int num, int sc) {
  if (id == M_ARROW) return 44 * sc * 0.42;
  char b[16];
  if (num >= 0) { snprintf(b, sizeof b, "%d", num); return 8 * sc + 8 + text_w(sc >= 4 ? 2 : (sc >= 3 ? 1 : 0), b); }
  return 8 * sc;
}
static void glyph(int id, int ass, int num, double x, double y, int sc) {
  double s = 8 * sc;
  if (id == M_ARROW) {
    double w = 44 * sc * 0.42, h = 18 * sc * 0.42;
    SDL_Rect d = { (int)x, (int)(y + (s - h) / 2), (int)w, (int)h };
    SDL_RenderCopyEx(RD, A.sgExit, NULL, &d, 0, NULL, ass ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    return;
  }
  icon(id, x, y, sc);
  if (num >= 0) {
    char b[16]; snprintf(b, sizeof b, "%d", num);
    int f = sc >= 4 ? 2 : (sc >= 3 ? 1 : 0);
    text_at(f, b, x + s + 8, y + s / 2 - (f == 2 ? 18 : (f == 1 ? 12 : 8)), WHITE, 1);
  } else badge(ass, x, y, s);
}
static void abutton(double cx, double cy, int sc, double a) {
  SDL_SetTextureAlphaMod(A.icon[IC_BUTTON], (Uint8)(a * 255));
  icon(IC_BUTTON, cx - 4 * sc, cy - 4 * sc, sc);
  SDL_SetTextureAlphaMod(A.icon[IC_BUTTON], 255);
}

static void draw_ent(const Ent *e) {
  double dx = e->hasd ? e->dx : e->x, dy = e->hasd ? e->dy : e->y;
  double dw = e->hasd ? e->dw : e->w, dh = e->hasd ? e->dh : e->h;
  if (!e->active && e->falling) dy += e->fallY;
  SDL_Rect d = { (int)floor(dx + 0.5), (int)floor(dy + 0.5), (int)dw, (int)dh };
  switch (e->look) {
    case LK_GROUND: {
      int v = (int)(hsh((int)e->x / 32, (int)e->y / 32, 1) & 3);
      SDL_Texture *tx = e->crumble ? (e->top ? A.crTop : A.crFill) : (e->top ? A.top[v] : A.fill[v]);
      if (e->crumble && e->hast && e->active) d.x += (int)(sin(gtime * 90) * 2);
      if (!e->active && e->falling) { SDL_SetTextureAlphaMod(tx, 200); SDL_RenderCopy(RD, tx, NULL, &d); SDL_SetTextureAlphaMod(tx, 255); }
      else if (e->flip) SDL_RenderCopyEx(RD, tx, NULL, &d, 0, NULL, SDL_FLIP_VERTICAL);
      else SDL_RenderCopy(RD, tx, NULL, &d);
      break;
    }
    case LK_SPIKE: SDL_RenderCopy(RD, A.spike, NULL, &d); break;
    case LK_TORCH: {
      SDL_Texture *tt = A.torch[((int)(gtime * 8 + dx * 0.1)) % 3];
      int lit = torch_lit(world, e);
      if (!lit) SDL_SetTextureColorMod(tt, 70, 70, 95);
      SDL_RenderCopyEx(RD, tt, NULL, &d, 0, NULL, e->flip ? SDL_FLIP_VERTICAL : SDL_FLIP_NONE);
      if (!lit) SDL_SetTextureColorMod(tt, 255, 255, 255);
      break;
    }
    case LK_CRYSTAL: d.y += (int)(sin(gtime * 2 + dx) * 2) & ~1; SDL_RenderCopyEx(RD, A.crystal, NULL, &d, 0, NULL, e->flip ? SDL_FLIP_VERTICAL : SDL_FLIP_NONE); break;
    case LK_SPIKEDOWN: SDL_RenderCopyEx(RD, A.spike, NULL, &d, 0, NULL, SDL_FLIP_VERTICAL); break;
    case LK_COIN: {
      int fr = ((int)(gtime * 9 + dx * 0.05)) & 3;
      d.y += (int)(sin(gtime * 3 + dx) * 2) & ~1;
      SDL_RenderCopy(RD, A.coin[fr], NULL, &d);
      break;
    }
    case LK_DOOR: case LK_DOOR2: {
      int tall = (e->style % 5 == 0);   /* the wooden door is taller than a tile, so it reads as a door */
      SDL_Rect dd = { (int)(dx + dw / 2 - 16), (int)dy - (tall ? 16 : 0), 32, tall ? 48 : 32 };
      SDL_RenderCopy(RD, A.door[e->style % 5], NULL, &dd);
      break;
    }
    default: break;
  }
}

static void blit_player(double cx, double bottom, int f, int flipH, int flipV, double sx, double sy) {
  double dw = PAW * 2 * sx, dh = PAH * 2 * sy;
  SDL_Rect d = { (int)floor(cx - dw / 2 + 0.5), (int)floor((flipV ? bottom : bottom - dh) + 0.5), (int)dw, (int)dh };
  int fl = (flipH ? SDL_FLIP_HORIZONTAL : 0) | (flipV ? SDL_FLIP_VERTICAL : 0);
  SDL_RenderCopyEx(RD, A.player[f], NULL, &d, 0, NULL, (SDL_RendererFlip)fl);
}

static void draw_player(const World *w) {
  const Player *p = &w->p;
  int f = 0, up = (p->vy * w->gravity) < 0;
  if (!p->ground) f = up ? 5 : 6;
  else if (fabs(p->vx) > 25) f = 1 + ((int)(gtime * 14)) % 4;
  double sx = 1, sy = 1;
  if (!p->ground) { double st = fabs(p->vy) / 2400.0; if (st > 0.14) st = 0.14; sy = 1 + st; sx = 1 - st * 0.8; }
  if (squash > 0) { sy = 1 - squash * 0.9; sx = 1 + squash * 0.9; }
  if (f == 0) sy += sin(gtime * 3.2) * 0.025;
  double cx = p->x + p->w / 2, flipV = w->gravity < 0;
  /* soft shadow on whatever is below (never on invisible floors) */
  double sh = flipV ? -1e9 : 1e9;
  for (int i = 0; i < w->nent; i++) {
    const Ent *e = &w->ents[i];
    if (e->kind != K_SOLID || !e->active || e->look == LK_NONE) continue;
    if (e->x + e->w <= p->x + 2 || e->x >= p->x + p->w - 2) continue;
    if (!flipV && e->y >= p->y + p->h - 2 && e->y < sh) sh = e->y;
    if (flipV && e->y + e->h <= p->y + 2 && e->y + e->h > sh) sh = e->y + e->h;
  }
  if (fabs(sh) < 1e8) {
    double dist = flipV ? p->y - sh : sh - (p->y + p->h);
    double k = 1.0 - (dist > 160 ? 160 : dist) / 200.0;
    setc(0, 0, 0.05f, (float)(0.38 * k));
    fellipse(cx, flipV ? sh + 1 : sh - 1, 11 * k + 3, 3);
  }
  static const float yel[3] = { 1.0f, 0.82f, 0.35f };
  SDL_SetRenderDrawBlendMode(RD, SDL_BLENDMODE_ADD);
  glow(cx, p->y + p->h / 2, 46, yel, 0.22);
  SDL_SetRenderDrawBlendMode(RD, SDL_BLENDMODE_BLEND);
  blit_player(cx, flipV ? p->y : p->y + p->h, f, p->face < 0, flipV, sx, sy);
}

/* a lit torch shows what is hidden near it: invisible floors, hidden spikes, armed ceilings. about half of the torches lie (they show it a tile away / at the wrong time) */
static void torch_reveal(const World *w) {
  const Level *lv = w->lv;
  double ph = lv->P > 0 ? fmod(w->t, lv->P) : 0;
  const double R = 170;
  for (int i = 0; i < w->nent; i++) {
    const Ent *T = &w->ents[i];
    if (T->look != LK_TORCH || !torch_lit(w, T)) continue;
    double tx = T->x + 16, ty = T->y + 16;
    double sh = T->fx ? 0 : (((wh(lv->idx, i, 5) >> 3) & 1) ? 32 : -32);
    for (int j = 0; j < w->nent; j++) {
      const Ent *e = &w->ents[j];
      if (e->look == LK_NONE && e->kind == K_SOLID) {
        double dd = hypot(e->x + 16 - tx, e->y + 16 - ty); if (dd > R) continue;
        double a = 1.0 - dd / R;
        setc(0.35f, 0.85f, 1, (float)(0.18 * a)); if (e->active) frect(e->x + sh, e->y, 32, 32);
        setc(0.45f, 0.9f, 1, (float)(0.75 * a)); orect(e->x + sh + 1, e->y + 1, 30, 30);
      } else if (e->popi) {
        double dd = hypot(e->dx + 16 - tx, e->dy + 16 - ty); if (dd > R) continue;
        double a = 1.0 - dd / R;
        SDL_SetTextureColorMod(A.spike, 255, 90, 90); SDL_SetTextureAlphaMod(A.spike, (Uint8)(200 * a));
        SDL_Rect d = { (int)(e->dx + sh), (int)e->dy, 32, 32 };
        SDL_RenderCopy(RD, A.spike, NULL, &d);
        SDL_SetTextureColorMod(A.spike, 255, 255, 255); SDL_SetTextureAlphaMod(A.spike, 255);
      }
    }
    for (int z = 0; z < lv->jk; z++) {
      double x0 = lv->jz[z].s * 32.0, x1 = (lv->jz[z].e + 1) * 32.0, y = lv->jz[z].row * 32.0 - 38;
      double nx = tx < x0 ? x0 : (tx > x1 ? x1 : tx);
      double dd = hypot(nx - tx, y - ty); if (dd > R) continue;
      int armed = ph >= lv->jz[z].a && ph < lv->jz[z].b;
      if (!(T->fx ? armed : !armed)) continue;
      double a = 1.0 - dd / R;
      setc(1, 0.3f, 0.3f, (float)(0.8 * a));
      for (double x = x0; x < x1; x += 12) if (fabs(x - tx) < R) frect(x, y, 7, 3);
    }
  }
}

static void world_draw(const World *w, int hidePlayer) {
  curpal = &PALS[w->lv->world - 1];
  art_world(w->lv->world);
  draw_bg(curpal, w->p.x + w->p.w / 2, 1, 0);
  for (int i = 0; i < w->nent; i++) {
    const Ent *e = &w->ents[i];
    if (e->look == LK_NONE) continue;
    if (e->active || (e->falling && e->fallY < 500)) draw_ent(e);
  }
  torch_reveal(w);
  if (w->jkShown) {   /* the ceiling that was waiting for you */
    int z = w->jkShown - 1;
    for (int c = w->lv->jz[z].s; c <= w->lv->jz[z].e; c++) {
      SDL_Rect d = { c * 32, (w->lv->jz[z].row - 3) * 32, 32, 32 };
      SDL_RenderCopyEx(RD, A.spike, NULL, &d, 0, NULL, SDL_FLIP_VERTICAL);
    }
  }
  /* lights: coins and doors glow */
  static const float gold[3] = { 1.0f, 0.8f, 0.3f }, warm[3] = { 1.0f, 0.7f, 0.35f }, white[3] = { 1, 1, 1 }, cy[3] = { 0.3f, 0.85f, 1.0f };
  SDL_SetRenderDrawBlendMode(RD, SDL_BLENDMODE_ADD);
  for (int i = 0; i < w->nent; i++) {
    const Ent *e = &w->ents[i];
    if (!e->active) continue;
    double pulse = 0.75 + 0.25 * sin(gtime * 3 + i);
    if (e->look == LK_COIN) glow(e->x + 16 - 8, e->y + 16 - 8, 26, gold, 0.35 * pulse);
    else if (e->look == LK_TORCH) { if (torch_lit(w, e)) glow(e->x + 14, e->y + (e->flip ? 22 : 10), 58, warm, 0.38 * (0.8 + 0.2 * sin(gtime * 11 + i))); }
    else if (e->look == LK_CRYSTAL) glow(e->x + 16, e->y + 16, 36, cy, 0.34 * pulse);
    else if (e->look == LK_DOOR || e->look == LK_DOOR2) glow(e->x + 14, e->y + 18, 48, (e->style == 3) ? cy : ((e->style == 1) ? gold : warm), 0.34 * pulse);
    else if (e->look == LK_SPIKE && e->kind == K_NONE) glow(e->x + 10, e->y + 18, 14, white, 0.0);
  }
  if (revealT > 0) {   /* a crystal showed you the real exit */
    static const float gr[3] = { 0.3f, 1.0f, 0.5f };
    double fade = revealT < 1 ? revealT : 1;
    for (int i = 0; i < w->nent; i++) {
      const Ent *e = &w->ents[i];
      if (e->kind != K_GOAL) continue;
      glow(e->x + 14, e->y + 18, 70 + 8 * sin(gtime * 6), gr, 0.75 * fade);
    }
  }
  SDL_SetRenderDrawBlendMode(RD, SDL_BLENDMODE_BLEND);
  if (revealT > 0) {
    double fade = revealT < 1 ? revealT : 1;
    for (int i = 0; i < w->nent; i++) {
      const Ent *e = &w->ents[i];
      if (e->kind != K_GOAL) continue;
      double bx = e->x + 14, by = e->y - 14 + 5 * sin(gtime * 7);
      setc(0.3f, 1, 0.5f, (float)fade);
      thick_line(bx - 10, by - 12, bx, by, 4); thick_line(bx + 10, by - 12, bx, by, 4);
    }
  }
  for (int i = 0; i < w->lv->nsigns; i++) draw_sign(&w->lv->signs[i]);
  if (!hidePlayer) draw_player(w);
  SDL_Rect v = { 0, 0, 640, 480 };
  SDL_RenderCopy(RD, A.vig, NULL, &v);
}

/* ============================================================== screens */
typedef struct {
  int kind, truth; char label[8]; int world;
  double t, mt, hold; int fails, n, taps, stage; char msg[64];
} Screen;
static Screen scr;

static int any_press(const Keys *k) { return k->left || k->right || k->up || k->down || k->jump || k->start || k->a || k->b; }
static void screen_init(const Level *lv) {
  memset(&scr, 0, sizeof scr);
  scr.kind = lv->scrKind; scr.truth = lv->scrTruth; scr.world = lv->world; scr.stage = 1;
  strcpy(scr.label, lv->scrLabel);
}
static const char *DIRN[4] = { "DOWN", "UP", "LEFT", "RIGHT" };
static int truth_pressed(void) {
  switch (scr.truth) { case 0: return pr.down; case 1: return pr.up; case 2: return pr.left; default: return pr.right; }
}
/* returns 1 when done */
static int screen_update(double dt) {
  static const char *msgs[6] = { "Nope.", "That is not the button.", "Still no.",
    "Hint: it is not on the right side of the controller.", "Hint: try the D-pad.", "Hint: only one direction works." };
  static const char *wmsgs[5] = { "Hey! Don't touch anything.", "You pressed something. Start over.",
    "Patience.", "Hands off.", "Almost. No wait, you pressed again." };
  scr.t += dt;
  if (scr.mt > 0) { scr.mt -= dt; if (scr.mt < 0) scr.mt = 0; }
  switch (scr.kind) {
    case 0:
      if (truth_pressed()) { sfx(SND_WIN); return 1; }
      if (any_press(&pr)) {
        scr.fails++;
        if (scr.fails > 6) snprintf(scr.msg, sizeof scr.msg, "It is %s. Sorry.", DIRN[scr.truth]);
        else snprintf(scr.msg, sizeof scr.msg, "%s", msgs[scr.fails - 1]);
        scr.mt = 2.5; sfx(SND_HURT);
      }
      break;
    case 1:
      if (any_press(&pr)) {
        scr.t = 0; scr.n++;
        snprintf(scr.msg, sizeof scr.msg, "%s", wmsgs[(scr.n - 1) % 5]);
        scr.mt = 2.2; sfx(SND_HURT);
      } else if (scr.t >= 4.0) { sfx(SND_WIN); return 1; }
      break;
    case 2:
      if (pr.jump) { scr.taps++; sfx(SND_TICK); }
      if (held.jump) scr.hold += dt; else scr.hold = 0;
      if (scr.taps >= 8) { sfx(SND_WIN); return 1; }
      break;
    case 3:
      if (scr.stage == 1 && (any_press(&pr) || scr.t > 14)) { scr.stage = 2; scr.t = 0; sfx(SND_WIN); }
      else if (scr.stage == 2 && scr.t > 2.2) { scr.stage = 3; scr.t = 0; sfx(SND_HURT); }
      else if (scr.stage == 3 && scr.t > 0.5 && any_press(&pr)) return 1;
      break;
  }
  return 0;
}
static void screen_draw(void) {
  art_world(scr.world);
  draw_bg(&PALS[scr.world - 1], 320 + sin(gtime * 0.3) * 120, 1, 0.45);
  char b[96];
  switch (scr.kind) {
    case 0: {
      snprintf(b, sizeof b, "PRESS %s TO CONTINUE", scr.label);
      center(b, 150, 2, WHITE, 1);
      double pulse = 1 + sin(scr.t * 4) * 0.04;
      setc(0.2f, 0.75f, 0.35f, 1); fcircle(320, 280, 52 * pulse);
      setc(1, 1, 1, 1); ocircle(320, 280, 52 * pulse);
      char l[2] = { scr.label[0], 0 };
      center(l, 255, 3, WHITE, 1);
      if (scr.mt > 0) center(scr.msg, 390, 1, REDT, scr.mt > 1 ? 1 : scr.mt);
      break;
    }
    case 1:
      center("PRESS ANY BUTTON", 140, 2, WHITE, 1);
      center("TO CONTINUE", 180, 2, WHITE, 1);
      if (scr.t > 1.2) { static const float g[3] = { .5f, .5f, .6f }; center("...", 280, 3, g, 0.6); }
      if (scr.mt > 0) center(scr.msg, 390, 1, REDT, scr.mt > 1 ? 1 : scr.mt);
      break;
    case 2:
      center("HOLD A BUTTON TO CHARGE", 140, 2, WHITE, 1);
      setc(0.2f, 0.2f, 0.28f, 1); frect(120, 250, 400, 36);
      setc(0.3f, 0.8f, 0.9f, 1); frect(120, 250, 400.0 * scr.taps / 8, 36);
      setc(1, 1, 1, 1); orect(120, 250, 400, 36);
      if (scr.hold > 1.5) { static const float o[3] = { 1, .6f, .5f }; center("Holding does nothing. Try tapping.", 330, 1, o, 1); }
      break;
    case 3:
      if (scr.stage == 1) {
        center("CONGRATULATIONS!", 120, 3, YEL, 1);
        if (scr.world == 1) center("You beat the game.", 210, 2, WHITE, 1);
        else { snprintf(b, sizeof b, "World %d complete.", scr.world); center(b, 210, 2, WHITE, 1); }
        static const float g[3] = { .7f, .7f, .8f };
        center("Thanks for playing. Press any button to exit.", 300, 1, g, 1);
        for (int i = 1; i <= 14; i++) {
          double a = scr.t * 2 + i;
          setc(1, 0.5f + 0.5f * (float)sin(i), 0.4f, 0.8f);
          fcircle(320 + cos(a * 1.3 + i) * 250, 240 + sin(a * 0.9 + i * 2) * 170, 4);
        }
      } else if (scr.stage == 2) {
        snprintf(b, sizeof b, "Saving your progress%.*s", ((int)(scr.t * 3)) % 4, "...");
        center(b, 220, 2, WHITE, 1);
      } else {
        center("Just kidding.", 170, 3, REDT, 1);
        center("The game isn't over.", 250, 2, WHITE, 1);
        static const float g[3] = { .7f, .7f, .8f };
        if (scr.t > 0.5) center("Press any button.", 340, 1, g, 1);
      }
      break;
  }
}

/* ============================================================== save */
static void save_game(void) {
  FILE *f = fopen("save.txt", "w");
  if (f) { fprintf(f, "%d\n%d\n%d\n", unlocked, deaths, coinsHeld); fclose(f); }
}
static void load_game(void) {
  FILE *f = fopen("save.txt", "r");
  if (!f) return;
  int a = 1, b = 0, c = 0;
  if (fscanf(f, "%d %d %d", &a, &b, &c) >= 1) {
    unlocked = a < 1 ? 1 : (a > COUNT ? COUNT : a); deaths = b < 0 ? 0 : b; coinsHeld = c < 0 ? 0 : c;
  }
  fclose(f);
}

/* ============================================================== flow */
static void start_level(int i) {
  idx = i < 1 ? 1 : (i > COUNT ? COUNT : i);
  level_get(idx, &level);
  revealT = toastT = 0; levelDeaths = 0; paused = 0; deadT = 0; shake = 0; squash = 0; wasGround = 1; runT = 0;
  for (int k = 0; k < 160; k++) parts[k].life = 0;
  art_world(level.world);
  if (level.kind == 1) screen_init(&level);
  else world_init(world, &level);
  state = ST_PLAY; briefT = 0; lvlT = 0;   /* no briefing or card screens any more: claims live in the R1 guide */
}
static void next_level(void) {
  if (idx >= COUNT) { state = ST_END; save_game(); return; }
  if (idx + 1 > unlocked) unlocked = idx + 1;
  save_game();
  start_level(idx + 1);
}
static void burst(const Player *p) {
  static const float cols[4][3] = { { 1, .86f, .35f }, { 1, 1, 1 }, { 1, .6f, .3f }, { .9f, .3f, .35f } };
  for (int i = 0; i < 34; i++) {
    double a = rnd() * 6.2831853, sp = 70 + rnd() * 260;
    emit(p->x + p->w / 2, p->y + p->h / 2, cos(a) * sp, sin(a) * sp - 140, 0.5 + rnd() * 0.5, 4 + (rand() % 3) * 2, 900, cols[rand() % 4]);
  }
}
static void dust(double x, double y, int n, double dir) {
  static const float c[3] = { .85f, .85f, .92f };
  for (int i = 0; i < n; i++)
    emit(x + (rnd() - 0.5) * 10, y, dir * (20 + rnd() * 60) + (rnd() - 0.5) * 50, -(20 + rnd() * 50), 0.28 + rnd() * 0.2, 4, 120, c);
}

static int nav(int which, int isHeld, int isPressed, double dt) {
  if (isPressed) { rep[which] = -0.4; return 1; }
  if (isHeld) { rep[which] += dt; if (rep[which] > 0.07) { rep[which] = 0; return 1; } }
  else rep[which] = 0;
  return 0;
}

static void play_events(void) {
  const Player *p = &world->p;
  double fy = world->gravity > 0 ? p->y + p->h : p->y;
  if (world->sfxJump) { sfx(SND_JUMP); world->sfxJump = 0; dust(p->x + p->w / 2, fy, 5, 0); }
  if (world->sfxPop) {
    sfx(SND_POP); world->sfxPop = 0; shake = 0.18;
    static const float wh[3] = { 1, 1, 1 };
    for (int i = 0; i < 14; i++) { double a = i * 0.4488; emit(p->x + 10, p->y + 14, cos(a) * 190, sin(a) * 190, 0.35, 4, 0, wh); }
  }
  if (world->crEv) {
    int fx = world->crEv - 1; world->crEv = 0; sfx(SND_COIN);
    static const float cy[3] = { 0.4f, 0.85f, 1.0f };
    for (int i = 0; i < 14; i++) { double a = i * 0.4488; emit(p->x + 10, p->y + 6, cos(a) * 140, sin(a) * 140 - 30, 0.5, 4, 0, cy); }
    if (fx == 0) revealT = 6;
    else if (fx == 2 && level.nclaims > 0) { int ci = world->crI >= 0 ? world->ents[world->crI].fxc : 0; toastClaim = (ci >= 0 && ci < level.nclaims) ? ci : 0; toastT = 5; }
  }
  if (world->newCoins > 0) {
    static const float gd[3] = { 1.0f, 0.85f, 0.3f };
    coinsHeld += world->newCoins; world->newCoins = 0; sfx(SND_COIN);
    for (int i = 0; i < 8; i++) { double a = i * 0.785; emit(world->coinX, world->coinY, cos(a) * 110, sin(a) * 110 - 40, 0.4, 3, 0, gd); }
  }
}

/* a finished level goes straight on to the next one; what it claimed stays available in the R1 guide */
static void level_won(void) {
  sfx(SND_WIN);
  if (level.kind == 0 && level.nclaims > 0) { memcpy(lastClaims, level.claims, sizeof lastClaims); lastN = level.nclaims; lastIdx = idx; }
  next_level();
  flashT = 0.45;
}

static void update_play(double dt) {
  lvlT += dt;
  if (level.kind == 1) {
    if (screen_update(dt)) level_won();
    return;
  }
  if (pr.start) paused = !paused;
  if (paused) {
    if (pr.select && !held.start) { state = ST_TITLE; paused = 0; }
    return;
  }
  if (levelDeaths >= 8 && pr.select && !held.start) { sfx(SND_TICK); next_level(); return; }
  if (deadT > 0) {
    deadT -= dt;
    if (deadT <= 0) { world_init(world, &level); wasGround = 1; squash = 0; revealT = toastT = 0; }
    return;
  }
  if (revealT > 0) revealT -= dt;
  if (toastT > 0) toastT -= dt;
  Held h = { held.left, held.right, held.jump };
  prevVy = world->p.vy;
  world_update(world, dt, &h, pr.jump);
  {
    const Player *p = &world->p;
    double fy = world->gravity > 0 ? p->y + p->h : p->y;
    if (p->ground && !wasGround && prevVy * world->gravity > 260) {
      double sq = prevVy * world->gravity / 3600.0; squash = sq > 0.2 ? 0.2 : sq;
      dust(p->x + p->w / 2, fy, 7, 0);
    }
    if (p->ground && fabs(p->vx) > 120) {
      runT -= dt;
      if (runT <= 0) { runT = 0.08; dust(p->x + p->w / 2, fy, 1, p->vx > 0 ? -1 : 1); }
    }
    wasGround = p->ground;
  }
  play_events();
  if (world->dead) {
    deaths++; levelDeaths++; coinsHeld = 0; sfx(SND_DIE); burst(&world->p); deadT = 0.55; shake = 0.3;
  } else if (world->won) {
    level_won();
  }
}

static void game_update(double dt) {
  if (dt > 0.05) dt = 0.05;
  gtime += dt; parts_update(dt);
  if (squash > 0) { squash -= dt * 1.1; if (squash < 0) squash = 0; }
  if (shake > 0) { shake -= dt; if (shake < 0) shake = 0; }
  if (held.start && held.select) { quitT += dt; if (quitT > 1) quitFlag = 1; } else quitT = 0;
  if (flashT > 0) flashT -= dt;
  if (helpOpen) { help_update(dt); return; }
  if (pr.r1 && (state == ST_BRIEF || state == ST_PLAY || state == ST_CARD)) { helpOpen = 1; helpScroll = 0; sfx(SND_TICK); return; }

  if (state == ST_TITLE) {
    if (nav(0, held.up, pr.up, dt)) { menuSel = (menuSel + 3) % 4; confirmNew = 0; sfx(SND_TICK); }
    if (nav(1, held.down, pr.down, dt)) { menuSel = (menuSel + 1) % 4; confirmNew = 0; sfx(SND_TICK); }
    if (pr.jump || pr.start) {
      if (menuSel == 0) start_level(unlocked);
      else if (menuSel == 1) { state = ST_SELECT; selCursor = idx < unlocked ? idx : unlocked; if (selCursor < 1) selCursor = 1; }
      else if (menuSel == 2) {
        if (unlocked > 1 && !confirmNew) { confirmNew = 1; sfx(SND_HURT); }
        else { unlocked = 1; deaths = 0; coinsHeld = 0; confirmNew = 0; save_game(); start_level(1); }
      } else quitFlag = 1;
    }
  } else if (state == ST_SELECT) {
    int d = 0;
    if (nav(0, held.left, pr.left, dt)) d = -1;
    if (nav(1, held.right, pr.right, dt)) d = 1;
    if (nav(2, held.up, pr.up, dt)) d = -10;
    if (nav(3, held.down, pr.down, dt)) d = 10;
    if (d) { selCursor += d; if (selCursor < 1) selCursor = 1; if (selCursor > unlocked) selCursor = unlocked; sfx(SND_TICK); }
    if (pr.jump || pr.start) start_level(selCursor);
    if (pr.select && !held.start) state = ST_TITLE;
  } else if (state == ST_BRIEF) {
    briefT += dt;
    if (briefT > 0.25 && (pr.jump || pr.start)) { state = ST_PLAY; sfx(SND_TICK); }
    if (pr.select && !held.start) state = ST_TITLE;
  } else if (state == ST_PLAY) {
    update_play(dt);
  } else if (state == ST_CARD) {
    cardT += dt;
    if (cardT > 0.4 && (pr.jump || pr.start)) next_level();
  } else if (state == ST_END) {
    if (pr.start || pr.jump) state = ST_TITLE;
  }
}

/* ============================================================== screens drawing */
static void title_text(const char *s, double y, int f, const float *c) {
  static const float ol[3] = { 0.05f, 0.03f, 0.12f }, sh[3] = { 0.55f, 0.18f, 0.3f };
  double w = text_w(f, s), x = (640 - w) / 2;
  for (int dy = -3; dy <= 3; dy += 3) for (int dx = -3; dx <= 3; dx += 3) if (dx || dy) text_pass(f, s, x + dx, y + dy, ol, 1);
  text_pass(f, s, x, y + 6, sh, 1);
  text_pass(f, s, x, y, c, 1);
}

static void ground_strip(double y0) {
  for (int c = 0; c < 20; c++) {
    SDL_Rect d = { c * 32, (int)y0, 32, 32 };
    SDL_RenderCopy(RD, A.top[hsh(c, 0, 1) & 3], NULL, &d);
    for (int r = 1; (int)y0 + r * 32 < 480; r++) {
      SDL_Rect e = { c * 32, (int)y0 + r * 32, 32, 32 };
      SDL_RenderCopy(RD, A.fill[hsh(c, r, 1) & 3], NULL, &e);
    }
  }
}

static void draw_title(void) {
  int wd = (unlocked - 1) / PER + 1;
  art_world(wd);
  draw_bg(&PALS[wd - 1], 320 + sin(gtime * 0.25) * 160, 1, 0);
  ground_strip(432);
  double px = fmod(gtime * 70, 760) - 60;
  blit_player(px, 432, 1 + ((int)(gtime * 14)) % 4, 0, 0, 1, 1);
  if (fmod(gtime * 70, 760) > 0 && ((int)(gtime * 12)) % 2 == 0) dust(px - 10, 432, 1, -1);
  parts_draw();
  title_text("DEVIL'S DETOUR", 34, 3, YEL);
  static const float s[3] = { .85f, .85f, .95f };
  center("Every level lies to you.", 108, 1, s, 1);
  char b[64];
  if (unlocked > 1) snprintf(b, sizeof b, "Continue  (level %d)", unlocked); else snprintf(b, sizeof b, "Play");
  const char *items[4] = { b, "Level select", confirmNew ? "Press again to erase progress" : "New game", "Quit" };
  panel(150, 150, 340, 200, ACC_BLUE);
  for (int n = 0; n < 4; n++) {
    int sel = (n == menuSel);
    double y = 168 + n * 44;
    if (sel) { setc(0.36f, 0.42f, 0.7f, 0.35f); frect(162, y - 4, 316, 34); setc(1, 0.86f, 0.35f, 1); frect(164, y + 4, 6, 6); }
    static const float dim[3] = { .55f, .55f, .68f };
    center(items[n], y, 2, sel ? WHITE : dim, 1);
  }
  center("D-pad: move    any face button: jump    START: pause", 392, 0, GREYT, 1);
  center("Hold START + SELECT to quit. The controls are 100% honest.", 412, 0, GREYT, 1);
  SDL_Rect v = { 0, 0, 640, 480 }; SDL_RenderCopy(RD, A.vig, NULL, &v);
}

static void draw_select(void) {
  int wd = (selCursor - 1) / PER + 1;
  art_world(wd);
  draw_bg(&PALS[wd - 1], 320, 1, 0.35);
  char b[96];
  snprintf(b, sizeof b, "WORLD %d: %s", wd, WORLD_NAMES[wd - 1]);
  center(b, 22, 2, YEL, 1);
  panel(20, 72, 600, 288, ACC_BLUE);
  int base = (wd - 1) * PER;
  for (int n = 1; n <= PER; n++) {
    int i = base + n;
    int col = (n - 1) % 10, row = (n - 1) / 10;
    double x = 38 + col * 56, y = 88 + row * 52;
    int open = i <= unlocked;
    if (i == selCursor) { setc(1, 0.86f, 0.35f, 1); frect(x - 4, y - 4, 52, 44); }
    if (open) { setc(0.2f, 0.28f, 0.45f, 1); frect(x, y, 44, 36); setc(0.3f, 0.4f, 0.62f, 1); frect(x, y, 44, 3); }
    else { setc(0.1f, 0.1f, 0.15f, 1); frect(x, y, 44, 36); }
    float c[3];
    if (i == selCursor) { c[0] = c[1] = c[2] = 0.05f; }
    else if (open) { c[0] = c[1] = c[2] = 1; }
    else { c[0] = c[1] = .35f; c[2] = .4f; }
    if (i % 10 == 0 && open) { if (i == selCursor) { c[0] = c[1] = c[2] = 0.05f; } else { c[0] = 1; c[1] = .7f; c[2] = .4f; } }
    char num[16]; snprintf(num, sizeof num, "%d", i);
    text_box(0, num, x, y + 10, 44, 1, c, 1);
  }
  snprintf(b, sizeof b, "Level %d of %d  (unlocked: %d)", selCursor, COUNT, unlocked);
  center(b, 378, 1, WHITE, 1);
  center("D-pad: choose    A/B: play    SELECT: back", 432, 0, GREYT, 1);
  }

static void draw_hud(void) {
  char b[96];
  snprintf(b, sizeof b, "%d", idx);
  double tw = text_w(1, b) + 64;
  panel(320 - tw / 2, 4, tw, 32, ACC_BLUE);
  icon(ICON_FLAG, 320 - tw / 2 + 14, 8, 3);
  text_at(1, b, 320 - tw / 2 + 46, 8, WHITE, 1);
  /* progress bar, bottom left */
  setc(0.1f, 0.11f, 0.18f, 0.9f); frect(10, 458, 112, 10);
  setc(1, 0.86f, 0.35f, 1); frect(12, 460, 108.0 * idx / COUNT, 6);
  setc(0.36f, 0.42f, 0.7f, 1); orect(10, 458, 112, 10);
  /* coins you carry until you die: coin pile + number, top right */
  snprintf(b, sizeof b, "%d", coinsHeld);
  double cw = text_w(0, b);
  icon(IC_COINS, 628 - cw - 30, 6, 3);
  text_box(0, b, 0, 10, 628, 2, GOLD, 1);
  /* death counter: skull + number (the number may lie) */
  snprintf(b, sizeof b, "%d", deaths + level.deathOff);
  double dw = text_w(0, b);
  icon(ICON_DIE, 628 - dw - 26, 452, 2);
  text_box(0, b, 0, 456, 628, 2, WHITE, 1);
  if (levelDeaths >= 8) { static const float o[3] = { 1, .8f, .4f }; center("SELECT: skip this level", 434, 0, o, 1); }
  else if (idx <= 3 && lvlT < 6 && level.kind == 0) text_box(0, "R1: what the level says + icon guide", 0, 44, 640, 1, GREYT, lvlT < 5 ? 1 : 6 - lvlT);

  if (world->slowT > 0 && level.kind == 0) {   /* crystal: the clock crawls */
    setc(0.2f, 0.45f, 1, 0.10f); frect(0, 0, 640, 480);
    icon(14, 12, 44, 3);
    setc(0.1f, 0.11f, 0.18f, 0.9f); frect(40, 52, 60, 8);
    setc(0.4f, 0.7f, 1, 1); frect(42, 54, 56.0 * world->slowT / 4.0, 4);
    setc(0.36f, 0.42f, 0.7f, 1); orect(40, 52, 60, 8);
  }
  if (toastT > 0 && level.kind == 0) {   /* crystal: one claim, true or not */
    const Claim *c = &level.claims[toastClaim];
    int tr = !c->lie; double gw = glyph_w(c->id, c->num, 3);
    double pw = 56 + gw + 24 + 120, px = 320 - pw / 2;
    panel(px, 44, pw, 54, tr ? ACC_OK : ACC_BAD);
    icon(32, px + 10, 58, 3);
    glyph(c->id, c->ass, c->num, px + 46, 58, 3);
    static const float gr[3] = { 0.5f, 1, 0.55f }, rd[3] = { 1, 0.4f, 0.4f };
    if (tr) { draw_tick(px + 56 + gw + 22, 56, 26, gr); text_at(1, "TRUE", px + 56 + gw + 54, 58, GREEN, 1); }
    else { draw_cross(px + 56 + gw + 22, 56, 26, rd); text_at(1, "LIE", px + 56 + gw + 54, 58, REDT, 1); }
  }
}

/* the briefing: everything the level wants you to believe, as pictures */
static void draw_brief(void) {
  art_world(level.world);
  draw_bg(&PALS[level.world - 1], 320 + sin(gtime * 0.3) * 100, 1, 0.5);
  char b[96];
  snprintf(b, sizeof b, "%d", idx);
  double tw = text_w(2, b);
  icon(ICON_GENERIC, 320 - tw / 2 - 48, 8, 4);
  title_text(b, 8, 2, YEL);
  snprintf(b, sizeof b, "World %d: %s", level.world, WORLD_NAMES[level.world - 1]);
  center(b, 48, 0, GREYT, 1);
  int n = level.nclaims;
  for (int i = 0; i < n; i++) {
    const Claim *c = &level.claims[i];
    int col = i % 4, row = i / 4;
    double tx = 24 + col * 148, ty = 72 + row * 84;
    panel(tx, ty, 140, 76, ACC_BLUE);
    double gw = glyph_w(c->id, c->num, 5);
    glyph(c->id, c->ass, c->num, tx + (140 - gw) / 2, ty + 18, 5);
  }
  /* legend */
  icon(11, 150, 420, 3); badge(1, 150, 420, 24);
  text_at(0, "= happens", 182, 426, GREYT, 1);
  icon(11, 360, 420, 3); badge(0, 360, 420, 24);
  text_at(0, "= does not happen", 392, 426, GREYT, 1);
  text_at(0, "R1: icon guide", 24, 452, GREYT, 1);
  if (briefT > 0.25) abutton(320, 456, 3, 0.85 + 0.15 * sin(briefT * 5));
}

static void draw_card(void) {
  if (level.kind == 0) world_draw(world, 1);
  else { art_world(level.world); draw_bg(&PALS[level.world - 1], 320, 1, 0); }
  setc(0, 0, 0.03f, 0.8f); frect(0, 0, 640, 480);
  char b[128];
  static const float gr[3] = { .5f, .95f, .55f };
  snprintf(b, sizeof b, "%d", idx);
  double tw = text_w(2, b);
  icon(13, 320 - tw / 2 - 48, 6, 4);
  title_text(b, 8, 2, gr);
  int nt = level.nclaims - level.nlies;
  draw_cross(250, 46, 26, C_RED);
  snprintf(b, sizeof b, "%d", level.nlies); text_at(1, b, 282, 46, WHITE, 1);
  draw_tick(340, 46, 26, C_GRN);
  snprintf(b, sizeof b, "%d", nt); text_at(1, b, 372, 46, WHITE, 1);
  int n = level.nclaims;
  int rows = (n + 1) / 2;
  panel(10, 80, 306, rows * 42 + 12, ACC_BLUE);
  panel(324, 80, 306, rows * 42 + 12, ACC_BLUE);
  for (int i = 0; i < n; i++) {
    const Claim *c = &level.claims[i];
    int col = i / rows, row = i % rows;
    double x = (col ? 340 : 26), y = 86 + row * 42 + 6;
    glyph(c->id, c->ass, c->num, x, y, 3);
    if (c->lie) draw_cross(x + 112, y, 24, C_RED); else draw_tick(x + 112, y, 24, C_GRN);
    if (c->lie) {
      setc(0.6f, 0.62f, 0.75f, 1);
      thick_line(x + 146, y + 12, x + 160, y + 12, 2);
      ftri(x + 166, y + 12, x + 158, x + 158, y + 12);   /* small arrowhead */
      setc(0.6f, 0.62f, 0.75f, 1); frect(x + 158, y + 8, 4, 8);
      int rn = c->num >= 0 ? c->realnum : -1;
      glyph(c->id, !c->ass, rn, x + 176, y, 3);
    }
  }
  if (level.kind == 0) {
    double yy = 86 + rows * 42 + 26;
    icon(15, 150, yy, 2);
    snprintf(b, sizeof b, "%d / %d", levelDeaths, deaths); text_at(1, b, 176, yy - 2, WHITE, 1);
    icon(17, 340, yy, 2);
    snprintf(b, sizeof b, "%d", deaths + level.deathOff); text_at(1, b, 366, yy - 2, WHITE, 1);
  }
  if (cardT > 0.4) abutton(320, 452, 3, 0.85 + 0.15 * sin(cardT * 5));
}

static void draw_play(void) {
  if (shake > 0) {
    int m = (int)(shake * 28);
    SDL_Rect vp = { rand() % (2 * m + 1) - m, rand() % (2 * m + 1) - m, 640, 480 };
    SDL_RenderSetViewport(RD, &vp);
  }
  if (level.kind == 1) screen_draw();
  else {
    world_draw(world, deadT > 0);
    parts_draw();
  }
  if (shake > 0) SDL_RenderSetViewport(RD, NULL);
  if (level.kind == 1) return;
#ifdef TRUST_TEST
  if (getenv("TRUST_NOHUD")) return;
#endif
  draw_hud();
  if (paused) {
    setc(0, 0, 0, 0.7f); frect(0, 0, 640, 480);
    panel(120, 150, 400, 190, ACC_BLUE);
    center("PAUSED", 168, 3, WHITE, 1);
    center("START: resume      SELECT: back to menu      R1: icon guide", 270, 0, GREYT, 1);
    center("Hold START + SELECT: quit game", 295, 0, GREYT, 1);
  }
}

static void draw_end(void) {
  art_world(WORLDS); draw_bg(&PALS[WORLDS - 1], 320 + sin(gtime * 0.3) * 120, 1, 0.25);
  title_text("THE END", 80, 3, YEL);
  center("(For real this time.)", 170, 1, GREYT, 1);
  char b[96];
  snprintf(b, sizeof b, "You survived %d levels of lies.", COUNT);
  center(b, 240, 2, WHITE, 1);
  snprintf(b, sizeof b, "Deaths: %d", deaths);
  center(b, 290, 2, REDT, 1);
  center("This message is the only honest thing in the game.", 360, 0, GREYT, 1);
  center("Press START", 420, 1, WHITE, 1);
}

/* ============================================================== icon guide (R1) */
typedef struct { int ic; const char *name, *means, *hint; } HelpE;
enum { HI_TICK = -2, HI_SLASH = -3, HI_CARD = -4, HI_SIGN = -5, HI_DOORS = -6, HI_COIN = -7, HI_BAR = -8, HI_GATE = -9 };
static const HelpE HELP[] = {
  /* how to read */
  { HI_TICK, "Green tick", "Beside an icon: this happens in this level.", "Any statement can be a lie. The LAST LEVEL section shows what really happened." },
  { HI_SLASH, "Red slash", "Beside an icon: this does NOT happen.", "A slash can lie too." },
  { HI_CARD, "Red cross / green tick", "In LAST LEVEL: a red cross means that claim lied, a green tick means it was true.", "An arrow shows what really happened." },
  { 14, "The clock", "Controls, speed, gravity, floors, ceilings, spikes and the exit flip on a repeating timer from the first frame.", "Learn the beat, then move on a safe moment." },
  /* claims */
  { 0, "Hidden spikes", "Spikes pop up out of the floor.", "They pop on the clock, not when you step close. Wait for them to sink." },
  { 1, "Exit direction", "Says which way the exit is.", "Signs can point the wrong way. Look for a flag." },
  { 2, "Flipped controls", "Left and right swap places.", "It toggles with the clock. Stand still when it flips." },
  { 3, "Deadly coins", "Coins kill you.", "Coins look the same either way. The harmless ones add to your counter." },
  { 4, "Crumbling floor", "Floors fall apart under you.", "Do not stop on shaky ground." },
  { 5, "Gravity flip", "Down becomes up.", "A ceiling catches you. It flips back later." },
  { 6, "Running exit", "The flag moves away.", "It hops on a schedule. Wait where it lands next." },
  { 7, "Fake flag", "A flag is not a real exit.", "Every flag looks the same, but only some of them win. The others kill you." },
  { 8, "Invisible floor", "You can walk on thin air over some pits.", "It may blink. Cross when it is solid, or jump a real pit." },
  { 9, "Speed change", "You run faster for a while.", "Let go early near edges." },
  { 10, "Painted spikes", "Some spikes are harmless.", "Guessing wrong still hurts." },
  { 11, "Spikes", "There are spikes in the level.", "Jump them. Some are fake." },
  { 12, "Pits", "There are pits to fall into.", "Some have an invisible floor. Some do not." },
  { 17, "Death counter", "The corner counter tells the truth.", "Treat the number as a rumour." },
  { 18, "Level number", "Says which level this is (the flag at the top).", "Do not count on it." },
  { 19, "Spike count", "Gives an exact number of spikes.", "Counting does not help much." },
  { 30, "High exit", "The exit is higher than the start.", "Look for ledges." },
  { 31, "Torch", "There is a torch in the level. A lit torch shows hidden things near it: invisible floors, hidden spikes, armed ceilings.", "It goes out and relights. About half of the torches lie and show it a tile off or at the wrong time." },
  { 33, "Coins", "There are coins here. The counter at the top right shows the ones you carry.", "You lose them all when you die." },
  { 34, "Jump kills", "Jumping will kill you: spikes hide in the ceiling.", "The ceiling is armed only part of the time. Walk, or jump while it is off." },
  { 35, "Gate", "A gate sends you somewhere else in the level.", "Gates look like a glowing ring. A gate may teleport you (maybe back near the start) or kill you." },
  /* trick screens */
  { 22, "Button", "A trick screen wants a button.", "Not always the one it names." },
  { 23, "START", "START button.", "" },
  { 24, "Heart", "Mistakes cost lives.", "" },
  { 25, "Sound", "Sound matters on this screen.", "" },
  { 26, "Platform level", "This is a normal platform level.", "" },
  { 27, "Question mark", "This screen has a trick.", "" },
  { 28, "Reset", "Failing sends you back.", "" },
  { 29, "Star", "A button that solves everything.", "" },
  { 13, "Last level", "The next level is the final one.", "" },
  { 15, "Skull", "You can die here. The corner counter is the death count (it may lie).", "" },
  /* things in the world */
  { HI_SIGN, "Exit sign", "Green sign with a runner and an arrow.", "It points where it wants. Look for the flag yourself." },
  { HI_DOORS, "Flags", "Checkered flags. Every flag looks exactly the same.", "One of them is the exit. The others kill you." },
  { HI_GATE, "Gates", "A glowing ring, not a flag.", "A gate never wins. It throws you elsewhere or kills you. Beside a wall it works only at the right moment: retry." },
  { HI_COIN, "Gold coin", "Pick it up: +1 on the counter at the top right.", "You lose them all when you die. Some coins are deadly." },
  { HI_BAR, "Progress bar", "Bottom left: how far you are through all the levels.", "" },
};
#define NHELP ((int)(sizeof HELP / sizeof *HELP))

static int help_wrap(int f, const char *s, double maxw, char out[][100], int maxl) {
  int n = 0; char line[100]; line[0] = 0;
  const char *p = s;
  while (*p && n < maxl) {
    char word[64]; int wl = 0;
    while (*p && *p != ' ' && wl < 62) word[wl++] = *p++;
    word[wl] = 0; while (*p == ' ') p++;
    char tryl[170]; snprintf(tryl, sizeof tryl, "%s%s%s", line, line[0] ? " " : "", word);
    if (line[0] && text_w(f, tryl) > maxw) { snprintf(out[n++], 100, "%s", line); snprintf(line, sizeof line, "%s", word); }
    else snprintf(line, sizeof line, "%s", tryl);
  }
  if (line[0] && n < maxl) snprintf(out[n++], 100, "%s", line);
  return n;
}

static int help_here(const HelpE *e) {
  if (e->ic < 0 || e->ic == 14) return 0;
  for (int k = 0; k < level.nclaims; k++) if (level.claims[k].id == e->ic) return 1;
  return 0;
}

static void help_icon(int ic, double x, double y) {
  if (ic >= 0) { icon(ic, x, y, 3); return; }
  if (ic == HI_TICK) { icon(11, x, y, 3); badge(1, x, y, 24); }
  else if (ic == HI_SLASH) { icon(11, x, y, 3); badge(0, x, y, 24); }
  else if (ic == HI_CARD) { draw_cross(x, y + 2, 11, REDT); static const float g[3] = { .4f, 1, .5f }; draw_tick(x + 12, y + 8, 11, g); }
  else if (ic == HI_SIGN) { SDL_Rect d = { (int)x - 6, (int)y + 2, 36, 15 }; SDL_RenderCopy(RD, A.sgExit, NULL, &d); }
  else if (ic == HI_DOORS) { SDL_Rect d = { (int)x + 4, (int)y - 6, 16, 24 + 12 }; SDL_RenderCopy(RD, A.door[0], NULL, &d); }
  else if (ic == HI_GATE) { SDL_Rect d = { (int)x, (int)y, 24, 24 }; SDL_RenderCopy(RD, A.door[3], NULL, &d); }
  else if (ic == HI_COIN) { SDL_Rect d = { (int)x, (int)y, 24, 24 }; SDL_RenderCopy(RD, A.coin[0], NULL, &d); }
  else if (ic == HI_BAR) { setc(0.1f, 0.11f, 0.18f, 0.9f); frect(x, y + 8, 28, 8); setc(1, 0.86f, 0.35f, 1); frect(x + 2, y + 10, 14, 4); setc(0.36f, 0.42f, 0.7f, 1); orect(x, y + 8, 28, 8); }
}

/* rows: section titles and entries; returns total height and (optionally) draws */
static void help_head(const char *s, double y) {
  text_at(1, s, 24, y, YEL, 1); setc(0.36f, 0.42f, 0.7f, 1); frect(24, y + 22, 592, 2);
}
static void help_pass(int draw) {
  char buf[8][100];
  double y = 70 - helpScroll;
  if (level.kind == 0 && level.nclaims > 0) {   /* what this level wants you to believe */
    char hb2[64]; snprintf(hb2, sizeof hb2, "LEVEL %d SAYS", idx);
    if (draw && y > 40 && y < 450) help_head(hb2, y);
    y += 32;
    int n = level.nclaims;
    for (int i = 0; i < n; i++) {
      const Claim *c = &level.claims[i];
      double tx = 24 + (i % 4) * 148, ty = y + (i / 4) * 84;
      if (draw && ty + 76 > 52 && ty < 452) { panel(tx, ty, 140, 76, ACC_BLUE); double gw = glyph_w(c->id, c->num, 5); glyph(c->id, c->ass, c->num, tx + (140 - gw) / 2, ty + 18, 5); }
    }
    y += ((n + 3) / 4) * 84;
    if (draw && y > 40 && y < 450) {
      icon(11, 24, y, 2); badge(1, 24, y, 16); text_at(0, "= happens", 48, y + 2, GREYT, 1);
      icon(11, 190, y, 2); badge(0, 190, y, 16); text_at(0, "= does not happen. Any of it can be a lie.", 214, y + 2, GREYT, 1);
    }
    y += 34;
  }
  if (lastN > 0) {   /* the card of the previous level */
    char hb2[64]; snprintf(hb2, sizeof hb2, "LAST LEVEL (%d): WHAT LIED", lastIdx);
    if (draw && y > 40 && y < 450) help_head(hb2, y);
    y += 32;
    int rows = (lastN + 1) / 2;
    for (int i = 0; i < lastN; i++) {
      const Claim *c = &lastClaims[i];
      int col = i / rows, row = i % rows;
      double x = col ? 330 : 24, yy = y + row * 42;
      if (!(draw && yy + 34 > 52 && yy < 452)) continue;
      glyph(c->id, c->ass, c->num, x, yy, 3);
      if (c->lie) draw_cross(x + 112, yy, 24, C_RED); else draw_tick(x + 112, yy, 24, C_GRN);
      if (c->lie) {
        setc(0.6f, 0.62f, 0.75f, 1); thick_line(x + 146, yy + 12, x + 160, yy + 12, 2);
        ftri(x + 166, yy + 12, x + 158, x + 158, yy + 12);
        setc(0.6f, 0.62f, 0.75f, 1); frect(x + 158, yy + 8, 4, 8);
        glyph(c->id, !c->ass, c->num >= 0 ? c->realnum : -1, x + 176, yy, 3);
      }
    }
    y += rows * 42 + 14;
  }
  int hereN = 0; for (int i = 0; i < NHELP; i++) if (help_here(&HELP[i])) hereN++;
  static const char *SEC[3] = { "HOW TO READ", "IN THIS LEVEL", "EVERYTHING ELSE" };
  int order[3][NHELP], cnt[3] = { 0, 0, 0 };
  for (int i = 0; i < NHELP; i++) {
    int s = (i < 4) ? 0 : (help_here(&HELP[i]) ? 1 : 2);
    order[s][cnt[s]++] = i;
  }
  (void)hereN;
  for (int s = 0; s < 3; s++) {
    if (!cnt[s]) continue;
    if (draw && y > 40 && y < 450) help_head(SEC[s], y);
    y += 32;
    for (int q = 0; q < cnt[s]; q++) {
      const HelpE *e = &HELP[order[s][q]];
      int nm = help_wrap(0, e->means, 470, buf, 3);
      char hb[8][100]; int nh = e->hint[0] ? help_wrap(0, e->hint, 470, hb, 3) : 0;
      double h = 22 + nm * 16 + nh * 16 + 12; if (h < 56) h = 56;
      if (draw && y + h > 40 && y < 450) {
        help_icon(e->ic, 30, y + 6);
        text_at(1, e->name, 100, y, WHITE, 1);
        if (s == 1) text_at(0, "HERE", 560, y + 3, YEL, 1);
        for (int k = 0; k < nm; k++) text_at(0, buf[k], 100, y + 22 + k * 16, WHITE, 0.92);
        for (int k = 0; k < nh; k++) text_at(0, hb[k], 100, y + 22 + (nm + k) * 16, GREYT, 1);
        setc(0.36f, 0.42f, 0.7f, 0.35f); frect(24, y + h - 4, 592, 1);
      }
      y += h;
    }
  }
  helpTotal = y + helpScroll - 70;
}

static void draw_help(void) {
  setc(0.02f, 0.025f, 0.06f, 0.96f); frect(0, 0, 640, 480);
  help_pass(1);
  /* header and footer cover the scrolling list */
  setc(0.02f, 0.025f, 0.06f, 1); frect(0, 0, 640, 52); frect(0, 452, 640, 28);
  setc(0.36f, 0.42f, 0.7f, 1); frect(0, 52, 640, 2); frect(0, 452, 640, 2);
  char b[64]; snprintf(b, sizeof b, "ICON GUIDE   level %d", idx);
  text_box(1, b, 0, 14, 640, 1, WHITE, 1);
  text_box(0, "UP / DOWN: scroll    LEFT / RIGHT: page    R1 or A: close", 0, 458, 640, 1, GREYT, 1);
  /* scroll bar */
  double view = 398, maxs = helpTotal - view; if (maxs < 1) maxs = 1;
  double th = view * view / (helpTotal > view ? helpTotal : view); if (th < 24) th = 24;
  setc(0.15f, 0.17f, 0.3f, 1); frect(630, 56, 4, view);
  setc(1, 0.86f, 0.35f, 1); frect(630, 56 + (view - th) * (helpScroll / maxs), 4, th);
}

static void help_update(double dt) {
  double sp = 0;
  if (held.down) sp += 420 * dt;
  if (held.up) sp -= 420 * dt;
  if (pr.right) sp += 300; if (pr.left) sp -= 300;
  helpScroll += sp;
  double maxs = helpTotal - 398; if (maxs < 0) maxs = 0;
  if (helpScroll > maxs) helpScroll = maxs;
  if (helpScroll < 0) helpScroll = 0;
  if (pr.r1 || pr.jump || pr.start || pr.select) { helpOpen = 0; sfx(SND_TICK); }
}

static void draw_all(void) {
  SDL_SetRenderDrawColor(RD, 0, 0, 0, 255);
  SDL_RenderClear(RD);
  switch (state) {
    case ST_TITLE: draw_title(); break;
    case ST_SELECT: draw_select(); break;
    case ST_BRIEF: draw_brief(); break;
    case ST_PLAY: draw_play(); break;
    case ST_CARD: draw_card(); break;
    default: draw_end(); break;
  }
  if (helpOpen) draw_help();
  if (flashT > 0) { setc(1, 1, 1, (float)(flashT / 0.45 * 0.7)); frect(0, 0, 640, 480); }
  if (quitT > 0.05) { setc(1, 0.4f, 0.4f, 1); frect(0, 474, 640 * (quitT > 1 ? 1 : quitT), 6); }
}

/* ============================================================== input */
static SDL_GameController *gc[4];
static SDL_Joystick *js[4];

static void open_pads(void) {
  int n = SDL_NumJoysticks();
  for (int i = 0; i < n && i < 4; i++) {
    if (gc[i] || js[i]) continue;
    if (SDL_IsGameController(i)) gc[i] = SDL_GameControllerOpen(i);
    else js[i] = SDL_JoystickOpen(i);
  }
}

static Keys sample(void) {
  Keys k; memset(&k, 0, sizeof k);
  const Uint8 *ks = SDL_GetKeyboardState(NULL);
  k.left = ks[SDL_SCANCODE_LEFT]; k.right = ks[SDL_SCANCODE_RIGHT];
  k.up = ks[SDL_SCANCODE_UP]; k.down = ks[SDL_SCANCODE_DOWN];
  k.a = ks[SDL_SCANCODE_Z]; k.b = ks[SDL_SCANCODE_X];
  k.jump = ks[SDL_SCANCODE_Z] || ks[SDL_SCANCODE_X] || ks[SDL_SCANCODE_C] || ks[SDL_SCANCODE_SPACE];
  k.start = ks[SDL_SCANCODE_RETURN];
  k.select = ks[SDL_SCANCODE_ESCAPE] || ks[SDL_SCANCODE_RSHIFT];
  k.r1 = ks[SDL_SCANCODE_R] || ks[SDL_SCANCODE_TAB]; k.l1 = ks[SDL_SCANCODE_L];
  for (int i = 0; i < 4; i++) {
    if (gc[i]) {
      SDL_GameController *c = gc[i];
#define BTN(b) SDL_GameControllerGetButton(c, b)
      int lx = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
      int ly = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
      k.left |= BTN(SDL_CONTROLLER_BUTTON_DPAD_LEFT) || lx < -16000;
      k.right |= BTN(SDL_CONTROLLER_BUTTON_DPAD_RIGHT) || lx > 16000;
      k.up |= BTN(SDL_CONTROLLER_BUTTON_DPAD_UP) || ly < -16000;
      k.down |= BTN(SDL_CONTROLLER_BUTTON_DPAD_DOWN) || ly > 16000;
      int fa = BTN(SDL_CONTROLLER_BUTTON_A), fb = BTN(SDL_CONTROLLER_BUTTON_B);
      k.jump |= fa || fb || BTN(SDL_CONTROLLER_BUTTON_X) || BTN(SDL_CONTROLLER_BUTTON_Y);
      k.a |= fa; k.b |= fb;
      k.start |= BTN(SDL_CONTROLLER_BUTTON_START);
      k.select |= BTN(SDL_CONTROLLER_BUTTON_BACK);
      k.r1 |= BTN(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER); k.l1 |= BTN(SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
#undef BTN
    } else if (js[i]) {
      SDL_Joystick *j = js[i];
      Uint8 hat = SDL_JoystickNumHats(j) > 0 ? SDL_JoystickGetHat(j, 0) : 0;
      int ax = SDL_JoystickNumAxes(j) > 0 ? SDL_JoystickGetAxis(j, 0) : 0;
      int ay = SDL_JoystickNumAxes(j) > 1 ? SDL_JoystickGetAxis(j, 1) : 0;
      k.left |= (hat & SDL_HAT_LEFT) != 0 || ax < -16000;
      k.right |= (hat & SDL_HAT_RIGHT) != 0 || ax > 16000;
      k.up |= (hat & SDL_HAT_UP) != 0 || ay < -16000;
      k.down |= (hat & SDL_HAT_DOWN) != 0 || ay > 16000;
      int nb = SDL_JoystickNumButtons(j);
      for (int b = 0; b < 4 && b < nb; b++) if (SDL_JoystickGetButton(j, b)) { k.jump = 1; if (b == 0) k.a = 1; if (b == 1) k.b = 1; }
      if (nb > 7 && SDL_JoystickGetButton(j, 7)) k.start = 1;
      if (nb > 6 && SDL_JoystickGetButton(j, 6)) k.select = 1;
      if (nb > 5 && SDL_JoystickGetButton(j, 5)) k.r1 = 1;
      if (nb > 4 && SDL_JoystickGetButton(j, 4)) k.l1 = 1;
    }
  }
  return k;
}

static void compute_pressed(void) {
  pr.left = held.left && !prevh.left; pr.right = held.right && !prevh.right;
  pr.up = held.up && !prevh.up; pr.down = held.down && !prevh.down;
  pr.jump = held.jump && !prevh.jump; pr.start = held.start && !prevh.start;
  pr.select = held.select && !prevh.select; pr.a = held.a && !prevh.a; pr.b = held.b && !prevh.b;
  pr.r1 = held.r1 && !prevh.r1; pr.l1 = held.l1 && !prevh.l1;
  prevh = held;
}

/* ============================================================== test tooling */
#ifdef TRUST_TEST

/* overlap audit: no two static objects (coins, spikes, torches, flags, gates, signs, hidden spikes, running-exit spots) may share space,
 * and none may sit inside a platform (touching the top of one is fine). A flag is drawn 48 px tall, so its upper half counts too. */
static int overlap_level(const Level *L, int verbose) {
  int occ[15][20], bad = 0; memset(occ, 0, sizeof occ);
  static const char *OBJ = "^vfF*cTqDOGZL";
  #define SOL(r, c) ((r) >= 0 && (r) < 15 && (c) >= 0 && (c) < 20 && (L->map[r][c] == '#' || L->map[r][c] == '=' || L->map[r][c] == 'n'))
  #define COMPLAIN(...) do { bad++; if (verbose) { printf("  L%d: ", L->idx); printf(__VA_ARGS__); printf("\n"); } } while (0)
  for (int r = 0; r < 15; r++) for (int c = 0; c < 20; c++) {
    char ch = L->map[r][c];
    if (!strchr(OBJ, ch) || !ch) continue;
    occ[r][c]++;
    if (strchr("DG", ch)) {   /* tall flag: its top half spills into the cell above */
      if (r > 0 && (SOL(r - 1, c) || (L->map[r - 1][c] && strchr(OBJ, L->map[r - 1][c])))) COMPLAIN("flag at (%d,%d) overlaps the cell above", c, r);
    }
  }
  for (int i = 0; i < L->npops; i++) {
    int col = L->mir ? 19 - L->pops[i].col : L->pops[i].col, r = L->pops[i].top - 1;
    if (r >= 0 && r < 15 && col >= 0 && col < 20) { if (occ[r][col]) COMPLAIN("hidden spike at (%d,%d) shares a cell", col, r); occ[r][col]++; }
  }
  for (int i = 0; i < L->nflee; i++) {   /* the exit hops between spots: each spot must be free too */
    int col = L->flee[i].col, r = (int)(L->flee[i].y / 32);
    for (int r2 = 0; r2 < 15; r2++) for (int c2 = 0; c2 < 20; c2++) (void)r2, (void)c2;
    if (col >= 0 && col < 20 && r >= 0 && r < 15) {
      if (SOL(r, col)) COMPLAIN("running exit spot (%d,%d) inside a platform", col, r);
      if (r > 0 && SOL(r - 1, col)) COMPLAIN("running exit spot (%d,%d): flag top inside a platform", col, r);
      if (L->map[r][col] && strchr("^vfF*cTq", L->map[r][col])) COMPLAIN("running exit spot (%d,%d) on an object", col, r);
      if (r > 0 && L->map[r - 1][col] && strchr(OBJ, L->map[r - 1][col])) COMPLAIN("running exit spot (%d,%d): flag top overlaps an object", col, r);
    }
  }
  for (int r = 0; r < 15; r++) for (int c = 0; c < 20; c++) {
    if (occ[r][c] > 1) COMPLAIN("two objects in cell (%d,%d)", c, r);
    if (occ[r][c] && SOL(r, c)) COMPLAIN("object inside a platform at (%d,%d)", c, r);
  }
  for (int s = 0; s < L->nsigns; s++) {
    const Sign *sg = &L->signs[s]; int w = 88, h = 36;
    if (sg->kind == SG_CHEV) { w = 56; h = 20; } else if (sg->kind == SG_DECOY) { w = 56; h = 36; } else if (sg->kind == SG_Q) { w = 32; h = 32; }
    double x0 = sg->cx - w / 2.0, x1 = sg->cx + w / 2.0, y0 = sg->y, y1 = sg->y + h;
    for (int r = 0; r < 15; r++) for (int c = 0; c < 20; c++) {
      int has = occ[r][c] || SOL(r, c); if (!has) continue;
      if (x0 < c * 32 + 32 && x1 > c * 32 && y0 < r * 32 + 32 && y1 > r * 32) COMPLAIN("sign kind %d overlaps cell (%d,%d)", sg->kind, c, r);
    }
    for (int s2 = s + 1; s2 < L->nsigns; s2++) {
      const Sign *o = &L->signs[s2]; int w2 = 88, h2 = 36;
      if (o->kind == SG_CHEV) { w2 = 56; h2 = 20; } else if (o->kind == SG_DECOY) { w2 = 56; h2 = 36; } else if (o->kind == SG_Q) { w2 = 32; h2 = 32; }
      if (x0 < o->cx + w2 / 2.0 && x1 > o->cx - w2 / 2.0 && y0 < o->y + h2 && y1 > o->y) COMPLAIN("two signs overlap");
    }
  }
  #undef SOL
  #undef COMPLAIN
  return bad;
}
static int overlap_all(int verbose) {
  static Level L; int levels = 0, total = 0;
  for (int i = 1; i <= COUNT; i++) { level_get(i, &L); if (L.kind != 0) continue; int b = overlap_level(&L, verbose); if (b) { levels++; total += b; } }
  printf("overlap: %d levels with problems, %d problems in total\n", levels, total);
  return total;
}

static void sign_center_stats(void) {
  static Level L; int withSign = 0, freeCenter = 0, freeAnyY = 0; int hist[16] = {0};
  for (int i = 1; i <= COUNT; i++) {
    level_get(i, &L); if (L.kind != 0) continue;
    int has = 0; for (int s = 0; s < L.nsigns; s++) if (L.signs[s].kind == SG_EXIT) has = 1;
    if (!has) continue; withSign++;
    /* free test for the exit sign + chevrons block centred at x=320: cols 8..11 (x 256..384 covers the sign width 88 -> 276..364 => cols 8..11) */
    int ok = 1; for (int r = 6; r <= 8; r++) for (int c = 8; c <= 11; c++) { char ch = L.map[r][c]; if (ch != '.' ) ok = 0; }
    freeCenter += ok;
    int topmin = 99; for (int c = 8; c <= 11; c++) { int r = 0; while (r < 15 && L.map[r][c] != '#' && L.map[r][c] != '=' && L.map[r][c] != 'n') r++; if (r < topmin) topmin = r; }
    hist[topmin > 15 ? 15 : topmin]++;
  }
  printf("levels with an exit sign: %d, centre block free as is: %d\n", withSign, freeCenter);
  for (int r = 0; r < 16; r++) if (hist[r]) printf("  lowest platform top in cols 8-11 = row %d : %d levels\n", r, hist[r]);
}
static void dump_levels(void) {
  static Level L;
  for (int i = 1; i <= COUNT; i++) {
    level_get(i, &L);
    printf("L %d kind=%d world=%d lies=%d/%d jk=%d air=%d on=", i, L.kind, L.world, L.nlies, L.nclaims, L.jk, L.airwalk);
    for (int m = 0; m < M_N; m++) printf("%d", L.on[m]);
    printf("\n");
    if (L.kind == 0) {
      for (int r = 0; r < 15; r++) printf("%s\n", L.map[r]);
      for (int s = 0; s < L.nsigns; s++) printf("S %.0f %.0f kind=%d flip=%d\n", L.signs[s].cx, L.signs[s].y, L.signs[s].kind, L.signs[s].flip);
    } else printf("SCR %d %d %s\n", L.scrKind, L.scrTruth, L.scrLabel);
    for (int c = 0; c < L.nclaims; c++) printf("C %s %s | %s\n", L.claims[c].lie ? "LIE " : "TRUE", L.claims[c].text, L.claims[c].real);
  }
}

static int stats_all(void) {
  static Level L; int bad = 0, nbhist[8] = {0}, mut[M_N] = {0}, minL = 99, maxL = 0, minT = 99, minN = 99, maxN = 0;
  int lieCnt[C_N] = {0}, trueCnt[C_N] = {0};
  static int hasC[36][2];
  for (int i = 1; i <= COUNT; i++) {
    level_get(i, &L);
    int nt = L.nclaims - L.nlies;
    if (L.nlies < 6 || nt < 1) { printf("BAD claims level %d: lies=%d truths=%d\n", i, L.nlies, nt); bad++; }
    if (L.nlies < minL) minL = L.nlies; if (L.nlies > maxL) maxL = L.nlies;
    if (nt < minT) minT = nt;
    if (L.nclaims < minN) minN = L.nclaims; if (L.nclaims > maxN) maxN = L.nclaims;
    int nb = 0; for (int m = 0; m < M_N; m++) { nb += L.on[m]; mut[m] += L.on[m]; }
    nbhist[nb > 7 ? 7 : nb]++;
    if (L.kind == 0 && nb < 1) { printf("level %d has no behaviour\n", i); }
    /* verify lie flags against the data: count them */
    int cl = 0; for (int c = 0; c < L.nclaims; c++) cl += L.claims[c].lie;
    if (cl != L.nlies) { printf("BAD nlies mismatch %d\n", i); bad++; }
    if (L.kind == 0) for (int c = 0; c < L.nclaims; c++) { const Claim *q = &L.claims[c]; if (q->id >= 0 && q->id < 36) hasC[q->id][q->lie ? !q->ass : q->ass]++; }
    for (int c = 0; c < L.nclaims; c++) {
      if (!L.claims[c].text[0]) { printf("BAD empty claim level %d\n", i); bad++; }
      /* match text to claim id to count lie/true per statement */
      for (int id = 0; id < C_N; id++) for (int a = 0; a < 2; a++) for (int v = 0; v < 3; v++)
        if (!strcmp(CTXT[id][a][v], L.claims[c].text) && CTXT[id][a][v][0]) { if (L.claims[c].lie) lieCnt[id]++; else trueCnt[id]++; }
    }
  }
  printf("reality (has) per claim id [0/1]:");
  for (int id = 0; id < 36; id++) if (hasC[id][0] + hasC[id][1]) printf(" [%d %d/%d]", id, hasC[id][0], hasC[id][1]);
  printf("\n");
  printf("claims per level %d..%d, lies per level %d..%d, min truths %d\n", minN, maxN, minL, maxL, minT);
  printf("behaviours/level hist:"); for (int a = 0; a < 8; a++) printf(" %d:%d", a, nbhist[a]); printf("\n");
  printf("mutator counts:"); for (int m = 0; m < M_N; m++) printf(" %d", mut[m]); printf("\n");
  printf("per-claim lie/true:"); for (int id = 0; id < C_N; id++) printf(" [%d %d/%d]", id, lieCnt[id], trueCnt[id]); printf("\n");
  { int nj = 0, na = 0; for (int i = 1; i <= COUNT; i++) { level_get(i, &L); nj += L.jk; na += L.airwalk; } printf("ceiling zones: %d, ghost pits: %d\n", nj, na); }
  printf("stats problems: %d\n", bad);
  return bad;
}

typedef struct { Player p; int g, inv, jumped, pj, cry; double sp, t, slowT; } Node;
typedef struct { double pri; int node; } HEnt;
static HEnt *heap; static int hn;
static void hpush(double pri, int node) {
  int i = hn++; heap[i].pri = pri; heap[i].node = node;
  while (i > 0) { int par = (i - 1) / 2; if (heap[par].pri <= heap[i].pri) break; HEnt t = heap[par]; heap[par] = heap[i]; heap[i] = t; i = par; }
}
static int hpop(void) {
  int top = heap[0].node; heap[0] = heap[--hn]; int i = 0;
  for (;;) { int l = 2 * i + 1, r = l + 1, m = i;
    if (l < hn && heap[l].pri < heap[m].pri) m = l;
    if (r < hn && heap[r].pri < heap[m].pri) m = r;
    if (m == i) break; HEnt t = heap[m]; heap[m] = heap[i]; heap[i] = t; i = m; }
  return top;
}
#define HSIZE (1 << 21)
static unsigned long long *seen;
static int seen_add(unsigned long long k) {
  if (k == 0) k = 1;
  unsigned long long i = (k * 0x9E3779B97F4A7C15ULL) >> 43;
  for (;;) { i &= HSIZE - 1; if (seen[i] == k) return 0; if (seen[i] == 0) { seen[i] = k; return 1; } i++; }
}
static long long fl(double v) { return (long long)floor(v); }
static unsigned long long keyof(const Player *p, const World *w, int pj) {
  long long tb = w->lv->P > 0 ? fl(fmod(w->t, w->lv->P) / 0.2) : 0;   /* the clock decides what is true, so it is part of the state */
  long long c[10] = { fl(p->x / 8), fl(p->y / 12), fl(p->vx / 70), fl(p->vy / 200), p->ground, w->gravity, w->invert, w->speed > 1, pj, tb };
  unsigned long long h = 1469598103934665603ULL;
  for (int i = 0; i < 10; i++) { h ^= (unsigned long long)(c[i] + 1000); h *= 1099511628211ULL; }
  return h;
}
static int solve_level(const Level *lv, int limit, int *nout) {
  static World W; static Node *nodes; static int nn;
  if (!nodes) { nodes = (Node *)malloc(sizeof(Node) * 700000); heap = (HEnt *)malloc(sizeof(HEnt) * 700000); seen = (unsigned long long *)malloc(sizeof(unsigned long long) * HSIZE); }
  memset(seen, 0, sizeof(unsigned long long) * HSIZE);
  world_init(&W, lv);
  double gx = lv->on[M_FLEE] ? 18 * 32 + 16 : W.ents[W.goal].x + W.ents[W.goal].w / 2;
  nn = 0; hn = 0;
  nodes[0].p = W.p; nodes[0].g = W.gravity; nodes[0].inv = W.invert; nodes[0].sp = W.speed; nodes[0].t = W.t; nodes[0].jumped = W.jumped; nodes[0].pj = 0; nodes[0].slowT = 0; nodes[0].cry = 0;
  nn = 1; hpush(fabs(gx - W.p.x), 0);
  seen_add(keyof(&W.p, &W, 0));
  int n = 0;
  while (hn > 0 && n < limit) {
    int cur = hpop(); n++;
    Node s = nodes[cur];
    for (int d = -1; d <= 1; d++) for (int j = 0; j <= 1; j++) {
      W.p = s.p; W.gravity = s.g; W.invert = s.inv; W.speed = s.sp; W.t = s.t; W.jumped = s.jumped; W.dead = W.won = 0; W.slowT = s.slowT; W.crEv = 0;
      for (int q = 0; q < W.ncry; q++) W.ents[W.cry[q]].active = !((s.cry >> q) & 1);
      if (j && !s.pj) W.p.buf = 0.12;
      Held h = { d < 0, d > 0, j };
      for (int k = 0; k < 4; k++) { world_step(&W, 1.0 / 60.0, &h); if (W.dead || W.won) break; }
      if (W.won) { *nout = n; if (getenv("SOLVE_T")) printf("  lvl %d won t=%.2f x=%.0f\n", lv->idx, W.t, W.p.x); return 1; }
      if (!W.dead) {
        world_hooks(&W, 4.0 / 60.0, 1);
        int cm = 0; for (int q = 0; q < W.ncry; q++) if (!W.ents[W.cry[q]].active) cm |= 1 << q;
        if (!W.dead && seen_add(keyof(&W.p, &W, j) ^ ((unsigned long long)(fl(W.slowT / 0.5) + 1) * 0x9E3779B1ULL) ^ ((unsigned long long)cm << 40)) && nn < 700000) {
          nodes[nn].p = W.p; nodes[nn].g = W.gravity; nodes[nn].inv = W.invert; nodes[nn].sp = W.speed; nodes[nn].t = W.t; nodes[nn].jumped = W.jumped; nodes[nn].pj = j; nodes[nn].slowT = W.slowT; nodes[nn].cry = cm;
          hpush(fabs(gx - W.p.x) + n * 0.002, nn); nn++;
        }
      }
    }
  }
  *nout = n;
  return 0;
}
static int solve_range(int a, int b) {
  static Level L; int fails = 0, built = 0, maxn = 0;
  for (int i = a; i <= b; i++) {
    level_get(i, &L);
    if (L.kind) continue;
    int n; int ok = solve_level(&L, 60000, &n);
    built++; if (n > maxn) maxn = n;
    if (!ok) { printf("FAIL %d on=", i); for (int m = 0; m < M_N; m++) printf("%d", L.on[m]); printf("\n"); fails++; }
  }
  printf("solved %d platform levels, %d failures, max expansions %d\n", built, fails, maxn);
  return fails;
}
#endif

/* ============================================================== main */
/* the game draws on a 640x480 canvas; scale x and y separately so it fills any screen (720x480 on the RG34XX SP) */
static void fit_screen(void) {
  int ow = 0, oh = 0;
  SDL_GetRendererOutputSize(RD, &ow, &oh);
  if (ow > 0 && oh > 0) SDL_RenderSetScale(RD, (float)ow / 640.0f, (float)oh / 480.0f);
}

#ifdef TRUST_TEST
/* ---- box art / preview scene (test build only): colourful key art drawn with the game's own sprites; the title is added afterwards in PIL */
static void a_lerp(double t, const double (*st)[4], int n, double *o) {
  for (int i = 0; i + 1 < n; i++) if (t <= st[i + 1][0] || i + 2 == n) {
    double u = (t - st[i][0]) / (st[i + 1][0] - st[i][0]); if (u < 0) u = 0; if (u > 1) u = 1;
    for (int k = 0; k < 3; k++) o[k] = st[i][1 + k] + (st[i + 1][1 + k] - st[i][1 + k]) * u;
    return;
  }
}
static void a_sprite(SDL_Texture *tx, double cx, double bottom, double wpx, int flipH) {
  int w, h; SDL_QueryTexture(tx, NULL, NULL, &w, &h);
  double s = wpx / w; SDL_Rect d = { (int)(cx - wpx / 2), (int)(bottom - h * s), (int)wpx, (int)(h * s) };
  SDL_RenderCopyEx(RD, tx, NULL, &d, 0, NULL, flipH ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
}
static void a_platform(double x0, double x1, double gy, double H) {
  setc(0.67f, 0.33f, 0.22f, 1); frect(x0, gy, x1 - x0, H - gy);
  for (double y = gy + 14; y < H; y += 32) { setc(0.5f, 0.23f, 0.18f, 1); frect(x0, y, x1 - x0, 2); }
  for (double x = x0; x < x1; x += 32) { setc(0.5f, 0.23f, 0.18f, 1); frect(x, gy, 2, H - gy); }
  for (double y = gy + 18; y < H; y += 32) for (double x = x0 + 4; x < x1; x += 32) {
    unsigned h = wh((unsigned)x, (unsigned)y, 9); setc(0.82f, 0.5f, 0.32f, 1); frect(x + (h & 15), y + ((h >> 4) & 7), 4, 4);
    setc(0.4f, 0.18f, 0.16f, 1); frect(x + ((h >> 8) & 15), y + ((h >> 12) & 7) + 6, 3, 3);
  }
  setc(0.2f, 0.58f, 0.31f, 1); frect(x0, gy + 8, x1 - x0, 6);
  for (double x = x0; x < x1; x += 16) { unsigned h = wh((unsigned)x, 4, 2); setc(0.2f, 0.58f, 0.31f, 1); frect(x + 2, gy + 14, 8, 4 + (h % 9)); }
  setc(0.33f, 0.84f, 0.42f, 1); frect(x0, gy, x1 - x0, 9);
  setc(0.68f, 0.98f, 0.58f, 1); frect(x0, gy, x1 - x0, 3);
}
static void art_scene(int kind, const char *out) {
  int W = kind ? 640 : 480, H = kind ? 480 : 640;
  art_world(1);
  SDL_Texture *tg = SDL_CreateTexture(RD, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, W, H);
  SDL_SetRenderTarget(RD, tg); SDL_RenderSetScale(RD, 1, 1); SDL_SetRenderDrawBlendMode(RD, SDL_BLENDMODE_BLEND);
  double gy = kind ? 392 : 534;
  static const double sky[5][4] = { { 0, 22, 10, 74 }, { 0.30, 78, 24, 138 }, { 0.58, 206, 48, 130 }, { 0.78, 255, 120, 88 }, { 1, 255, 196, 96 } };
  for (int y = 0; y < H; y++) { double c[3]; a_lerp((double)y / gy, sky, 5, c); setc(c[0] / 255, c[1] / 255, c[2] / 255, 1); frect(0, y, W, 1); }
  /* stars */
  for (int i = 0; i < 90; i++) {
    unsigned h = wh(i, 77, 5); double x = h % W, y = (h >> 10) % (int)(gy * 0.62), s = 2 + ((h >> 20) % 3 == 0) * 2;
    static const float sc[3][3] = { { 1, 1, 1 }, { 1, .92f, .6f }, { .6f, .95f, 1 } };
    const float *c = sc[(h >> 24) % 3]; setc(c[0], c[1], c[2], 0.5f + 0.5f * ((h >> 5) % 2)); frect(x, y, s, s);
  }
  /* sun */
  double sx = W / 2.0, sy = kind ? gy - 112 : gy - 150, sr = kind ? 112 : 100;
  static const float pk[3] = { 1, 0.3f, 0.6f }, ye[3] = { 1, 0.85f, 0.3f }, cy[3] = { 0.3f, 0.85f, 1 }, wm[3] = { 1, 0.7f, 0.35f }, gr[3] = { 0.4f, 1, 0.5f }, rd[3] = { 1, 0.25f, 0.3f };
  SDL_SetRenderDrawBlendMode(RD, SDL_BLENDMODE_ADD); glow(sx, sy, sr * 2.1, pk, 0.55); SDL_SetRenderDrawBlendMode(RD, SDL_BLENDMODE_BLEND);
  static const double sun[2][4] = { { 0, 255, 222, 84 }, { 1, 255, 62, 150 } };
  for (int y = (int)(sy - sr); y < (int)(sy + sr); y++) {
    double dy = y - sy, hw = sqrt(sr * sr - dy * dy), t = (y - (sy - sr)) / (2 * sr);
    if (t > 0.52 && fmod(y, 15) < (t - 0.5) * 22) continue;
    double c[3]; a_lerp(t, sun, 2, c); setc(c[0] / 255, c[1] / 255, c[2] / 255, 1); frect(sx - hw, y, 2 * hw, 1);
  }
  /* mountains */
  for (int layer = 0; layer < 2; layer++) {
    double base = gy - (layer ? 44 : 110), amp = layer ? 46 : 70, ph = layer ? 1.7 : 0.4;
    for (int x = 0; x < W; x++) {
      double y = base - amp * (0.55 * sin(x * (layer ? 0.017 : 0.011) + ph) + 0.3 * sin(x * 0.031 + ph * 2) + 0.15 * sin(x * 0.071));
      if (layer) setc(0.55f, 0.15f, 0.55f, 1); else setc(0.27f, 0.15f, 0.6f, 1);
      frect(x, y, 1, gy - y);
      if (layer) setc(1, 0.45f, 0.7f, 1); else setc(0.55f, 0.4f, 0.95f, 1);
      frect(x, y, 1, 3);
    }
  }
  /* ground with a pit */
  double pl = kind ? 200 : 150, pr = kind ? 330 : 282;
  setc(0.11f, 0.04f, 0.2f, 1); frect(pl, gy, pr - pl, H - gy);
  SDL_SetRenderDrawBlendMode(RD, SDL_BLENDMODE_ADD); glow((pl + pr) / 2, H, 150, pk, 0.7); SDL_SetRenderDrawBlendMode(RD, SDL_BLENDMODE_BLEND);
  a_platform(0, pl, gy, H); a_platform(pr, W, gy, H);
  setc(0, 0, 0.1f, 0.35f); frect(pl, gy + 14, 6, H - gy); frect(pr - 6, gy + 14, 6, H - gy);
  { double sw = 46; int n = (int)((pr - pl) / sw); double x0 = pl + ((pr - pl) - n * sw) / 2;
    for (int i = 0; i < n; i++) a_sprite(A.spike, x0 + sw * (i + 0.5), H - 4, sw, 0); }
  /* left side: torch, claim card */
  a_sprite(A.torch[1], 38, gy, 64, 0);
  double cx0 = kind ? 40 : 20, cy0 = kind ? 218 : 300;
  panel(cx0, cy0, 84, 84, ACC_BLUE);
  icon(11, cx0 + 14, cy0 + 12, 7);
  { double bx = cx0 + 14 + 56 * 0.8, by = cy0 + 12 + 56 * 0.8, rr = 15;
    setc(0.02f, 0.1f, 0.05f, 1); fcircle(bx, by, rr + 3); setc(0.3f, 0.82f, 0.4f, 1); fcircle(bx, by, rr);
    static const float wh_[3] = { 1, 1, 1 }; draw_tick(bx - 13, by - 13, 26, wh_); }
  /* right side: doors and a sign that points the wrong way */
  double ds = kind ? 62 : 52, dx0 = pr + (kind ? 26 : 10);
  static const int sty[3] = { 0, 0, 3 };
  for (int i = 0; i < 3; i++) a_sprite(A.door[sty[i]], dx0 + i * (ds + 10) + ds / 2, gy, ds, 0);
  a_sprite(A.sgExit, dx0 + (3 * ds + 20) / 2, gy - ds * 1.5 - 12, kind ? 150 : 140, 1);
  /* the hero, mid jump over the pit */
  double hx = (pl + pr) / 2, hb = gy - (kind ? 84 : 78);
  a_sprite(A.player[5], hx, hb, kind ? 78 : 72, 0);
  /* coins on the jump arc */
  for (int i = 0; i < 4; i++) {
    double ang = (i - 1.5) * 0.62, cx = hx + sin(ang) * 120, cyy = hb - 100 + (1 - cos(ang)) * 0 - 30 * (1 - fabs(i - 1.5) / 1.5) * 0 + fabs(i - 1.5) * 26;
    a_sprite(A.coin[0], cx, cyy - (kind ? 30 : 20), 34, 0);
  }
  /* glows */
  SDL_SetRenderDrawBlendMode(RD, SDL_BLENDMODE_ADD);
  glow(38, gy - 40, 70, wm, 0.7);
  glow(hx, hb - 40, 70, ye, 0.5);
  for (int i = 0; i < 3; i++) glow(dx0 + i * (ds + 10) + ds / 2, gy - ds * 0.75, 62, i == 2 ? cy : wm, 0.4);
  glow(dx0 + (3 * ds + 20) / 2, gy - ds * 1.5 - 38, 60, gr, 0.45);
  SDL_SetRenderDrawBlendMode(RD, SDL_BLENDMODE_BLEND);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_ARGB8888);
  SDL_RenderReadPixels(RD, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
  SDL_SaveBMP(s, out); SDL_FreeSurface(s);
  SDL_SetRenderTarget(RD, NULL);
}
#endif

#ifdef TRUST_TEST
#define WIN_W (getenv("TRUST_W") ? atoi(getenv("TRUST_W")) : 640)
#else
#define WIN_W 640
#endif
int main(int argc, char **argv) {
  world = (World *)malloc(sizeof(World));
#ifdef TRUST_TEST
  if (argc > 1 && !strcmp(argv[1], "--signstat")) { sign_center_stats(); return 0; }
  if (argc > 1 && !strcmp(argv[1], "--overlap")) { return overlap_all(argc > 2) ? 1 : 0; }
  if (argc > 1 && !strcmp(argv[1], "--dump")) { dump_levels(); return 0; }
  if (argc > 1 && !strcmp(argv[1], "--stats")) { return stats_all() ? 1 : 0; }
  if (argc > 3 && !strcmp(argv[1], "--solve")) { return solve_range(atoi(argv[2]), atoi(argv[3])) ? 1 : 0; }
  if (argc > 1 && !strcmp(argv[1], "--fuzz")) {
    srand(7); int maxent = 0;
    for (int i = 1; i <= COUNT; i++) {
      unlocked = COUNT; level_get(i, &level); idx = i;
      if (level.kind == 0) world_init(world, &level);
      for (int f = 0; f < 1800; f++) {
        memset(&held, 0, sizeof held); memset(&pr, 0, sizeof pr);
        if (f % 40 < 25) held.right = 1; if (rand() % 17 == 0) held.left = 1; if (rand() % 9 == 0) { held.jump = 1; pr.jump = 1; }
        if (level.kind == 1) { screen_init(&level); }
        else { Held h = { held.left, held.right, held.jump }; world_update(world, 1.0 / 60, &h, pr.jump);
          if (world->nent > maxent) maxent = world->nent;
          if (world->dead || world->won) world_init(world, &level); }
        if (level.kind == 1) break;
      }
    }
    printf("fuzz ok, max entities %d\n", maxent); return 0;
  }
  const char *shotMode = NULL, *shotOut = NULL; int shotLevel = 1, shotFrames = 60;
  if (argc > 5 && !strcmp(argv[1], "--shot")) { shotMode = argv[2]; shotLevel = atoi(argv[3]); shotFrames = atoi(argv[4]); shotOut = argv[5]; }
#endif
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  Uint32 wf = 0;
  const char *fsEnv = getenv("TRUST_FULLSCREEN");
  if (fsEnv && fsEnv[0] == '1') wf |= SDL_WINDOW_FULLSCREEN_DESKTOP;
  SDL_Window *win = SDL_CreateWindow("Devil's Detour", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, 480, wf);
  if (!win) { fprintf(stderr, "window failed: %s\n", SDL_GetError()); return 1; }
#ifdef TRUST_TEST
  if (argc > 5 && !strcmp(argv[1], "--shot")) RD = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
  else
#endif
  RD = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!RD) RD = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
  if (!RD) { fprintf(stderr, "renderer failed: %s\n", SDL_GetError()); return 1; }
  if (getenv("TRUST_KEEP_ASPECT")) SDL_RenderSetLogicalSize(RD, 640, 480);   /* default: stretch to the whole screen */
  else fit_screen();
  SDL_SetRenderDrawBlendMode(RD, SDL_BLENDMODE_BLEND);
  SDL_ShowCursor(SDL_DISABLE);
  fonts_init();
  art_init();
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) sound_init();
  open_pads();
  load_game();

#ifdef TRUST_TEST
  if (shotMode) {
    if (!strcmp(shotMode, "art")) { art_scene(shotFrames, shotOut); printf("saved %s\n", shotOut); return 0; }
    if (!strcmp(shotMode, "title")) { state = ST_TITLE; }
    else if (!strcmp(shotMode, "select")) { unlocked = shotLevel; selCursor = shotLevel; state = ST_SELECT; }
    else if (!strcmp(shotMode, "end")) { state = ST_END; deaths = 123; }
    else if (!strcmp(shotMode, "help")) { unlocked = COUNT; if (shotLevel > 1) { start_level(shotLevel - 1); if (level.nclaims > 0) { memcpy(lastClaims, level.claims, sizeof lastClaims); lastN = level.nclaims; lastIdx = idx; } } start_level(shotLevel); state = ST_PLAY; helpOpen = 1; help_pass(0); helpScroll = shotFrames; if (helpScroll > helpTotal - 398) helpScroll = helpTotal - 398; help_pass(0); }
    else {
      unlocked = COUNT; start_level(shotLevel);
      if (!strcmp(shotMode, "brief")) { briefT = 1; state = ST_BRIEF; }
      else if (!strcmp(shotMode, "screen") || !strcmp(shotMode, "card") || !strcmp(shotMode, "play") || !strcmp(shotMode, "crys") || !strcmp(shotMode, "win")) state = ST_PLAY;
      if (!strcmp(shotMode, "brief")) { }
      else if (!strcmp(shotMode, "win")) {   /* test: finish the level and land in the next one */
        state = ST_PLAY; world->won = 1; memset(&held, 0, sizeof held); memset(&pr, 0, sizeof pr);
        game_update(1.0 / 60);
        printf("after win: idx=%d state=%d lastN=%d lastIdx=%d unlocked=%d\n", idx, state, lastN, lastIdx, unlocked);
        for (int f = 0; f < shotFrames; f++) game_update(1.0 / 60);
      }
      else if (!strcmp(shotMode, "crys")) {   /* test: stand on the first crystal (TRUST_FX forces its effect), then idle */
        state = ST_PLAY;
        if (world->ncry > 0) {
          Ent *c = &world->ents[world->cry[0]];
          if (getenv("TRUST_FX")) c->fx = atoi(getenv("TRUST_FX"));
          world->p.x = c->x + 6; world->p.y = c->y + 4; world->p.vy = 0;
        }
        for (int f = 0; f < shotFrames; f++) { memset(&held, 0, sizeof held); memset(&pr, 0, sizeof pr); game_update(1.0 / 60); }
      }
      else if (!strcmp(shotMode, "screen")) { for (int i = 0; i < shotFrames; i++) { memset(&pr, 0, sizeof pr); game_update(1.0 / 60); } }
      else {
        for (int f = 0; f < shotFrames; f++) {
          memset(&held, 0, sizeof held); memset(&pr, 0, sizeof pr);
          held.right = 1; if (f % 25 > 12) held.jump = 1; if (f % 25 == 13) pr.jump = 1;
          game_update(1.0 / 60);
        }
        if (!strcmp(shotMode, "card")) { state = ST_CARD; cardT = 1; }
      }
    }
    gtime = shotFrames / 60.0;
    if (getenv("TRUST_SHAKE")) shake = atof(getenv("TRUST_SHAKE"));
    fit_screen();
    draw_all();
    int sw = 640, sh = 480; SDL_GetRendererOutputSize(RD, &sw, &sh);
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, sw, sh, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_RenderReadPixels(RD, NULL, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch);
    SDL_SaveBMP(s, shotOut);
    printf("saved %s coins=%d\n", shotOut, coinsHeld);
    return 0;
  }
#endif

  Uint64 freq = SDL_GetPerformanceFrequency(), last = SDL_GetPerformanceCounter();
  while (!quitFlag) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      if (ev.type == SDL_QUIT) quitFlag = 1;
      else if (ev.type == SDL_CONTROLLERDEVICEADDED || ev.type == SDL_JOYDEVICEADDED) open_pads();
    }
    Uint64 now = SDL_GetPerformanceCounter();
    double dt = (double)(now - last) / (double)freq; last = now;
    held = sample();
    compute_pressed();
    game_update(dt);

    if (!getenv("TRUST_KEEP_ASPECT")) fit_screen();
    draw_all();
    SDL_RenderPresent(RD);
    Uint64 after = SDL_GetPerformanceCounter();
    double frame = (double)(after - now) / (double)freq;
    if (frame < 0.0155) SDL_Delay((Uint32)((0.0165 - frame) * 1000.0));
  }
  save_game();
  if (audioOK) SDL_CloseAudioDevice(adev);
  SDL_DestroyRenderer(RD); SDL_DestroyWindow(win); SDL_Quit();
  return 0;
}

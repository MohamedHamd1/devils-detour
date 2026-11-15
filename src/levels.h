/* Level generation: layout + behaviours + the claims each level makes about itself. */
#ifndef LEVELS_H
#define LEVELS_H
#include <math.h>
#include <stdio.h>
#include <string.h>

#define WORLDS 11
#define PER 50
#define COUNT (WORLDS * PER)


static const char *WORLD_NAMES[WORLDS] = { "Trust Falls", "Fine Print", "Small Lies", "Tall Tales",
  "Half Truths", "Hearsay", "Gaslight", "Bait & Switch", "Alibi", "Perjury", "The Whole Truth" };

/* ground body g1, grass/top g2, sky stops (top, middle, horizon), sun (top, bottom), mountains (far, near) */
typedef struct { float g1[3], g2[3], sk[3][3], sun[2][3], mt[2][3]; } Pal;
#define RGB(r, g, b) { (r) / 255.0f, (g) / 255.0f, (b) / 255.0f }
static const Pal PALS[WORLDS] = {
  /* 1 Trust Falls: violet sunset */   { RGB(170, 84, 56), RGB(84, 214, 106), { RGB(22, 10, 74), RGB(110, 30, 150), RGB(255, 130, 90) }, { RGB(255, 222, 84), RGB(255, 62, 150) }, { RGB(70, 40, 150), RGB(150, 40, 150) } },
  /* 2 Fine Print: pink dusk */        { RGB(130, 70, 160), RGB(255, 140, 215), { RGB(18, 10, 80), RGB(110, 34, 180), RGB(255, 110, 170) }, { RGB(255, 235, 130), RGB(255, 80, 170) }, { RGB(66, 34, 150), RGB(140, 44, 170) } },
  /* 3 Small Lies: green dawn */       { RGB(150, 100, 60), RGB(120, 232, 90), { RGB(8, 40, 70), RGB(20, 130, 130), RGB(255, 214, 120) }, { RGB(255, 250, 170), RGB(255, 150, 80) }, { RGB(20, 96, 120), RGB(40, 150, 120) } },
  /* 4 Tall Tales: honey sunset */     { RGB(190, 104, 52), RGB(255, 206, 72), { RGB(60, 20, 90), RGB(220, 70, 100), RGB(255, 196, 80) }, { RGB(255, 245, 160), RGB(255, 110, 60) }, { RGB(120, 40, 110), RGB(200, 70, 100) } },
  /* 5 Half Truths: ocean */           { RGB(64, 96, 210), RGB(110, 240, 200), { RGB(10, 20, 96), RGB(30, 96, 206), RGB(130, 224, 255) }, { RGB(255, 255, 255), RGB(110, 226, 255) }, { RGB(36, 64, 176), RGB(50, 130, 214) } },
  /* 6 Hearsay: crimson */             { RGB(160, 62, 70), RGB(255, 132, 112), { RGB(54, 8, 48), RGB(190, 34, 88), RGB(255, 150, 96) }, { RGB(255, 224, 150), RGB(255, 56, 96) }, { RGB(116, 24, 80), RGB(190, 44, 90) } },
  /* 7 Gaslight: toxic teal */         { RGB(70, 126, 116), RGB(176, 255, 150), { RGB(6, 32, 46), RGB(20, 116, 116), RGB(186, 242, 140) }, { RGB(244, 255, 176), RGB(60, 224, 176) }, { RGB(22, 84, 96), RGB(40, 146, 124) } },
  /* 8 Bait & Switch: orange */        { RGB(204, 112, 44), RGB(255, 224, 84), { RGB(44, 20, 86), RGB(232, 96, 44), RGB(255, 224, 110) }, { RGB(255, 250, 196), RGB(255, 150, 44) }, { RGB(134, 54, 96), RGB(214, 96, 64) } },
  /* 9 Alibi: cyan night */            { RGB(44, 112, 156), RGB(104, 250, 230), { RGB(6, 18, 66), RGB(12, 104, 166), RGB(96, 232, 232) }, { RGB(206, 255, 255), RGB(64, 204, 236) }, { RGB(24, 64, 156), RGB(34, 136, 196) } },
  /* 10 Perjury: magenta */            { RGB(136, 54, 156), RGB(255, 116, 232), { RGB(32, 8, 66), RGB(156, 34, 166), RGB(255, 96, 204) }, { RGB(255, 206, 255), RGB(226, 64, 226) }, { RGB(86, 24, 136), RGB(176, 44, 176) } },
  /* 11 The Whole Truth: silver */     { RGB(116, 116, 140), RGB(244, 244, 255), { RGB(10, 10, 24), RGB(70, 70, 104), RGB(226, 226, 255) }, { RGB(255, 255, 255), RGB(190, 200, 255) }, { RGB(56, 56, 96), RGB(100, 100, 150) } },
};


/* ------------------------------------------------------------------ rng */
typedef struct { long long s; } Rng;
static long long rng_next(Rng *r) { r->s = (r->s * 16807LL) % 2147483647LL; return r->s; }
static void rng_init(Rng *r, long long seed) {
  r->s = (seed * 7919LL + 13LL) % 2147483646LL + 1LL;
  rng_next(r); rng_next(r); rng_next(r);
}
static double rf(Rng *r) { return (double)rng_next(r) / 2147483647.0; }
static int ri(Rng *r, int a, int b) { return a + (int)floor(rf(r) * (double)(b - a + 1)); }

/* ------------------------------------------------------------------ layout */
#define PIT (-1)
#define NIL (-2)
typedef struct { int longRun, manySpots, lastFlat, flatMid, wallRun; } Opt;
typedef struct { int s, e, top; } Grp;
typedef struct { int s, top; } Spot;

static int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void fillrun(int *top, int c, int len, int v) {
  for (int k = 0; k < len; k++) if (c + k <= 19) top[c + k] = v;
}

static void gen_tops(Rng *R, double diff, const Opt *opt, int *top) {
  for (int c = 0; c < 20; c++) top[c] = NIL;
  int f = 13;
  for (int c = 0; c <= 2; c++) top[c] = f;
  int c = 3;
  int gmax = diff < 0.3 ? 2 : 3;
  int first = 1;
  while (c < 17) {
    double roll = rf(R);
    if ((opt->longRun || opt->wallRun) && first) {
      int n = opt->wallRun ? 7 : 6;   /* a wall run: columns 0..9 are flat, room for the wall and the lucky gate */
      fillrun(top, c, n, f);
      c += n;
    } else if (roll < 0.25 + 0.30 * diff) {
      int g = ri(R, 2, gmax);
      int nf = f;
      double r2 = rf(R);
      if (r2 < 0.25) nf = f + 1; else if (r2 < 0.45 && g <= 2) nf = f - 1;
      nf = clampi(nf, 8, 13);
      fillrun(top, c, g, PIT);
      c += g;
      int len = ri(R, 2, 4);
      fillrun(top, c, len, nf);
      c += len;
      f = nf;
    } else if (roll < 0.55 + 0.10 * diff) {
      static const int dl[4] = { -2, -1, 1, 2 };
      int nf = clampi(f + dl[ri(R, 1, 4) - 1], 8, 13);
      int len = ri(R, 2, 4);
      fillrun(top, c, len, nf);
      c += len;
      f = nf;
    } else {
      int len = ri(R, 2, 5);
      fillrun(top, c, len, f);
      c += len;
    }
    first = 0;
  }
  for (int k = 0; k < 20; k++) if (top[k] == NIL) top[k] = f;
}

static int groups(const int *top, Grp *out) {
  int n = 0, c = 0;
  while (c <= 19) {
    if (top[c] >= 0) {
      int s = c, t = top[c];
      while (c + 1 <= 19 && top[c + 1] >= 0 && top[c + 1] == t) c++;
      out[n].s = s; out[n].e = c; out[n].top = t; n++;
    }
    c++;
  }
  return n;
}

/* occ is indexed with +4 offset */
static int spots(const int *top, int w, const int *occ, Spot *out) {
  Grp g[20];
  int ng = groups(top, g), n = 0;
  for (int gi = 0; gi < ng; gi++) {
    int lo = g[gi].s + 2; if (lo < 4) lo = 4;
    int hi = g[gi].e - w - 1; if (hi > 16 - (w - 1)) hi = 16 - (w - 1);
    for (int s = lo; s <= hi; s++) {
      int ok = 1;
      if (occ) for (int k = s - 2; k <= s + w + 1; k++) if (occ[k + 4]) ok = 0;
      if (ok) { out[n].s = s; out[n].top = g[gi].top; n++; }
    }
  }
  return n;
}

static int validate(const int *top, int dir, double diff, const Opt *opt) {
  int gmax = diff < 0.3 ? 2 : 3;
  if (top[1] < 0 || top[18] < 0) return 0;
  int run = 0;
  for (int c = 0; c <= 20; c++) {
    if (c <= 19 && top[c] >= 0) run++;
    else { if (run > 0 && run < 2) return 0; run = 0; }
  }
  int c0 = 0, c1 = 19, st = 1;
  if (dir < 0) { c0 = 19; c1 = 0; st = -1; }
  int gap = 0, last = NIL;
  for (int c = c0; st > 0 ? c <= c1 : c >= c1; c += st) {
    int t = top[c];
    if (t >= 0) {
      if (gap > 0) {
        if (gap > gmax) return 0;
        int rise = last - t;
        if (rise > 1) return 0;
        if (gap >= 3 && rise > 0) return 0;
        gap = 0;
      } else if (last != NIL && last - t > 2) {
        return 0;
      }
      last = t;
    } else gap++;
  }
  if (gap > 0) return 0;
  Spot sp[40];
  if (opt->longRun && spots(top, 1, 0, sp) < 1) return 0;
  if (opt->manySpots && spots(top, 1, 0, sp) < 3) return 0;
  if (opt->lastFlat) {
    int t = top[16];
    if (t < 0) return 0;
    for (int c = 16; c <= 19; c++) if (top[c] != t) return 0;
  }
  if (opt->flatMid) {
    int t = top[7];
    if (t < 0) return 0;
    for (int c = 7; c <= 13; c++) if (top[c] != t) return 0;
  }
  return 1;
}

/* ------------------------------------------------------------------ claims model
 * A level has up to 4 REAL behaviours ("mutators").  Every level also makes 11-13 CLAIMS about itself
 * (mutator claims + fact claims).  Each claim is independently generated as a lie or a truth, then
 * adjusted so there are always >= 6 lies and >= 1 truth.  The same statement is a lie on one level
 * and the truth on another, so nothing can be learned by rote. */
enum { M_POP, M_ARROW, M_INVERT, M_COINS, M_CRUMBLE, M_GRAVITY, M_FLEE, M_DECOY, M_INVIS, M_SPEED, M_FAKES, M_N };
enum { F_SPIKES = M_N, F_PITS, F_HIGH, F_TORCH, F_CRYSTAL, F_COINS, F_CTR, F_NUMB, F_COUNT, C_JK, C_TP, C_N };
#define IC_HIGH 30
#define IC_TORCH 31
#define IC_CRYSTAL 32
#define IC_COINS 33
#define IC_JK 34
#define IC_TP 35

/* text variants: [claim][asserts-it-happens 0/1][variant] */
static const char *CTXT[C_N][2][3] = {
  /* POP */    { { "Nothing pops up out of the floor.", "No hidden spikes. None at all.", "Every spike is there from the start." },
                 { "Spikes will pop up as you walk.", "Some spikes are hiding. They will pop.", "Spikes appear when you get close." } },
  /* ARROW */  { { "The exit is on the right.", "Head right. The exit is there.", "The way out is to the right." },
                 { "The exit is on the left.", "Go left. The exit is over there.", "The way out is to the left." } },
  /* INVERT */ { { "Your controls are normal.", "Left is left. Right is right.", "Nothing is wrong with the controls." },
                 { "Your controls will flip.", "Left and right keep swapping.", "The controls turn against you now and then." } },
  /* COINS */  { { "Coins are harmless.", "Free coins. Take as many as you like.", "Coins cannot hurt you." },
                 { "Coins are deadly.", "Do not touch the coins.", "Coins will kill you." } },
  /* CRUMBLE */{ { "The floor is solid.", "Stand wherever you like.", "Every floor here will hold." },
                 { "Some floors will crumble.", "Do not trust the floor.", "The ground is falling apart." } },
  /* GRAVITY */{ { "Gravity never changes.", "Down is always down.", "Gravity is perfectly normal." },
                 { "Gravity will flip.", "Down will become up.", "You will fall upwards." } },
  /* FLEE */   { { "The exit stays where it is.", "The flag will wait for you.", "The exit is not going anywhere." },
                 { "The exit will run away.", "The flag moves when you get close.", "The exit will try to escape you." } },
  /* DECOY */  { { "Every flag is a real exit.", "There is no fake flag.", "All flags lead out." },
                 { "One of the flags is fake.", "Do not trust the first flag.", "A decoy flag is waiting." } },
  /* INVIS */  { { "There are no invisible floors.", "Step into a pit and you fall. Always.", "Every pit is a real pit." },
                 { "Some floors are invisible.", "You can walk across some pits.", "Not every pit is a pit." } },
  /* SPEED */  { { "Your speed never changes.", "You are not getting faster.", "Same speed as always." },
                 { "You will speed up.", "You will get faster, then slower.", "Things are about to speed up." } },
  /* FAKES */  { { "Every spike is deadly.", "All spikes are real.", "Spikes mean death. Always." },
                 { "Some spikes are harmless.", "Some spikes are only painted on.", "Not every spike bites." } },
  /* SPIKES */ { { "There are no spikes here.", "This level is spike-free.", "Not a single spike. Promise." },
                 { "There are spikes in this level.", "Spikes ahead. Be careful.", "Watch out for the spikes." } },
  /* PITS */   { { "There are no pits here.", "The ground is unbroken.", "You cannot fall down here." },
                 { "There are pits to fall into.", "Mind the gaps.", "Some of the ground is missing." } },
  /* HIGH */   { { "The exit is not higher than the start.", "The exit is down low.", "You never have to climb to the exit." },
                 { "The exit is higher than the start.", "The exit is up high.", "You must climb to reach the exit." } },
  /* TORCH */ { { "There are no torches here.", "No torches anywhere.", "It is dark. No torch." },
                 { "There is a torch here.", "A torch is burning nearby.", "Look for the torch." } },
  /* CRYSTAL */{ { "There are no crystals here.", "No crystals in this level.", "Nothing shiny but coins." },
                 { "There is a crystal here.", "A crystal is hidden nearby.", "Look for the crystal." } },
  /* COINS */  { { "There are no coins here.", "Not one coin in sight.", "This level has no coins." },
                 { "There are coins here.", "Coins are lying around.", "You can find coins here." } },
  /* CTR */    { { "The death counter lies.", "The death counter is off.", "Do not trust the death counter." },
                 { "The death counter is honest.", "The death counter is accurate.", "Trust the death counter." } },
  /* NUMB */   { { "", "", "" }, { "", "", "" } },
  /* COUNT */  { { "", "", "" }, { "", "", "" } },
  /* JUMPKILL */{ { "Jumping is safe.", "Jump as much as you like.", "Nothing is waiting above you." },
                 { "Jumping will kill you.", "Do not jump here.", "The ceiling bites jumpers." } },
  /* TELEPORT */{ { "No gate will move you.", "Gates never teleport anyone.", "A gate is just a gate." },
                 { "A gate will send you elsewhere.", "Some gates are portals.", "One gate takes you back." } },
};
/* what really happened (shown on the card for lies):  [mutator][behaviour 0/1] */
static const char *CREAL[M_N][2] = {
  { "No spikes popped up.", "Spikes popped up." },
  { "The exit was on the right.", "The exit was on the left." },
  { "Controls stayed normal.", "Controls flipped." },
  { "Coins were harmless.", "Coins were deadly." },
  { "Floors held.", "Floors crumbled." },
  { "Gravity stayed put.", "Gravity flipped." },
  { "The exit stayed put.", "The exit ran away." },
  { "No fake flag.", "A flag was fake." },
  { "No invisible floors.", "Floors were invisible." },
  { "Speed stayed put.", "You sped up." },
  { "All spikes were real.", "Some spikes were fake." },
};
static const char *FREAL[7][2] = { /* F_SPIKES.. */
  { "There are no spikes.", "There are spikes." }, { "There are no pits.", "There are pits." },
  { "The exit was not higher.", "The exit was higher." }, { "There was no torch.", "There was a torch." },
  { "There was no crystal.", "There was a crystal." }, { "There were no coins.", "There were coins." },
  { "The counter lied.", "The counter was right." },
};

enum { SG_EXIT, SG_CHEV, SG_DECOY, SG_Q };
typedef struct { double cx, y; int kind, flip; } Sign;
typedef struct { int col, top, done; double a, b; } Pop;   /* spike is up while the clock is inside [a, b) */
typedef struct { int col; double y, t; } FleeT;   /* the exit stands at col from moment t on */
typedef struct { char text[56]; char real[28]; int lie, id, ass, num, realnum; } Claim;

typedef struct {
  int kind;            /* 0 platform, 1 screen */
  int world, k, idx;
  char title[96];
  char map[15][21];
  Sign signs[12];
  int nsigns;
  int mir, dir;
  int on[M_N];         /* which behaviours really happen on this level */
  int nd; struct { int c, r, type, style, dc, dr; } door[5];   /* type 0 exit door, 1 deadly door, 2 teleport gate to (dc, dr), 3 deadly gate, 4 lucky gate (teleports or kills, by timing); style = how it looks (doors 0 = wood, gates 3) */
  int jk, airwalk;     /* number of hidden-ceiling zones, number of invisible-floor pits */
  int lucky; double lgp, lga, lgb;   /* a lucky gate throws you past a wall only while fmod(t, lgp) is inside [lga, lgb); otherwise it kills you */
  /* everything below flips with the clock: one period of P seconds that repeats from the first frame */
  double P;
  int ninv; double inv[6];                 /* the controls swap sides at each of these moments */
  int nspd; double spdA, spdB;             /* you run 1.4x faster inside [spdA, spdB) */
  struct { int s, e, row; double a, b; } jz[4];   /* hidden ceiling over columns s..e, deadly to jumpers inside [a, b) */
  struct { int s, e, blink; double a, b; } gp[6]; int ngp;   /* invisible-floor pits, solid inside [a, b) when blink */
  int npops; Pop pops[4];
  int nflee; FleeT flee[5];
  int ngw; double gw[2][2];   /* gravity points up inside these windows */
  Claim claims[16];
  int nclaims, nlies, tip, deathOff;
  int scrKind, scrTruth; char scrLabel[8];
} Level;

static const float GREEN[3] = { 0.5f, 0.9f, 0.55f };
static const float ORANGE[3] = { 1.0f, 0.6f, 0.4f };
static const float GOLD[3] = { 1.0f, 0.85f, 0.3f };
static const float GREY[3] = { 0.5f, 0.5f, 0.55f };

static int nearest_solid(const int *top, int c0) {
  static const int offs[9] = { 0, 1, -1, 2, -2, 3, -3, 4, -4 };
  for (int k = 0; k < 9; k++) {
    int c = c0 + offs[k];
    if (c >= 2 && c <= 17 && top[c] >= 0 && top[c + 1] >= 0) return c;
  }
  return c0;
}

typedef struct { char g[15][20]; int top[20]; int occ[32]; Level *L; int mir; } Bld;

static void add_sign(Bld *b, double cx, double y, int kind, int flip) {
  Level *L = b->L;
  if (L->nsigns >= 12) return;
  Sign *s = &L->signs[L->nsigns++];
  s->cx = b->mir ? 640.0 - cx : cx;
  s->y = y; s->kind = kind; s->flip = flip;
}

static void add_claim(Level *L, const char *text, const char *real, int lie, int id, int ass, int num, int realnum) {
  if (L->nclaims >= 16) return;
  Claim *c = &L->claims[L->nclaims++];
  strncpy(c->text, text, 55); c->text[55] = 0;
  strncpy(c->real, real, 27); c->real[27] = 0;
  c->lie = lie; c->id = id; c->ass = ass; c->num = num; c->realnum = realnum;
  if (lie) L->nlies++;
}

/* gravity set rules out things whose geometry would be flipped under them */
static int mut_conflict(const int *on, int m) {
  if (m == M_FLEE) return on[M_ARROW] || on[M_DECOY];
  if (m == M_ARROW || m == M_DECOY) return on[M_FLEE];
  return 0;
}

static int primary_of(int i) {
  static const int forced[9] = { M_POP, M_ARROW, M_INVERT, M_COINS, M_CRUMBLE, M_GRAVITY, M_FLEE, M_DECOY, M_INVIS };
  static const int order[M_N] = { M_POP, M_ARROW, M_INVERT, M_COINS, M_CRUMBLE, M_GRAVITY, M_FLEE, M_DECOY,
                                  M_INVIS, M_SPEED, M_FAKES };
  return (i <= 9) ? forced[i - 1] : order[(i * 5 + i / 11) % M_N];
}

static void pick_mut(int i, double diff, int *on) {
  int primary = primary_of(i);
  on[primary] = 1;
  Rng X; rng_init(&X, (long long)i * 1009 + 4242);
  int nb = 1;
  if (i >= 10) { nb = 1 + (int)floor(diff * 2.5) + (rf(&X) < 0.4 ? 1 : 0); if (nb > 4) nb = 4; }
  int have = 1;
  for (int tries = 0; tries < 40 && have < nb; tries++) {
    int m = ri(&X, 0, M_N - 1);
    if (on[m] || mut_conflict(on, m)) continue;
    on[m] = 1; have++;
  }
}


/* carve a pit of width w into the ground; both sides must stay solid and level */
static int carve_pit(int *top, Rng *R, int w, int minc) {
  for (int t = 0; t < 16; t++) {
    int c0 = ri(R, minc, 15 - w + 1);
    int ok = 1;
    for (int c = c0 - 2; c <= c0 + w + 1; c++) {
      if (c < 2 || c > 17) { ok = 0; break; }
      if ((c < c0 || c >= c0 + w) && top[c] < 0) { ok = 0; break; }
    }
    if (!ok || abs(top[c0 - 1] - top[c0 + w]) > 1) continue;
    for (int c = c0; c < c0 + w; c++) top[c] = PIT;
    return c0;
  }
  return -1;
}

typedef struct { int s, w, top; } SpikeCol;

static void build_try(int i, Level *L, int attempt) {
  memset(L, 0, sizeof *L);
  int world = (i - 1) / PER + 1, k = (i - 1) % PER + 1;
  double diff = (double)(i - 1) / (double)(COUNT - 1);
  int want[M_N]; memset(want, 0, sizeof want);
  pick_mut(i, diff, want);
  long long seed = (long long)i * 1009 + 77 + (long long)attempt * 100003LL;
  Opt opt = { 0, 1, 0, 0 };
  if (want[M_POP]) opt.longRun = 1;
  if (want[M_DECOY]) opt.lastFlat = 1;
  if (want[M_GRAVITY]) opt.flatMid = 1;
  /* lucky-gate levels (about one in three eligible levels): a flat start run with a wall and a gate on it */
  if (i >= 14 && !want[M_GRAVITY] && !want[M_FLEE] && !want[M_CRUMBLE]) { Rng G0; rng_init(&G0, (long long)i * 1009 + 6262); if (rf(&G0) < 0.34) opt.wallRun = 1; }
  int mir = want[M_ARROW];
  int dir = mir ? -1 : 1;

  Bld B; memset(&B, 0, sizeof B);
  Rng R; int found = 0;
  int *top = B.top;
  for (int a = 0; a <= 600; a++) {
    rng_init(&R, seed + (long long)a * 131);
    gen_tops(&R, diff, &opt, top);
    if (validate(top, dir, diff, &opt)) { found = 1; break; }
  }
  if (!found) for (int c = 0; c < 20; c++) top[c] = 13;

  /* jump tricks.  Pits get their own rule: open pit (jump it, walking in kills), ghost pit (walk across the
     invisible floor), or ghost pit with a hidden ceiling (walk across, jumping kills).  Rules differ pit by pit. */
  Rng JX; rng_init(&JX, seed + 777);
  int gh[10] = { 0 }, ce[10] = { 0 }, npit = 0; struct { int s, e; } pit[10];
  if (!want[M_GRAVITY]) {
    int nx = (rf(&JX) < 0.65 ? 1 : 0) + (diff > 0.1 && rf(&JX) < 0.5 ? 1 : 0);
    for (int q = 0; q < nx; q++) carve_pit(top, &JX, ri(&JX, 2, 3), opt.wallRun ? 11 : 4);
    if (rf(&JX) < 0.22) carve_pit(top, &JX, 5, opt.wallRun ? 11 : 4);   /* too wide to jump: only crossable on a ghost floor */
  }
  for (int c = 0; c < 20 && npit < 10;) {
    if (top[c] < 0) { int s0 = c; while (c < 20 && top[c] < 0) c++; pit[npit].s = s0; pit[npit].e = c - 1; npit++; } else c++;
  }
  int nghost = 0;
  for (int q = 0; q < npit; q++) {
    int s0 = pit[q].s, e0 = pit[q].e, wide = (e0 - s0 + 1) >= 5;
    if (want[M_GRAVITY]) { gh[q] = want[M_INVIS]; }
    else {
      gh[q] = wide || rf(&JX) < (want[M_INVIS] ? 0.6 : 0.1);
      int canCeil = s0 >= 3 && e0 <= 17 && top[s0 - 1] == top[s0 - 2] && top[e0 + 1] >= top[s0 - 1];
      ce[q] = gh[q] && canCeil && rf(&JX) < 0.55;
    }
    nghost += gh[q];
  }
  if (want[M_INVIS] && npit > 0 && nghost == 0) { gh[0] = 1; nghost = 1; }
  L->airwalk = nghost;
  for (int q = 0; q < npit && L->ngp < 6; q++) if (gh[q]) {
    L->gp[L->ngp].s = pit[q].s; L->gp[L->ngp].e = pit[q].e; L->gp[L->ngp].blink = 0; L->ngp++;
  }

  for (int r = 0; r < 15; r++) for (int c = 0; c < 20; c++)
    B.g[r][c] = (top[c] >= 0 && r >= top[c]) ? '#' : '.';
  for (int c = 0; c < 20; c++) if (top[c] < 0) B.g[14][c] = '^';
  B.g[top[1] - 1][1] = 'P';
  B.g[top[18] - 1][18] = 'G';

  B.L = L; B.mir = mir;
  L->world = world; L->k = k; L->idx = i; L->mir = mir; L->dir = dir;
  int *occ = B.occ;
  if (opt.wallRun) for (int c = 3; c <= 9; c++) occ[c + 4] = 1;   /* keep objects off the wall run */
  /* hidden-ceiling zones: each is armed during its own random stretch of the first seconds */
  #define ADD_ZONE(S, E, ROW) do { if (L->jk < 4) { int z = L->jk++; L->jz[z].s = (S); L->jz[z].e = (E); L->jz[z].row = (ROW); } } while (0)
  for (int q = 0; q < npit; q++) if (ce[q]) {
    ADD_ZONE(pit[q].s, pit[q].e, top[pit[q].s - 1]);
    for (int c = pit[q].s - 3; c <= pit[q].e + 3; c++) if (c >= 0 && c < 20) occ[c + 4] = 1;
  }
  if (!want[M_GRAVITY] && !opt.wallRun && attempt < 20 && rf(&JX) < 0.6) {
    Grp gq[20]; int ng2 = groups(top, gq), st = ri(&JX, 0, ng2 > 0 ? ng2 - 1 : 0), want2 = (rf(&JX) < 0.3) ? 2 : 1, got = 0;
    for (int q0 = 0; q0 < ng2 && got < want2; q0++) {
      int q = (q0 + st) % ng2;
      int lo = gq[q].s > 3 ? gq[q].s : 3, hi = gq[q].e < 16 ? gq[q].e : 16;
      if (hi - lo + 1 >= 7) { ADD_ZONE(lo + 1, hi - 1, gq[q].top); got++; for (int c = lo + 1; c <= hi - 1; c++) occ[c + 4] = 1; }
    }
  }
  SpikeCol spikeCols[8]; int nspike = 0; (void)spikeCols;
  Rng *T = &R;
  int gaps = 0; for (int c = 0; c < 20; c++) if (top[c] < 0) gaps++;

  if (want[M_GRAVITY]) for (int c = 7; c <= 13; c++) occ[c + 4] = 1;

  /* placeSpikes: put N items of glyph CH on flat spots */
  #define PLACE(N, CH, WIDE) do { \
    for (int _n = 0; _n < (N); _n++) { \
      int w = ((WIDE) && diff > 0.3 && rf(T) < 0.5) ? 2 : 1; \
      Spot sp[40]; int ns = spots(top, w, occ, sp); \
      if (ns == 0 && w == 2) { w = 1; ns = spots(top, 1, occ, sp); } \
      if (ns == 0) break; \
      Spot s = sp[ri(T, 1, ns) - 1]; \
      for (int kk = 0; kk < w; kk++) B.g[s.top - 1][s.s + kk] = (CH); \
      for (int kk = s.s - 2; kk <= s.s + w + 1; kk++) occ[kk + 4] = 1; \
      if ((CH) == '^' && nspike < 8) { spikeCols[nspike].s = s.s; spikeCols[nspike].w = w; spikeCols[nspike].top = s.top; nspike++; } \
    } } while (0)

  /* pops */
  if (want[M_POP]) {
    int n = 1 + (int)floor(diff * 2);
    for (int q = 0; q < n; q++) {
      Spot sp[40]; int ns = spots(top, 1, occ, sp);
      if (ns == 0) break;
      Spot s = sp[ri(T, 1, ns) - 1];
      if (L->npops < 4) { L->pops[L->npops].col = s.s; L->pops[L->npops].top = s.top; L->npops++; }
      for (int kk = s.s - 2; kk <= s.s + 2; kk++) occ[kk + 4] = 1;
    }
    L->on[M_POP] = L->npops > 0;
  }
  /* fake spikes (harmless, look deadly) */
  if (want[M_FAKES]) {
    int n = 1 + (int)floor(diff * 2); int placed = 0;
    for (int q = 0; q < n; q++) {
      Spot sp[40]; int ns = spots(top, 1, occ, sp);
      if (ns == 0) break;
      Spot s = sp[ri(T, 1, ns) - 1];
      if (B.g[s.top - 1][s.s] == '.') { B.g[s.top - 1][s.s] = 'f'; placed++; }
      for (int kk = s.s - 2; kk <= s.s + 2; kk++) occ[kk + 4] = 1;
    }
    L->on[M_FAKES] = placed > 0;
  }

  /* real spikes (always at least one) */
  int nSp = ri(T, 0, 1 + (int)floor(diff * 2));
  if (nSp == 0 && want[M_FAKES]) nSp = 1;   /* some levels have no real spikes at all */
  else if (nSp > 0 && !want[M_FAKES] && !want[M_POP] && rf(T) < 0.45) nSp = 0;
  PLACE(nSp, '^', 1);
  /* coins: always a couple, deadly or harmless */
  int coinsPlaced = 0;
  {
    int before = 0; for (int r = 0; r < 15; r++) for (int c = 0; c < 20; c++) if (B.g[r][c] == '*' || B.g[r][c] == 'c') before++;
    if (want[M_COINS] || rf(T) < 0.5) PLACE(2, want[M_COINS] ? '*' : 'c', 0);
    int after = 0; for (int r = 0; r < 15; r++) for (int c = 0; c < 20; c++) if (B.g[r][c] == '*' || B.g[r][c] == 'c') after++;
    coinsPlaced = after - before;
  }
  L->on[M_COINS] = want[M_COINS] && coinsPlaced > 0;

  if (want[M_CRUMBLE]) {
    Grp gp[20]; int ng = groups(top, gp); int any = 0;
    for (int gi = 0; gi < ng; gi++) {
      int hasStart = gp[gi].s <= 1 && gp[gi].e >= 1;
      int hasGoal = gp[gi].s <= 18 && gp[gi].e >= 18;
      if (gp[gi].e - gp[gi].s + 1 >= 2 && !hasStart && !hasGoal)
        for (int c = gp[gi].s; c <= gp[gi].e; c++) {
          for (int r = gp[gi].top; r <= 14; r++) if (B.g[r][c] == '#') { B.g[r][c] = '='; any = 1; }
        }
    }
    L->on[M_CRUMBLE] = any;
  }
  if (want[M_FLEE]) {
    B.g[top[18] - 1][18] = '.';
    int c0 = nearest_solid(top, 7);
    int c1 = nearest_solid(top, 12);
    if (c1 <= c0) c1 = (c0 + 1 < 17) ? c0 + 1 : 17;
    B.g[top[c0] - 1][c0] = 'G';
    { static const char *OBJS = "^vfF*cTqDOGZ"; char *h = &B.g[top[c1] - 1][c1]; if (*h && strchr(OBJS, *h)) *h = '.';   /* the hop target must be empty */
      if (top[c1] >= 2) { char *u = &B.g[top[c1] - 2][c1]; if (*u && strchr(OBJS, *u)) *u = '.'; }
      if (top[c0] >= 2) { char *u = &B.g[top[c0] - 2][c0]; if (*u && strchr(OBJS, *u)) *u = '.'; }
      int cs[3] = { c0, c1, 18 }; for (int q = 0; q < 3; q++) for (int kk = cs[q] - 1; kk <= cs[q] + 1; kk++) if (kk >= -3 && kk <= 19) occ[kk + 4] = 1; }
    L->flee[0].col = c0; L->flee[0].y = (top[c0] - 1) * 32; L->flee[0].t = 0;
    L->flee[1].col = c1; L->flee[1].y = (top[c1] - 1) * 32;
    L->flee[2].col = 18; L->flee[2].y = (top[18] - 1) * 32;
    L->nflee = 3;
    L->on[M_FLEE] = 1;
  }
  /* doors (all one look): the exit may be one of several, the others kill you. gates (another look) throw you elsewhere or kill you */
  int nonWin = 0;
  if (!want[M_FLEE]) {
    Rng DX; rng_init(&DX, seed + 9191);
    int dc[5], dr[5], nd = 0;
    dc[nd] = 18; dr[nd] = top[18] - 1; nd++;
    int nextra = 1 + (rf(&DX) < 0.55) + (diff > 0.05 && rf(&DX) < 0.3);
    for (int q = 0; q < nextra && nd < 4; q++) {
      int pool = 0, pc[20], hiC = 15;
      for (int c = 4; c <= hiC; c++) {
        if (top[c] < 0 || (top[c - 1] < 0 && top[c + 1] < 0)) continue;
        if (B.g[top[c] - 1][c] != '.' || occ[c + 3] || occ[c + 4] || occ[c + 5]) continue;
        int clash = 0; for (int e = 0; e < nd; e++) if (abs(dc[e] - c) < 3) clash = 1;
        if (!clash) pc[pool++] = c;
      }
      if (!pool) break;
      Spot s0; s0.s = pc[ri(&DX, 1, pool) - 1]; s0.top = top[s0.s];
      dc[nd] = s0.s; dr[nd] = s0.top - 1; nd++;
      for (int kk = s0.s - 2; kk <= s0.s + 2; kk++) occ[kk + 4] = 1;
    }
    int ty[5], anyWin = 0;
    for (int q = 0; q < nd; q++) {
      double u = rf(&DX);
      ty[q] = u < 0.35 ? 0 : (u < 0.68 ? 1 : 2);
      if (ty[q] == 0 && dc[q] < 8) ty[q] = 1 + (rf(&DX) < 0.45);   /* an exit right beside the start would be no game */
      anyWin |= ty[q] == 0;
    }
    if (!anyWin) { int el[5], ne = 0; for (int q = 0; q < nd; q++) if (dc[q] >= 8) el[ne++] = q; ty[el[ri(&DX, 0, ne - 1)]] = 0; }
    for (int q = 0; q < nd; q++) if (ty[q] == 1 && rf(&DX) < 0.4) ty[q] = 3;   /* a deadly one may be a gate */
    int allWin = 1; for (int q = 0; q < nd; q++) if (ty[q]) allWin = 0;
    if (want[M_DECOY] && allWin && nd > 1) ty[ri(&DX, 1, nd - 1)] = (rf(&DX) < 0.4) ? 2 : 1;
    if (want[M_DECOY] && nd == 1) { /* no room for a second door: keep the old single decoy beside the exit */
      if (top[17] >= 0 && B.g[top[17] - 1][17] == '.') { dc[nd] = 17; dr[nd] = top[17] - 1; ty[nd] = 1; nd++; }
    }
    for (int q = 0; q < nd; q++) { B.g[dr[q]][dc[q]] = ty[q] == 0 ? 'G' : (ty[q] == 1 ? 'D' : (ty[q] == 2 ? 'O' : 'Z')); nonWin |= ty[q] != 0; }
    L->nd = nd;
  }
  L->on[M_DECOY] = nonWin;
  /* lucky gate: on some levels a wall blocks the way (5 tiles, no jump clears it) and the only way across is a gate that throws you
   * past the wall, if your timing is lucky; otherwise it kills you and you try again. The levels beyond the wall stay as generated. */
  int lgOn = 0, lgGc = 0, lgDc = 0, lgDr = 0;
  if (opt.wallRun) {
    int cw = 6, ok = top[cw] >= 8;
    for (int c = cw - 2; ok && c <= cw + 2; c++) {
      if (top[c] != top[cw]) { ok = 0; break; }
      for (int r = top[cw] - 6; r < top[cw]; r++) if (B.g[r][c] != '.') ok = 0;
      for (int q = 0; q < L->npops; q++) if (L->pops[q].col == c) ok = 0;
      for (int z = 0; z < L->jk; z++) if (c >= L->jz[z].s && c <= L->jz[z].e) ok = 0;
    }
    if (ok) {
      int tp = top[cw];
      for (int r = tp - 5; r < tp; r++) B.g[r][cw] = '#';
      lgGc = cw - 2; lgDc = cw + 2; lgDr = tp - 1;
      B.g[tp - 1][lgGc] = 'L';
      for (int r = 0; r < 15; r++) for (int c = 0; c < 20; c++) if (B.g[r][c] == 'O') B.g[r][c] = 'Z';   /* a plain teleport gate would skip the wall */
      lgOn = 1;
    }
  }
  L->lucky = lgOn;
  /* invisible floors: the pits chosen above get an unseen floor tile */
  for (int q = 0; q < npit; q++) if (gh[q]) {
    int tp = pit[q].s > 0 ? top[pit[q].s - 1] : 13;
    for (int c = pit[q].s; c <= pit[q].e; c++) B.g[tp][c] = 'n';
  }
  L->on[M_INVIS] = nghost > 0;
  /* the clock: controls, speed, ceilings and ghost floors all flip on their own random schedule */
  {
    Rng TX; rng_init(&TX, seed + 4747);
    L->P = 5.0 + rf(&TX) * 4.0;
    if (lgOn) { Rng LX; rng_init(&LX, seed + 7373); L->lgp = L->P / 2; double wl = L->lgp * (0.45 + 0.15 * rf(&LX)); L->lga = rf(&LX) * (L->lgp - wl); L->lgb = L->lga + wl; }
    if (want[M_INVERT]) {
      double t = 0.5 + rf(&TX);
      while (L->ninv < 6 && t < L->P - 0.6) { L->inv[L->ninv++] = t; t += 1.3 + rf(&TX) * 1.4; }
      L->on[M_INVERT] = L->ninv > 0;
    }
    if (want[M_SPEED]) {
      L->spdA = 0.4 + rf(&TX) * 1.1; L->spdB = L->spdA + 1.6 + rf(&TX) * 2.2;
      if (L->spdB > L->P - 0.5) L->spdB = L->P - 0.5;
      L->nspd = 1;
    }
    for (int q = 0; q < L->npops; q++) {
      double len = 1.2 + rf(&TX) * 1.8; if (len > L->P - 1.0) len = L->P - 1.0;
      L->pops[q].a = rf(&TX) * (L->P - len); L->pops[q].b = L->pops[q].a + len;
    }
    if (want[M_GRAVITY]) {
      double a = 0.6 + rf(&TX), len = 1.8 + rf(&TX) * 1.4;
      if (a + len > L->P - 1.0) len = L->P - 1.0 - a;
      L->gw[0][0] = a; L->gw[0][1] = a + len; L->ngw = 1;
      double a2 = a + len + 1.2 + rf(&TX), len2 = 1.5 + rf(&TX) * 1.2;
      if (L->P > 6.5 && rf(&TX) < 0.6 && a2 + len2 < L->P - 0.3) { L->gw[1][0] = a2; L->gw[1][1] = a2 + len2; L->ngw = 2; }
    }
    if (want[M_FLEE] && L->nflee == 3) {
      L->flee[1].t = 1.2 + rf(&TX) * 1.3; L->flee[2].t = L->flee[1].t + 1.4 + rf(&TX) * 1.4;
      if (L->flee[2].t > L->P - 1.0) L->flee[2].t = L->P - 1.0;
      double t2n = L->flee[1].t + 1.4 + rf(&TX) * 1.2, t3n = t2n + 1.4 + rf(&TX) * 1.2;
      if (L->P > 6.5 && t3n < L->P - 0.8 && rf(&TX) < 0.5) {   /* come back once before the exit settles at the end */
        L->flee[3] = L->flee[2]; L->flee[3].t = t3n;
        L->flee[2] = L->flee[0]; L->flee[2].t = t2n;
        L->nflee = 4;
      }
    }
    for (int z = 0; z < L->jk; z++) {
      if (rf(&TX) < 0.35) { L->jz[z].a = 0; L->jz[z].b = L->P + 1; }
      else { L->jz[z].a = rf(&TX) * (L->P - 2.0); L->jz[z].b = L->jz[z].a + 1.5 + rf(&TX) * 2.5; }
    }
    for (int g = 0; g < L->ngp; g++) {
      L->gp[g].blink = !want[M_GRAVITY] && rf(&TX) < 0.5;
      double len = L->P - 1.2; double want2 = 2.6 + rf(&TX) * 2.0; if (want2 < len) len = want2;
      L->gp[g].a = rf(&TX) * (L->P - len); L->gp[g].b = L->gp[g].a + len;
    }
  }
  L->on[M_GRAVITY] = want[M_GRAVITY];
  L->on[M_SPEED] = want[M_SPEED];
  L->on[M_ARROW] = mir;

  /* decorative props (no effect on play, but they are claim material); never on top of anything */
  #define PLACE_PROP(CH) do { Spot sp[40]; int ns = spots(top, 1, occ, sp); \
    for (int _t = 0; ns > 0 && _t < 10; _t++) { Spot s = sp[ri(T, 1, ns) - 1]; \
      if (B.g[s.top - 1][s.s] == '.') { B.g[s.top - 1][s.s] = (CH); break; } } } while (0)
  if (rf(T) < 0.55) { PLACE_PROP('T'); if (rf(T) < 0.4) PLACE_PROP('T'); }
  (void)rf(T);   /* crystals were removed; keep the random stream the same */

  /* ---- final transforms */
  if (mir) {
    for (int z = 0; z < L->jk; z++) { int s2 = 19 - L->jz[z].e, e2 = 19 - L->jz[z].s; L->jz[z].s = s2; L->jz[z].e = e2; }
    for (int g = 0; g < L->ngp; g++) { int s2 = 19 - L->gp[g].e, e2 = 19 - L->gp[g].s; L->gp[g].s = s2; L->gp[g].e = e2; }
  }
  if (mir) {
    for (int r = 0; r < 15; r++) for (int c = 0; c < 10; c++) {
      char t = B.g[r][c]; B.g[r][c] = B.g[r][19 - c]; B.g[r][19 - c] = t;
    }
  }
  if (want[M_GRAVITY]) {   /* a ceiling to fall up to, with a few spikes hanging from it */
    for (int c = 0; c < 20; c++) { B.g[0][c] = '#'; B.g[1][c] = '#'; }
    int ncs = ri(T, 0, 2);
    for (int q = 0; q < ncs; q++) { int c = ri(T, 4, 15); if (B.g[2][c] == '.' && B.g[2][c - 1] != 'v' && B.g[2][c + 1] != 'v') B.g[2][c] = 'v'; }
  }
  for (int r = 0; r < 15; r++) { memcpy(L->map[r], B.g[r], 20); L->map[r][20] = 0; }

  /* door table (looks are shuffled per level; portals pick a safe standing spot somewhere else) */
  {
    Rng SX; rng_init(&SX, seed + 5151);
    L->nd = 0;
    for (int c = 0; c < 20; c++) for (int r = 0; r < 15; r++) {
      char ch = L->map[r][c];
      if ((ch == 'G' || ch == 'D' || ch == 'O' || ch == 'Z' || ch == 'L') && L->nd < 5) {
        int d = L->nd++;
        L->door[d].c = c; L->door[d].r = r;
        L->door[d].type = ch == 'G' ? 0 : (ch == 'D' ? 1 : (ch == 'O' ? 2 : (ch == 'L' ? 4 : 3)));
        L->door[d].style = (ch == 'O' || ch == 'Z' || ch == 'L') ? 3 : 0;
        if (ch == 'L') { L->door[d].dc = mir ? 19 - lgDc : lgDc; L->door[d].dr = lgDr; }   /* every door is wooden, every gate looks the same */
      }
    }
    for (int d = 0; d < L->nd; d++) if (L->door[d].type == 2) {
      int cc[300], cr[300], n = 0;
      for (int r = 1; r < 14; r++) for (int c = 2; c <= 17; c++) {
        if (L->map[r][c] != '.' || L->map[r + 1][c] != '#') continue;
        if (strchr("^*DOGZLvfF", L->map[r][c - 1]) || strchr("^*DOGZLvfF", L->map[r][c + 1])) continue;
        int near = 0;
        for (int e = 0; e < L->nd; e++) if (abs(L->door[e].c - c) <= 2 && abs(L->door[e].r - r) <= 2) near = 1;
        if (near || n >= 300) continue;
        cc[n] = c; cr[n] = r; n++;
      }
      if (n == 0) { L->door[d].type = 3; L->map[L->door[d].r][L->door[d].c] = 'Z'; continue; }
      int pick = ri(&SX, 0, n - 1);
      L->door[d].dc = cc[pick]; L->door[d].dr = cr[pick];
    }
  }

  /* ---- facts about the finished level */
  int nSpikes = L->npops, nHaz = L->npops;
  for (int r = 0; r < 15; r++) for (int c = 0; c < 20; c++) {
    char ch = L->map[r][c];
    if (ch == '^' || ch == 'v') {   /* only spikes standing on ground; pit-bottom spikes are not counted */
      int sup = (ch == '^') ? (r < 14 && strchr("#=n", L->map[r + 1][c])) : (r > 0 && strchr("#=n", L->map[r - 1][c]));
      if (sup) { nSpikes++; nHaz++; }
    }
    else if (ch == '*' || ch == 'D' || ch == 'Z') nHaz++;
  }
  if (!L->on[M_INVIS]) nHaz += gaps;
  int has[C_N]; memset(has, 0, sizeof has);
  for (int m = 0; m < M_N; m++) has[m] = L->on[m];
  Rng X; rng_init(&X, seed + 31337);
  L->deathOff = (rf(&X) < 0.30) ? 0 : (int[]){ 1, 2, 3, 4, 6, 9 }[ri(&X, 0, 5)];
  int rowG = -1, rowP = -1, nTorch = 0, nCrystal = 0, nCoin = 0;
  for (int r = 0; r < 15; r++) for (int c = 0; c < 20; c++) {
    char ch = L->map[r][c];
    if (ch == 'G') rowG = r; else if (ch == 'P') rowP = r;
    else if (ch == 'T') nTorch++; else if (ch == 'q') nCrystal++; else if (ch == 'c' || ch == '*') nCoin++;
  }
  (void)nHaz;
  has[F_SPIKES] = nSpikes > 0; has[F_PITS] = gaps > 0; has[F_HIGH] = rowG >= 0 && rowP >= 0 && rowG < rowP;
  has[F_TORCH] = nTorch > 0; has[F_CRYSTAL] = nCrystal > 0; has[F_COINS] = nCoin > 0; has[F_CTR] = (L->deathOff == 0);
  has[F_NUMB] = 1; has[F_COUNT] = 1; has[C_JK] = L->jk > 0;
  { int tp = 0; for (int d = 0; d < L->nd; d++) tp |= L->door[d].type == 2 || L->door[d].type == 4; has[C_TP] = tp; }
  /* claims that would be vacuous are left out */
  int skip[C_N]; memset(skip, 0, sizeof skip);
  if (nCoin == 0) skip[M_COINS] = 1;
  if (nSpikes == 0) skip[M_FAKES] = 1;
  skip[F_CRYSTAL] = 1;   /* crystals were removed from the game */

  /* pick which claims appear: all real behaviours, then random fill */
  int inc[C_N]; memset(inc, 0, sizeof inc);
  int ninc = 0;
  for (int m = 0; m < M_N; m++) if (L->on[m]) { inc[m] = 1; ninc++; }
  int target = 11 + (rf(&X) < 0.5) + (rf(&X) < 0.3);
  for (int tries = 0; tries < 200 && ninc < target; tries++) {
    int c = ri(&X, 0, C_N - 1);
    if (inc[c] || skip[c]) continue;
    inc[c] = 1; ninc++;
  }
  int list[C_N], nl = 0;
  for (int c = 0; c < C_N; c++) if (inc[c]) list[nl++] = c;
  for (int a = nl - 1; a > 0; a--) { int b2 = ri(&X, 0, a); int t = list[a]; list[a] = list[b2]; list[b2] = t; }
  int lie[C_N]; int lies = 0;
  for (int a = 0; a < nl; a++) { lie[a] = rf(&X) < 0.72; lies += lie[a]; }
  /* guarantees: >= 6 lies and >= 1 truth */
  if (lies == nl) { lie[0] = 0; lies--; }
  for (int a = 0; a < nl && lies < 6; a++) if (!lie[a] && (nl - lies) > 1) { lie[a] = 1; lies++; }

  Rng V; rng_init(&V, seed + 8080);
  L->nclaims = 0; L->nlies = 0;
  for (int a = 0; a < nl; a++) {
    int c = list[a];
    int ass = lie[a] ? !has[c] : has[c];
    int v = ri(&V, 0, 2);
    char text[64], real[28]; real[0] = 0; int shownNum = -1;
    if (c == F_NUMB) {
      int n = i;
      if (lie[a]) { int d = ri(&V, 1, 9); n = (ri(&V, 0, 1) && i - d >= 1) ? i - d : i + d; }
      shownNum = n;
      snprintf(text, sizeof text, v == 0 ? "This is level %d." : (v == 1 ? "You are on level %d." : "Welcome to level %d."), n);
      if (lie[a]) snprintf(real, sizeof real, "It was level %d.", i);
    } else if (c == F_COUNT) {
      int n = nSpikes;
      if (lie[a]) { int d = ri(&V, 1, 3); n = (ri(&V, 0, 1) && nSpikes - d >= 0) ? nSpikes - d : nSpikes + d; }
      shownNum = n;
      snprintf(text, sizeof text, v == 0 ? "There are exactly %d deadly spikes." : (v == 1 ? "Count the spikes: %d." : "%d spikes in total."), n);
      if (lie[a]) snprintf(real, sizeof real, "There were %d.", nSpikes);
    } else {
      snprintf(text, sizeof text, "%s", CTXT[c][ass][v]);
      if (lie[a]) {
        if (c < M_N) snprintf(real, sizeof real, "%s", CREAL[c][has[c]]);
        else if (c <= F_CTR) snprintf(real, sizeof real, "%s", FREAL[c - F_SPIKES][has[c]]);
      }
    }
    int ic = (c == F_HIGH) ? IC_HIGH : (c == F_TORCH) ? IC_TORCH : (c == F_CRYSTAL) ? IC_CRYSTAL : (c == F_COINS) ? IC_COINS : (c == C_JK) ? IC_JK : (c == C_TP) ? IC_TP : c;
    add_claim(L, text, real, lie[a], ic, ass, (c == F_NUMB || c == F_COUNT) ? shownNum : -1, (c == F_NUMB) ? i : nSpikes);
    /* the arrow claim is also painted on the level itself */
    if (c == M_ARROW) {
      /* dead centre of the screen: the pair (sign 36 px + gap 8 + chevrons 20) is centred on y = 240 */
      add_sign(&B, 320, 208, SG_EXIT, ass);
      add_sign(&B, 320, 252, SG_CHEV, ass);
    }
  }
  L->tip = ri(&X, 0, L->nclaims - 1);
  /* ARROW signs were added after mirroring happened; add_sign mirrors cx itself (320 stays 320) */
  snprintf(L->title, sizeof L->title, "Level %d", i);
  L->kind = 0;
}

/* the level's main behaviour must really be present; retry the layout until it is */
static void build_platform(int i, Level *L) {
  int p = primary_of(i);
  for (int attempt = 0; attempt < 40; attempt++) {
    build_try(i, L, attempt);
    if (L->on[p]) return;
  }
}

/* ------------------------------------------------------------------ trick screens */
static const char *SCR_LIES[] = {
  "Press any button to continue.", "Nothing on this screen is a trick.", "Hold START to skip.",
  "Wrong inputs cost a life.", "Pressing A solves everything.", "Sound matters here.",
  "This is a normal platform level.", "This screen has a time limit.", "The next level is the last one.",
  "Failing sends you back to level 1.",
};
/* picture for each screen claim: icon id, and whether it asserts the thing happens (1) or not (0) */
enum { IC_BUTTON = 22, IC_START, IC_HEART, IC_SOUND, IC_PLATFORM, IC_QMARK, IC_RESET, IC_STAR };
static const int SCR_LIE_ICON[10] = { IC_BUTTON, IC_QMARK, IC_START, IC_HEART, IC_STAR, IC_SOUND, IC_PLATFORM, 14, 13, IC_RESET };
static const int SCR_LIE_ASS[10]  = { 1, 0, 1, 1, 1, 1, 1, 1, 1, 1 };
static const int SCR_TRUTH_ICON[5] = { 15, IC_QMARK, IC_PLATFORM, 11, 12 };
static const int SCR_TRUTH_ASS[5]  = { 0, 1, 0, 0, 0 };
static const char *SCR_TRUTHS[] = {
  "You cannot die here.", "This screen has a trick.", "This is not a platform level.",
  "There are no spikes here.", "There are no pits here.",
};
#define N_SCR_LIES ((int)(sizeof SCR_LIES / sizeof *SCR_LIES))
#define N_SCR_TRUTHS ((int)(sizeof SCR_TRUTHS / sizeof *SCR_TRUTHS))

static void build_screen(int i, Level *L) {
  memset(L, 0, sizeof *L);
  static const int kinds[5] = { 0, 1, 2, 0, 3 };  /* pressX, waitIt, mash, pressX, fakeEnd */
  static const char *labels[4] = { "A", "B", "START", "X" };
  int sc = i / 10;
  L->kind = 1; L->idx = i; L->world = (i - 1) / PER + 1; L->k = (i - 1) % PER + 1;
  L->scrKind = kinds[(sc - 1) % 5];
  L->scrTruth = (sc - 1) % 4;  /* 0 down, 1 up, 2 left, 3 right */
  strcpy(L->scrLabel, labels[(sc - 1) % 4]);
  snprintf(L->title, sizeof L->title, "Level %d", i);
  Rng X; rng_init(&X, (long long)i * 7919 + 5);
  int nT = 2 + (rf(&X) < 0.4 ? 1 : 0), nL = 6 + (rf(&X) < 0.5 ? 1 : 0);
  int li[N_SCR_LIES], ti[N_SCR_TRUTHS];
  for (int a = 0; a < N_SCR_LIES; a++) li[a] = a;
  for (int a = 0; a < N_SCR_TRUTHS; a++) ti[a] = a;
  for (int a = N_SCR_LIES - 1; a > 0; a--) { int b = ri(&X, 0, a); int t = li[a]; li[a] = li[b]; li[b] = t; }
  for (int a = N_SCR_TRUTHS - 1; a > 0; a--) { int b = ri(&X, 0, a); int t = ti[a]; ti[a] = ti[b]; ti[b] = t; }
  int nl = 0, nt = 0, tot = nL + nT, pos[16];
  for (int a = 0; a < tot; a++) pos[a] = a;
  for (int a = tot - 1; a > 0; a--) { int b = ri(&X, 0, a); int t = pos[a]; pos[a] = pos[b]; pos[b] = t; }
  /* pos[a] < nL  -> lie slot, else truth slot; shuffled order */
  for (int a = 0; a < tot; a++) {
    if (pos[a] < nL) {
      int s = li[nl++];
      /* "press any button" is accidentally true on the fake-ending screen */
      if (L->scrKind == 3 && s == 0) s = li[nl++];
      add_claim(L, SCR_LIES[s], "", 1, SCR_LIE_ICON[s], SCR_LIE_ASS[s], -1, -1);
    } else { int q = ti[nt++]; add_claim(L, SCR_TRUTHS[q], "", 0, SCR_TRUTH_ICON[q], SCR_TRUTH_ASS[q], -1, -1); }
  }
  L->tip = 0;
}

static void level_get(int i, Level *L) {
  if (i % 10 == 0) build_screen(i, L); else build_platform(i, L);
}

#endif

/* Platformer world: tile map -> entities, player physics, lie hooks. Port of world.lua. */
#ifndef WORLD_H
#define WORLD_H
#include "levels.h"

#define TS 32
#define PW 20
#define PH 28
#define GRAV 1600.0
#define JUMP 560.0
#define RUN 190.0
#define ACC 1500.0
#define AIR 1000.0
#define MAXF 720.0

enum { K_SOLID, K_HAZARD, K_GOAL, K_NONE, K_TELE, K_LUCK };
enum { LK_GROUND, LK_SPIKE, LK_SPIKEDOWN, LK_COIN, LK_DOOR, LK_DOOR2, LK_NONE, LK_TORCH, LK_CRYSTAL };

typedef struct {
  double x, y, w, h;
  int kind, look, active;
  int hasd; double dx, dy, dw, dh;
  int style, popi; double tpx, tpy;   /* door look; portal destination */
  int top, crumble, flip, gi;   /* gi: 1+index of the blinking ghost pit this tile belongs to */
  double t, fallY, fv; int falling, hast;
  double tx; int hastx;
  int fx, fxc; double ph;   /* torch: fx=1 honest light, ph=flicker offset. crystal: fx=effect (0 door, 1 slow, 2 claim), fxc=claim */
} Ent;

typedef struct {
  double x, y, w, h, vx, vy;
  int ground; double coy, buf; int face;
} Player;

typedef struct {
  Ent ents[420]; int nent;
  Player p;
  const Level *lv;
  int goal;           /* index into ents or -1 */
  double t;
  int gravity, invert, jumped; double speed;
  int dead, won;
  Pop pops[4]; int fleeStage;
  int jkShown, teleShown;
  double slowT; int crEv, crI;   /* crystal: slow-clock seconds left; pick-up event (effect+1) for the game loop */
  int cry[4], ncry;
  int newCoins; double coinX, coinY;   /* harmless coins picked up since the game loop last looked */
  int sfxPop, sfxJump; /* event flags consumed by the game loop */
} World;

typedef struct { int left, right, jump; } Held;

static Ent *w_add(World *w, double x, double y, double ww, double hh, int kind, int look) {
  Ent *e = &w->ents[w->nent++];
  memset(e, 0, sizeof *e);
  e->x = x; e->y = y; e->w = ww; e->h = hh; e->kind = kind; e->look = look; e->active = 1;
  return e;
}
static void w_setd(Ent *e, double x, double y, double ww, double hh) {
  e->hasd = 1; e->dx = x; e->dy = y; e->dw = ww; e->dh = hh;
}

static unsigned wh(unsigned a, unsigned b, unsigned c) {
  unsigned h = a * 2654435761u ^ (b + 17) * 40503u ^ (c + 3) * 9973u;
  h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15; return h;
}
static int torch_lit(const World *w, const Ent *e) { return fmod(w->t + e->ph, 4.0) < 2.6; }

static int is_solidch(char c) { return c == '#' || c == '=' || c == 'n'; }

static void world_init(World *w, const Level *lv) {
  memset(w, 0, sizeof *w);
  w->lv = lv; w->gravity = 1; w->speed = 1; w->goal = -1;
  double px = 40, py = 100;
  for (int r = 0; r < 15; r++) for (int c = 0; c < 20; c++) {
    char ch = lv->map[r][c];
    double x = c * TS, y = r * TS;
    char above = r > 0 ? lv->map[r - 1][c] : '.';
    Ent *e;
    switch (ch) {
      case '#': case '=':
        e = w_add(w, x, y, TS, TS, K_SOLID, LK_GROUND);
        e->top = !is_solidch(above); e->crumble = (ch == '=');
        if (lv->on[M_GRAVITY] && r <= 1) {   /* the ceiling: its lit edge faces down */
          char below = r < 14 ? lv->map[r + 1][c] : '#';
          e->top = !is_solidch(below); e->flip = 1;
        }
        break;
      case 'n':
        e = w_add(w, x, y, TS, TS, K_SOLID, LK_NONE);
        for (int g = 0; g < lv->ngp; g++) if (lv->gp[g].blink && c >= lv->gp[g].s && c <= lv->gp[g].e) e->gi = g + 1;
        break;
      case '^': e = w_add(w, x + 6, y + 12, 20, 20, K_HAZARD, LK_SPIKE); w_setd(e, x, y, TS, TS); break;
      case 'v': e = w_add(w, x + 6, y, 20, 20, K_HAZARD, LK_SPIKEDOWN); w_setd(e, x, y, TS, TS); break;
      case 'f': e = w_add(w, x + 6, y + 12, 20, 20, K_NONE, LK_SPIKE); w_setd(e, x, y, TS, TS); break;
      case 'F': e = w_add(w, x + 6, y, 20, 20, K_NONE, LK_SPIKEDOWN); w_setd(e, x, y, TS, TS); break;
      case '*': e = w_add(w, x + 8, y + 8, 16, 16, K_HAZARD, LK_COIN); w_setd(e, x, y, TS, TS); break;
      case 'T': case 'q':
        e = w_add(w, x, y, TS, TS, K_NONE, ch == 'T' ? LK_TORCH : LK_CRYSTAL);
        { unsigned h = wh(lv->idx, c, r);
          if (ch == 'T') { e->fx = (h & 1); e->ph = ((h >> 4) % 40) / 10.0; }
          else {
            e->fx = 2;   /* a crystal only ever tells one claim's truth; effects 0 (reveal exit) and 1 (slow clock) are kept for tests (TRUST_FX) */
            e->fxc = lv->nclaims > 0 ? (int)((h >> 9) % lv->nclaims) : 0;
            if (w->ncry < 4) w->cry[w->ncry++] = w->nent - 1;
          } }
        break;
      case 'c': e = w_add(w, x + 8, y + 8, 16, 16, K_NONE, LK_COIN); w_setd(e, x, y, TS, TS); break;
      case 'D': case 'O': case 'G': case 'Z': case 'L': {
        int st = 0, dc = 0, dr = 0;
        for (int d = 0; d < lv->nd; d++) if (lv->door[d].c == c && lv->door[d].r == r) { st = lv->door[d].style; dc = lv->door[d].dc; dr = lv->door[d].dr; }
        if (ch == 'G') { e = w_add(w, x + 2, y, 28, TS, K_GOAL, LK_DOOR2); }
        else if (ch == 'D' || ch == 'Z') { e = w_add(w, x + 4, y + 2, 24, 30, K_HAZARD, LK_DOOR); w_setd(e, x + 2, y, 28, TS); }
        else { e = w_add(w, x + 4, y + 2, 24, 30, ch == 'L' ? K_LUCK : K_TELE, LK_DOOR); w_setd(e, x + 2, y, 28, TS); e->tpx = dc * TS + 6; e->tpy = (dr + 1) * TS - PH; }
        e->style = st;
        break;
      }
      case 'P': px = x + 6; py = y + TS - PH; break;
      default: break;
    }
  }
  { double best = -1e9;   /* heuristic goal: the exit that lies furthest along the way */
    for (int i = 0; i < w->nent; i++) if (w->ents[i].kind == K_GOAL) {
      double pr = lv->dir > 0 ? w->ents[i].x : -w->ents[i].x;
      if (pr > best) { best = pr; w->goal = i; }
    } }
  w->p.x = px; w->p.y = py; w->p.w = PW; w->p.h = PH; w->p.face = 1;
  for (int i = 0; i < lv->npops; i++) {   /* hidden spikes: present in the world, but only up while the clock says so */
    int col = lv->mir ? 19 - lv->pops[i].col : lv->pops[i].col;
    double x = col * 32, y = (lv->pops[i].top - 1) * 32;
    if (w->nent < 410) { Ent *e = w_add(w, x + 6, y + 12, 20, 20, K_HAZARD, LK_SPIKE); w_setd(e, x, y, 32, 32); e->popi = i + 1; e->active = 0; }
  }
}

static int ov(double ax, double ay, double aw, double ah, const Ent *b) {
  return ax < b->x + b->w && b->x < ax + aw && ay < b->y + b->h && b->y < ay + ah;
}

static void world_step(World *w, double dt, const Held *h) {
  Player *p = &w->p;
  const Level *lv = w->lv;
  if (w->slowT > 0) { w->t += dt * 0.5; w->slowT -= dt; if (w->slowT < 0) w->slowT = 0; }   /* crystal: the clock crawls */
  else w->t += dt;
  double ph = lv->P > 0 ? fmod(w->t, lv->P) : 0;
  if (lv->P > 0) {   /* the clock: controls, speed and ghost floors flip on their own schedule */
    int n = 0; for (int k = 0; k < lv->ninv; k++) if (lv->inv[k] <= ph) n++;
    int inv = n & 1, sp = (lv->nspd && ph >= lv->spdA && ph < lv->spdB);
    if (inv != w->invert || (sp ? 1.4 : 1.0) != w->speed) w->sfxPop = 1;
    w->invert = inv; w->speed = sp ? 1.4 : 1.0;
    int gv = 1; for (int k = 0; k < lv->ngw; k++) if (ph >= lv->gw[k][0] && ph < lv->gw[k][1]) gv = -1;
    if (gv != w->gravity) { w->gravity = gv; p->ground = 0; p->coy = 0; w->sfxPop = 1; }
    if (lv->on[M_FLEE] && w->goal >= 0 && lv->nflee > 0) {
      int k = 0; for (int j = 0; j < lv->nflee; j++) if (lv->flee[j].t <= ph) k = j;
      Ent *g = &w->ents[w->goal];
      double nx = lv->flee[k].col * 32 + 2;
      if (g->x != nx || g->y != lv->flee[k].y) { g->x = nx; g->y = lv->flee[k].y; w->sfxPop = 1; }
    }
    for (int i = 0; i < w->nent; i++) {
      Ent *e = &w->ents[i];
      if (e->popi) {
        const Pop *pp = &lv->pops[e->popi - 1];
        int on = ph >= pp->a && ph < pp->b;
        if (on != e->active) { e->active = on; if (on) w->sfxPop = 1; }
        continue;
      }
      if (!e->gi) continue;
      int on = ph >= lv->gp[e->gi - 1].a && ph < lv->gp[e->gi - 1].b;
      if (!on) e->active = 0;
      else if (!e->active && !ov(p->x, p->y, p->w, p->h, e)) e->active = 1;
    }
  }
  int l = h->left, r = h->right;
  if (w->invert) { int t = l; l = r; r = t; }
  int dir = (r ? 1 : 0) - (l ? 1 : 0);
  if (dir != 0) p->face = dir;
  double tv = dir * RUN * w->speed;
  double a = (p->ground ? ACC : AIR) * w->speed;
  if (p->vx < tv) { p->vx = p->vx + a * dt; if (p->vx > tv) p->vx = tv; }
  else { p->vx = p->vx - a * dt; if (p->vx < tv) p->vx = tv; }

  int gs = w->gravity;
  double g = GRAV;
  if (p->vy * gs < 0 && !h->jump) g = g * 2.4;
  p->vy = p->vy + g * gs * dt;
  if (p->vy * gs > MAXF) p->vy = MAXF * gs;
  p->buf -= dt; p->coy -= dt;
  if (p->buf > 0 && p->coy > 0) {
    p->vy = -JUMP * gs;
    p->buf = 0; p->coy = 0; p->ground = 0;
    w->jumped = 1; w->sfxJump = 1;
  }

  p->x += p->vx * dt;
  for (int i = 0; i < w->nent; i++) {
    Ent *e = &w->ents[i];
    if (e->kind == K_SOLID && e->active && ov(p->x, p->y, p->w, p->h, e)) {
      if (p->vx > 0) p->x = e->x - p->w; else if (p->vx < 0) p->x = e->x + e->w;
      p->vx = 0;
    }
  }
  p->y += p->vy * dt;
  p->ground = 0;
  for (int i = 0; i < w->nent; i++) {
    Ent *e = &w->ents[i];
    if (e->kind == K_SOLID && e->active && ov(p->x, p->y, p->w, p->h, e)) {
      if (p->vy > 0) { p->y = e->y - p->h; if (gs == 1) p->ground = 1; }
      else if (p->vy < 0) { p->y = e->y + e->h; if (gs == -1) p->ground = 1; }
      p->vy = 0;
    }
  }
  if (p->ground) p->coy = 0.09;

  if (p->x < 0) { p->x = 0; p->vx = 0; } else if (p->x > 640 - p->w) { p->x = 640 - p->w; p->vx = 0; }
  if (p->y > 520 || p->y < -120) { w->dead = 1; return; }

  if (lv->jk && !p->ground) {   /* hidden ceiling: leaving the floor inside an armed zone is fatal */
    double feet = p->y + p->h, pc = p->x + p->w / 2;
    for (int z = 0; z < lv->jk; z++) {
      double fl = lv->jz[z].row * 32.0;
      if (ph >= lv->jz[z].a && ph < lv->jz[z].b && pc > lv->jz[z].s * 32 && pc < (lv->jz[z].e + 1) * 32 && feet < fl - 38 && feet > fl - 300) {
        w->dead = 1; w->jkShown = z + 1; w->sfxPop = 1; return;
      }
    }
  }
  double hx = p->x + 3, hy = p->y + 3, hw = p->w - 6, hh = p->h - 6;
  for (int i = 0; i < w->nent; i++) {
    Ent *e = &w->ents[i];
    if (e->kind == K_HAZARD && e->active && ov(hx, hy, hw, hh, e)) { w->dead = 1; return; }
  }
  for (int i = 0; i < w->nent; i++) {
    Ent *e = &w->ents[i];
    if (e->active && e->kind == K_NONE && e->look == LK_COIN && ov(hx, hy, hw, hh, e)) {   /* a harmless coin: take it */
      e->active = 0; w->newCoins++; w->coinX = e->x + 8; w->coinY = e->y + 8;
    }
  }
  for (int i = 0; i < w->nent; i++) {
    Ent *e = &w->ents[i];
    if (e->active && e->look == LK_CRYSTAL && ov(hx, hy, hw, hh, e)) {   /* a crystal: one of three surprises */
      e->active = 0; w->crEv = e->fx + 1; w->crI = i;
      if (e->fx == 1) w->slowT = 4.0;
    }
  }
  for (int i = 0; i < w->nent; i++) {
    Ent *e = &w->ents[i];
    if (!e->active) continue;
    if (e->kind == K_GOAL && ov(hx, hy, hw, hh, e)) { w->won = 1; return; }
    if (e->kind == K_LUCK && ov(hx, hy, hw, hh, e)) {   /* a lucky gate: it throws you past the wall, or kills you, depending on the moment you touch it */
      double lph = fmod(w->t, w->lv->lgp);
      if (lph >= w->lv->lga && lph < w->lv->lgb) {
        p->x = e->tpx; p->y = e->tpy; p->vx = 0; p->vy = 0; p->ground = 0; p->coy = 0; w->sfxPop = 1; w->teleShown = i + 1; return;
      }
      w->dead = 1; return;
    }
    if (e->kind == K_TELE && ov(hx, hy, hw, hh, e)) {   /* a door that throws you somewhere else in the level */
      p->x = e->tpx; p->y = e->tpy; p->vx = 0; p->vy = 0; p->ground = 0; p->coy = 0; w->sfxPop = 1; w->teleShown = i + 1; return;
    }
  }
}

static int mcol(const World *w, int c) { return w->lv->mir ? 19 - c : c; }

/* runtime behaviours (the "hooks"). solver=1 skips the ones the solver treats as static */
static void world_hooks(World *w, double dt, int solver) {
  const Level *lv = w->lv;
  Player *p = &w->p;
  double pcx = p->x + p->w / 2;
  double prog = lv->dir > 0 ? pcx : 640.0 - pcx;   /* distance travelled towards the exit */
  (void)lv; (void)p; (void)prog; (void)dt; (void)solver; (void)w;   /* everything now runs on the clock inside world_step */
}

static void world_update(World *w, double dt, const Held *h, int jumpPressed) {
  if (w->dead || w->won) return;
  Player *p = &w->p;
  if (jumpPressed) p->buf = 0.12;
  int steps = (int)ceil(dt / (1.0 / 120.0)); if (steps < 1) steps = 1;
  double sub = dt / steps;
  for (int i = 0; i < steps; i++) { world_step(w, sub, h); if (w->dead || w->won) break; }
  for (int i = 0; i < w->nent; i++) {
    Ent *e = &w->ents[i];
    if (!e->crumble) continue;
    if (e->active) {
      if (e->hast) {
        e->t -= dt;
        if (e->t <= 0) { e->active = 0; e->falling = 1; e->fallY = 0; e->fv = 0; }
      } else if (p->ground && p->x + p->w > e->x + 2 && p->x < e->x + e->w - 2 &&
                 fabs((p->y + p->h) - e->y) < 2) {
        e->t = 0.22; e->hast = 1;
      }
    } else if (e->falling) {
      e->fv += 1400 * dt;
      e->fallY += e->fv * dt;
    }
  }
  world_hooks(w, dt, 0);
}

#endif

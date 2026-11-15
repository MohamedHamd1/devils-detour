# Devil's Detour: development notes

Handoff notes so work can continue later. Read `PROJECT_HISTORY.md` first (full story, restart guide), then this file, then `source/src/`.

## What this is
"Every level lies to you" trick-platformer for PortMaster on Anbernic RG34XX SP / RG35XX Pro (muOS).
C + SDL2, cross-compiled for aarch64 (needs only GLIBC_2.17, links the device's libSDL2).
550 levels = 11 worlds x 50; every 10th level is a trick screen (55), the other 495 are platform levels.
It was rebuilt in C because the user's PortMaster has no Love2D runtime.

## Folder layout (Desktop/troll)
- `devilsdetour/devilsdetour`: aarch64 binary (copy the whole `devilsdetour/` folder to `ports/` on the SD card)
- `Devils Detour.sh`: launcher (copy to `ROMS/Ports/`)
- `devilsdetour.zip`: same package zipped
- `port.json`, `gameinfo.xml`, `README.md`: PortMaster metadata and player-facing readme
- `source/src/`: `trust.c` (game loop, drawing, test tooling), `levels.h` (generation, claims), `world.h` (physics, rules), `art.h` (procedural pixel art), `font_data.c/.h`
- `source/inc/`, `source/build_arm.sh`, `source/gen_font.py`: build pieces
- (the first Love2D version was deleted at the user's request)
- `DEVELOPMENT.md`: this file

## Build
- ARM: `cd source && ./build_arm.sh` (needs `pip install ziglang`, `apt install libsdl2-dev`). Uses
  `python3 -m ziglang cc -target aarch64-linux-gnu.2.17` and a stub libSDL2 so the binary records DT_NEEDED only.
  Check with `objdump -T out/arm/devilsdetour | grep -o "GLIBC_[0-9.]*" | sort -u` (should be only GLIBC_2.17).
- Test build (native): `gcc -O2 -DTRUST_TEST -I inc -I/usr/include/SDL2 -Isrc src/trust.c src/font_data.c -o out/trust_test -lSDL2 -lm`
  Run with `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`.
- Sanitizers: same line with `-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer`, then `--fuzz`.

## Test tooling (only in the -DTRUST_TEST build)
- `--dump`: prints every level (map, signs, claims). Header has `jk=` (hidden-ceiling zones), `air=` (ghost pits), `on=` (11 mutator flags).
- `--solve A B`: best-first solver over `world_step` for platform levels A..B. Prints failures. `SOLVE_T=1` prints win time and x.
  The solver state key includes the clock phase (bucketed 0.2 s), so time rules are proven finishable too.
- `--stats`: per-claim reality counts, lies/truths per level (>=6 lies, >=1 truth), zone/ghost counts. "stats problems: 0" is the pass line.
- `--fuzz`: random-input runs over all levels (use with ASAN).
- `--shot mode level frames out.bmp`: screenshots (modes: brief, play, card, screen, title, select, end, help, crys, win, art).
Last full run: 495 platform levels solved, 0 failures; stats problems 0; ASAN fuzz clean.

## Level generation (levels.h, `build_try`)
Deterministic from the level index (Park-Miller `Rng`). `build_platform` retries up to 40 attempts until the level's primary behaviour is really present.
Order inside `build_try`:
1. `pick_mut`: 1-4 mutators (`M_POP, M_ARROW, M_INVERT, M_COINS, M_CRUMBLE, M_GRAVITY, M_FLEE, M_DECOY, M_INVIS, M_SPEED, M_FAKES`). Levels 1-9 force one each. Conflicts: FLEE vs ARROW/DECOY only.
2. `gen_tops` + `validate`: ground heights per column (`top[c]`, `PIT` = -1).
3. Pits: extra pits carved (`carve_pit`, width 2-3, sometimes 5 = unjumpable). Each pit gets a rule: open pit (jump it, walking in kills), ghost pit (invisible floor, walk it), ghost pit + hidden ceiling (walk it, jumping kills). Not in gravity levels (except ghost on/off follows M_INVIS).
4. Hidden-ceiling zones: over some ghost pits and over some flat ground (>=7 wide). Max 4. Spots reserved so nothing needing a jump sits next to them.
5. Pops, fakes, real spikes, deadly/harmless coins, crumble, flee, then doors (see below), ghost tiles (`n`), then the clock, props, mirror (ARROW), gravity ceiling.
6. Final map -> door table (styles shuffled, portal destinations) -> facts -> claims.

## The clock (everything flips with time)
Each level has period `P` (5-9 s) that repeats from the first frame; `phase = fmod(t, P)`. All of it lives in `world_step` (world.h), so the real game and the solver share it:
- Controls swap sides at `inv[]` moments (2-6 flips, >=1.3 s apart).
- Speed 1.4x inside `[spdA, spdB)`.
- Hidden ceilings `jz[]`: deadly to anyone airborne inside the zone while `phase` is in `[a, b)`.
- Ghost floors `gp[]` blink solid inside `[a, b)` (about half of them); tiles reactivate only when the player does not overlap them.
- Pops: hidden spikes are real hazards that are active only inside their window.
- Gravity points up inside `gw[]` windows (1-2). Gravity levels have a ceiling (rows 0-1) and 0-2 hanging spikes.
- FLEE exit moves between 3-4 spots on a schedule (`flee[].t`), the last one is col 18.
- Truth guarantee: the first change of each rule happens early enough that the claim is really true, even for the fastest run.
- `world_hooks` is now an empty stub (kept for the solver call).

## Doors and gates
Up to 4 objects per level (FLEE levels keep one door). Map chars: `G` exit door, `D` deadly door, `O` teleport gate, `Z` deadly gate. Looks: every door uses style 0 (a 16x24 checkered finish flag on a pole, drawn from an ASCII map in `gen_tiles` and rendered 32x48; players see them as "flags", the code still calls them doors), every gate style 3 (portal ring), so the look only tells door from gate, never win from kill. `Level.door[].type`: 0 exit, 1 deadly door, 2 teleport gate (destination = random safe standing cell elsewhere, `K_TELE` in world.h), 3 deadly gate, 4 lucky gate. A deadly object is a gate 40% of the time. Extra objects sit on ground columns 4-15, never an exit within 8 columns of the start, always >=1 exit. `on[M_DECOY]` is true when any non-exit object exists. Claim C_TP ("a gate teleports") is true when a type-2 gate exists.

## Claims (the "lies")
Each platform level has 8-13 claims, >=6 lies and >=1 truth. Every claim has `lie`, `id` (icon), `ass` (asserts it happens), numbers for the counter claims. The same statement is true on one level and false on another; there is no fixed reality. Claim ids: the 11 mutators, facts (spikes, pits, high exit, torch, crystal, coins, death counter, level number, spike count), `C_JK` (jumping kills), `C_TP` (a door teleports). Picture grammar: icon + green tick (it happens) or red slash (it does not). Texts are in `CTXT[claim][asserts][variant]`.
There is no briefing or card screen any more (removed 2026-11-14: too many screens). `start_level()` goes straight to ST_PLAY, a win calls `level_won()` (saves the level's claims in `lastClaims`, goes to the next level, short white flash `flashT`). The R1 guide shows the claims (`LEVEL n SAYS`, pictures) and the previous level's tick/cross + real state (`LAST LEVEL`). `draw_brief`/`draw_card` and ST_BRIEF/ST_CARD still exist for `--shot brief|card` but are unreachable in play. The first 3 levels show a 6 s hint `R1: ...` under the flag.

## HUD
Flag + level number (top centre), coins (top right), progress bar (bottom left), skull + death counter (bottom right, may lie). The old bulb + hint claim at the bottom centre was removed (it showed one random, possibly false claim, so it told the player nothing). `Level.tip` is still generated but unused.

## Coins
Harmless coins (`c`) are collectable. `World.newCoins` is read by `play_events()` in trust.c, which adds to `coinsHeld`. The count shows top right (coin pile icon) and is kept across levels until you die (any death sets it to 0). It is saved as the third line of `save.txt`; New game resets it. Deadly coins (`*`) are unchanged. Replaying a level lets you collect its coins again.

## Icon guide (R1)
`HelpE HELP[]` table in trust.c (before `draw_all`): sections: LEVEL n SAYS (claim pictures), LAST LEVEL (what lied), then how to read (first 4 entries), section 1 = icons whose id is in `level.claims` (tagged HERE), section 2 = everything else. `help_pass(draw)` lays out and measures rows (word-wrapped), `draw_help()` overlays, `help_update()` handles scroll/close. `helpOpen` freezes `game_update` (opens in PLAY). Input: `Keys.l1/r1` (SDL controller shoulder buttons, joystick buttons 4/5, keyboard R/Tab/L; gptk maps r1=r, l1=l). Hints must not reveal what is true in a level. Test: `--shot help level scrollpx out.bmp`.

## Torches (crystals were removed)
Props `T` / `q` (placed in `build_try`, `PLACE_PROP`) now do something. Per-prop randomness is `wh(level idx, col, row)` in world.h, so it is deterministic.
- Torch (`Ent.fx` 1 = honest, `Ent.ph` flicker offset): lit while `fmod(w->t + ph, 4) < 2.6` (`torch_lit`). `torch_reveal()` in trust.c draws, within 170 px of a lit torch, outlines for ghost tiles, red spikes for pops, and a dashed red line for armed hidden ceilings. A lying torch shifts tile/spike outlines by one tile and shows the ceiling line when it is NOT armed.
- Crystals were removed from the game at the user's request (they added little). Levels no longer place `q`, and `skip[F_CRYSTAL]` keeps the crystal claim out. Dead code left in place: `LK_CRYSTAL`, `World.slowT/crEv/cry`, the toast and slow bar, the `crys` shot mode, `F_CRYSTAL` tables, `IC_CRYSTAL`.

## Box art (muOS catalogue)
Colourful key art (v3): `--shot art 1 0|1 out.bmp` in the test build draws the scene with the game's own sprites (`art_scene()` in trust.c), `source/make_art.py` adds the title and downsizes to box 240x320 / preview 320x240. Targets: `MUOS/info/catalogue/External - Ports/{box,preview}/Devils Detour.png`. The device bridge may re-encode PNGs, so compare the picture, not the byte size.

## Colours
`Pal` in levels.h per world: ground body `g1`, grass/top `g2`, sky stops `sk[3]` (top, middle, horizon), striped sun `sun[2]`, mountains `mt[2]` (far, near). `gen_world_art()` in art.h builds the dithered sunset sky, the striped sun, and the two mountain layers with a pink rim from these. World 1 matches the box art. Crumbling tiles are the body colour mixed 28% with pale yellow so they stay recognisable on orange ground.

## Screen
The game draws on a 640x480 canvas and stretches it to the whole screen (x and y scaled separately, `fit_screen()` in trust.c), so the RG34XX SP's 720x480 has no black bars. Set `TRUST_KEEP_ASPECT=1` to get letterboxing back. Test build: `TRUST_W=720` fakes a wider window for `--shot`, `TRUST_SHAKE=0.3` forces screen shake.

## Physics numbers
GRAV 1600, JUMP 560, RUN 190, TS 32, player 20x28, coyote 0.09 s, jump buffer 0.12 s. `world_update` substeps at 1/120 s.

## Decisions the user made (in order)
1. Rebuild in C/SDL2 (no Love2D runtime).
2. 550 levels, more than 5 lies per level, and a lie in one level may be true in another.
3. AAA-style pixel art, everything as images (exit sign with a direction arrow, icons instead of words, even briefing and card).
4. No fixed truths: any claim can change level to level.
5. Jump tricks: jumping kills in some places, not jumping kills in others, some pits must be walked across.
6. More of those tricks, each pit with its own rule, and rules that change inside the same level.
7. Everything timed randomly and flipping as time passes, from the first level.
8. Several doors in different styles: one wins, some kill, some teleport.
9. POP, FLEE and gravity also on the clock.

## Known caveats
- Never tested on real hardware (only headless runs, screenshots and the solver).
- Blinking invisible floors and time rules are hard by design; the solver only proves a path exists. If it feels unfair, lower the blink chance (`rf(&TX) < 0.5` in the clock block) or lengthen windows.
- Solver samples every 4 frames; real play uses 1/120 s substeps, so a path can differ slightly in timing.
- The A-button prompt icon looks dim; some 8x8 icons are rough; 13px card text may be small on the device.
- Torch/crystal effects were only checked with screenshots, not on hardware. Torch radius (170) and crystal durations (6 s reveal, 4 s slow) are easy to tune.
- Unused leftovers: `World.pops`, `Pop.done`, `fleeStage`, `teleShown`, `jumped`.
- Claim ids 18/19 (counter, number) are special; stats show reality only for claims that appeared.
- No visual check of the jump-kill death animation (spikes drawn at row `jz.row - 3` after a ceiling death) or of a portal teleport.

## Ideas to continue
- A feedback cue (icon or flash) when a rule flips, optional hint of the clock.
- Visual shimmer for ghost floors when they are solid (off by default; it would reveal the truth).
- Portal arrival effect, door-specific sounds.
- Tune frequency: ceiling zones 125, ghost pits 237, doors per level 1 (89) / 2 (225) / 3 (91) / 4 (8), FLEE single-door 82.
- Make claims about the clock itself ("the floor will vanish").

## Shipping steps used so far
1. Build ARM, verify GLIBC_2.17.
2. Copy `devilsdetour` binary and `source/src/*` into `pkg/`, rezip `devilsdetour.zip`.
3. Copy to `Desktop/troll`; the user copies `Devils Detour.sh` to `ROMS/Ports` and `devilsdetour/` to `ports/` on the SD card.

## Overlap audit
`--overlap [v]` (test build) checks all platform levels for objects sharing space (coins, spikes, torches, flags, gates, hidden spikes, running-exit spots, signs, platforms). It must print `0 problems`. `--signstat` shows that the screen-centre block (cols 8-11, rows 6-8) is free for the exit sign. Run both after changing level generation.

## Lucky gates (wall levels)
Map char `L`, entity kind `K_LUCK`, door type 4, drawn as a gate (style 3). `Level.lucky`, `lgp` (period = P/2), `lga`/`lgb` (good window): while `fmod(t, lgp)` is inside [lga, lgb) the gate throws the player to (dc, dr) (the ground cell 4 columns beyond the gate, past the wall), otherwise it kills. The window is 45-60% of the period, so a random try succeeds about half the time. Generation (levels.h, `build_try`): `opt.wallRun` (about 34% of levels from 14 up, never with gravity flip, running exit or crumbling floors) forces a flat start run (columns 0..9), keeps objects, pits, hidden ceilings and teleport gates ('O' becomes 'Z') off it, then puts a 5-tile wall at column 6 and the gate at column 4. The wall cannot be jumped (jump height is about 3 tiles), so these levels are only solvable through the gate. Check: building with the gate never working (see PROJECT_HISTORY item 30) makes exactly the wall levels unsolvable (81 of 495). The solver keys time by `fmod(t, P)` in 0.2 s steps, and `lgp` divides P, so the key is enough.

# Devil's Detour: full project history and restart guide

Written 2026-11-14 (Riyadh). Purpose: when we continue next time, read this file first (then `DEVELOPMENT.md` for the technical details, then `source/src/`).
ملخص بالعربي: ده ملف فيه كل اللي عملناه في لعبة Devil's Detour بالترتيب، والحالة الحالية، وإزاي تبني وتختبر وتسلّم، وإيه اللي لسه ماجرّبناهوش على الجهاز، والأفكار الجاية.

## 0. How to resume next time (copy-paste start)
1. Give Claude this folder (`Desktop/troll`) or just this file plus `source/`.
2. Say what to change. Everything is built from `source/src/*` with the commands in section 6.
3. Always run the checks in section 6 before delivering (solver: 495 solved, 0 failures).
4. Deliver to `Desktop/troll` and (for art) to the SD card catalogue; verify afterwards (section 7).

## 1. The game in one paragraph
"Devil's Detour" is a trick platformer where every level lies to you. 550 levels = 11 worlds x 50 levels. Every 10th level is a trick UI screen (55 of them, e.g. "hold a button to charge"), the other 495 are platform levels. Each platform level makes 8-13 claims, shown as pictures when you press R1 (an icon plus a green tick = "this happens", or a red slash = "this does not happen"). At least 6 are lies and at least 1 is true, and the same claim can be true in one level and false in the next. The R1 guide also shows which claims of the previous level lied. Almost everything also flips on a repeating clock, from level 1.
Runs through PortMaster on Anbernic RG34XX SP (720x480) and RG35XX Pro (640x480) under muOS. Native C + SDL2 (his PortMaster has no Love2D runtime, which is why the first Lua version was abandoned).

## 2. Who and how
- Mohamed, Riyadh. Writes Egyptian Arabic and English. Likes things done and delivered, not planned in chat. Typical flow: he asks for a feature in Arabic, Claude implements, verifies with screenshots + the solver, delivers to `Desktop/troll`, he copies to the SD card.
- Deliver location: `/Users/mohamedhamdi/Desktop/troll/`. He copies `Devils Detour.sh` to `ROMS/Ports` and the `devilsdetour/` folder to `ports/` on the SD card.

## 3. Timeline: everything we did, in order
Early sessions (reconstructed from my notes; details are less exact than later ones):
1. **Love2D first**: first version in Lua (deleted at the user's request; it no longer exists anywhere). The device has no Love2D runtime, so we switched.
2. **Rebuild in C/SDL2**: one binary, procedural pixel art (no image files), bitmap font baked into `font_data.c`, cross-compiled for aarch64 with zig (needs only GLIBC_2.17, links the device's libSDL2).
3. **550 levels**: 11 worlds (Trust Falls, Fine Print, Small Lies, Devil's Detours, Half Truths, Hearsay, Gaslight, Bait & Switch, Alibi, Perjury, The Whole Truth) x 50, with trick screens every 10th level.
4. **More than 5 lies per level, and no fixed truths**: a claim can be a lie in one level and true in another. Claims are shown as pictures (icons + tick/slash), not text.
5. **AAA-style pixel art**: exit sign with direction arrow, icons instead of words everywhere (briefing, card), parallax backgrounds, glow, particles, player animation.
6. **Jump tricks**: jumping can kill (hidden ceiling), not jumping can kill, some pits must be walked across (invisible floor).

This conversation (all done, tested with the solver):
7. **More jump tricks, per-pit rules**: each pit has its own rule (open pit: jump it, walking in kills; ghost pit: invisible floor, walk it; ghost pit + hidden ceiling: walk it, jumping kills). Rules vary inside the same level.
8. **Controls flip several times per level** (2-6 flips, at least 1.3 s apart).
9. **Everything timed randomly and flipping as time passes, from level 1**: a per-level clock of P = 5-9 s that repeats. Controls, speed (1.4x window), hidden ceilings, blinking ghost floors, POP spikes, gravity windows and the fleeing exit all follow it. The solver includes the clock phase so every level is provably finishable.
10. **Several exit doors in 5 looks** (wood, gold, steel, portal ring, crimson): the look says nothing; one type wins, some kill, some teleport you elsewhere in the level. Up to 4 doors per level.
11. **POP, FLEE and gravity moved onto the clock** (they used to depend on your position).
12. **`DEVELOPMENT.md`** saved in the project folder (technical handoff).
13. **Stretch to the whole screen**: 640x480 canvas scaled in x and y separately, so the RG34XX SP's 720x480 has no black bars (`TRUST_KEEP_ASPECT=1` brings letterboxing back).
14. **Box art + preview for muOS** (first version). Problem found: the device bridge staging cache pushed a stale file once (see section 7).
15. **Collectable coins**: harmless coins are picked up, a counter (top right) stays across levels until you die (death resets it), saved as the 3rd line of `save.txt`. Deadly coins unchanged.
16. **HUD clean-up**: removed the useless bulb + tip at the bottom centre. HUD now: flag + level number (top centre), coins (top right), progress bar (bottom left), skull + death counter (bottom right, may lie).
17. **Icon guide on R1** (works in play and pause): scrollable list of every icon with meaning and hint; three sections (how to read, icons in this level tagged HERE, everything else). Up/down scroll, left/right page, R1/A/START/SELECT close; game frozen while open. Keyboard R or Tab, controller shoulder buttons, gptk maps `r1 = r`, `l1 = l`.
18. **Torches and crystals now do something** (brainstormed together, then implemented):
    - Torch: lights and goes out on a timer; while lit it shows hidden things within 170 px (invisible floors, hidden spikes, armed ceilings). About half of the torches lie (shift the outlines by one tile, or show the ceiling at the wrong time).
    - Crystal: touch = one random effect: real exit glows 6 s (only on levels with more than one door), or clock at half speed for 4 s (the only effect that changes play; the solver models it), or one briefing claim shown as TRUE or LIE (the crystal is truthful) for 5 s.
    - Decided NOT to do: the coin-priced shield idea. Coins still have no use except the counter.
19. **New box art** (colourful, expresses "every level lies": sign points the wrong way, a tick card saying spikes are safe, three doors, hero jumping a spike pit, sunset). Generated with the game's own sprites (`--shot art`) plus a PIL title (`source/make_art.py`). Installed on the SD card: `MUOS/info/catalogue/External - Ports/box/Devils Detour.png` (240x320) and `.../preview/Devils Detour.png` (320x240), verified pixel-identical to the final images.
20. **Same colours inside the game**: each of the 11 worlds has a vivid sunset palette (dithered 3-stop sky, striped sun, two mountain layers with a rim, ground body + grass colours). World 1 matches the box art. Crumbling tiles are lighter so they stay recognisable on orange ground.
21. **Fewer screens** (user: too many empty screens): removed the briefing before each level and the result card after it. A level now starts immediately and a win goes straight to the next level (short white flash). R1 shows what the level says (claim pictures), what lied in the last level (cross/tick + real state), and the icon guide. A 6 s hint about R1 appears on levels 1-3.

22. **Doors and gates split** (user): all doors now look identical (wooden; first made gold, changed to wood on request) and are only win-or-kill; a new object, the gate (glowing ring, one look), teleports you elsewhere or kills you. New map char `Z` = deadly gate. Help texts and box art (3 identical-look objects: two wooden doors and one gate) updated. Solver still 495/0.
23. **Door looks like a door**: the first wooden sprite looked like a crate, so it was redrawn as an arched stone frame with four wooden panels, iron hinges and a gold knob.

24. **Door redesign from pixel-art research** (user asked to learn from online pixel-art door ideas). Lessons used: one light source (top-left), a darker inner shadow under the lintel for depth, recessed panels (dark top edge, light bottom edge), a lighter frame so the silhouette pops against the dark sky, a small bright gold knob that stands out from the wood, few colours, and a tall rectangular shape (the earlier arch read as a tombstone and the square tile read as a cabinet). The door is now a 16x24 sprite drawn 32x48 (taller than a tile, hitbox unchanged); gates stay 16x16. Box art re-rendered (art/box_v4.png, art/preview_v4.png); sign in the scene moved up.

25. **The door became a flag** (user: "instead of the door, make it a flag for arrival"). Exit and deadly objects (map chars `G`/`D`) are now a checkered finish flag on a pole with a gold ball and a stone base (same 16x24 sprite slot, `DW[24]` in `gen_tiles`, drawn 32x48). All flags look identical, so only some win, as before; gates stay the glowing ring. Player-facing text says "flag" instead of "door" (claims, icon guide, "Fake flag"). Internal names (`LK_DOOR`, `door[]`, `HI_DOORS`) are unchanged. Box art re-rendered: art/box_v5.png, art/preview_v5.png.

26. **Crystals removed** (user thought they were not useful; options were keep / always-truthful / remove, and they chose remove after first choosing always-truthful). No `q` props are generated, the crystal claim is skipped, the icon-guide entry and the box-art crystal are gone. RNG stream kept the same where possible. Dead code remains, listed in DEVELOPMENT.md. Solver: 495/495, fuzz ok. Box art re-rendered: art/box_v6.png, art/preview_v6.png.

27. **Renamed** (user): the game was called "Trust Issues". After a naming round (Trust Fall, Fine Print, Bad Signs, White Lie, ...) the user's last choice was **Devil's Detour**. Renamed: title screen, window title, box art and preview text, launcher `Devils Detour.sh` (no apostrophe in file names, because it breaks shell launchers), folder and binary `devilsdetour`, `devilsdetour.gptk`, `devilsdetour.zip`, port.json, gameinfo.xml, docs. World 4 was called "White Lies" and is now "Tall Tales" (an earlier candidate name was White Lie). The save file is now `ports/devilsdetour/save.txt`, so old progress under `ports/trustissues/` is not picked up. Source files are still `trust.c` etc. SD card art file name is now `Devils Detour.png`. Idea not done yet: devil theme (horns on the hero, red/purple title colours).

28. **Old files deleted** (user: keep only the new version). Removed from `Desktop/troll`: the old `trustissues/` package, `Trust Issues.sh`, `trustissues.zip`, the Love2D version `lua-love-version/`, and older box-art drafts (only `art/box_v8.png` and `art/preview_v8.png` remain). Removed from the SD card: `Trust Issues.png` (box and preview), `Trust Issues.txt` and the muOS history entry. The SD card now has only `Devils Detour` (launcher in `ports/` and `ROMS/Ports/`, folder `ports/devilsdetour/`, art and text in the catalogue).

29. **Sign in the centre, no overlaps** (user, after playing on the device: "move the green direction sign to the centre of the screen" and "no overlapping between game objects"). The exit sign and its chevrons now sit at the exact centre of the 640x480 playfield (sign y=208, chevrons y=252, x=320; centred on the 720-wide screen too). The centre block is free of platforms and objects in every level (checked with `--signstat`). New test `--overlap [v]` checks every platform level: no two objects share a cell, none sits inside a platform, flags (drawn 48 px tall) have a free cell above, hidden spikes and the running exit's hop spots are free, signs touch nothing. It found 146 problems in 100 levels (signs on terrain, torches on hidden spikes, running-exit spots on coins). Fixes in levels.h: the sign is at the centre; props are placed with the same free-space marks as spikes; the running exit's spots are cleared and reserved. Now 0 problems; solver 495/495.

30. **Lucky gates** (user: some levels should be impassable except through a gate that throws you to a usable spot, and sometimes kills you if your timing is wrong, so you retry until you are lucky). New gate type 4 (`L`, `K_LUCK`): the outcome depends on the clock phase `fmod(t, P/2)`. 81 of the 495 platform levels (16%, spread over all 11 worlds, from level 18 on) are wall levels: a 5-tile wall that no jump clears, a lucky gate in front, the rest of the level beyond. Verified: solver 495/495 with the gate working, and with the gate forced to never work exactly those 81 levels fail (so the gate really is the only way). Overlap audit still 0, fuzz ok. Icon guide text for gates updated. Not tested on hardware: the feel of the timing (window is 45-60% of a 2.5-4.5 s period, so about 2 tries on average). Tuning: `0.34` (share of wall levels) and `0.45 + 0.15 * rf(&LX)` (window width) in levels.h.

## 4. What is in the game now (feature inventory)
- 550 levels, deterministic from the level index; trick screens every 10th.
- Hazards: spikes, painted (harmless) spikes, POP spikes (clock), pits (open / invisible floor / invisible + hidden ceiling), hidden ceilings (jump kills), crumbling floors, deadly coins, gravity flip with a ceiling and hanging spikes, speed window, flipped controls, fleeing exit, doors (win/kill, all the same look) and gates (teleport/kill).
- Pickups: harmless coins (counter), crystals (random effect), torches (reveal).
- Systems: save (`save.txt`: unlocked level, deaths, coins), level select, pause (START), quit (hold START+SELECT), skip after 8 deaths on a level (SELECT), screen shake, particles, sound effects (generated tones), icon guide (R1).
- Palettes: 11 worlds, each with its own sunset.

## 5. Code map (`source/src`, about 3200 lines)
- `levels.h`: palettes (`PALS`), RNG, level generation (`build_try`), clock block, doors, claims + claim texts (`CTXT`).
- `world.h`: entities, physics, `world_init`, `world_step` (clock, hazards, coins, crystals, doors, teleports), `wh()` hash, `torch_lit()`.
- `art.h`: procedural pixel art (sprites, icons, doors, per-world backgrounds and tiles).
- `trust.c`: game loop, drawing (`world_draw`, `torch_reveal`, `draw_hud`, briefing, card, menus, icon guide), input, audio, save, and the test tooling (`#ifdef TRUST_TEST`: dump, solver, stats, fuzz, screenshots, art scene).
- `font_data.c/.h`: baked bitmap fonts (`gen_font.py` makes them).
- Also in the folder: `build_arm.sh`, `inc/`, `make_art.py`, `port.json`, `gameinfo.xml`, `README.md` (player-facing), `devilsdetour.zip` (PortMaster package), `Devils Detour.sh` (launcher).

## 6. Build, test, ship (exact commands)
Working copy used in this session: `/home/claude/c` (source) and `/home/claude/pkg` (package). They are not on your computer; `Desktop/troll/source` is the copy to start from (recreate `src/`, `inc/`, `build_arm.sh` from it).
- ARM build: `cd source && ./build_arm.sh` (needs `pip install ziglang`, `apt install libsdl2-dev`). Check `objdump -T out/arm/devilsdetour | grep -o "GLIBC_[0-9.]*" | sort -u` shows only `GLIBC_2.17`.
- Test build: `gcc -O2 -DTRUST_TEST -I inc -I/usr/include/SDL2 -Isrc src/trust.c src/font_data.c -o out/trust_test -lSDL2 -lm`, run with `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy`.
- Pass line before every delivery:
  - `./out/trust_test --solve 1 550` -> `solved 495 platform levels, 0 failures` (last run: max expansions 5320)
  - `./out/trust_test --stats` -> `stats problems: 0`
  - ASAN build (`-O1 -g -fsanitize=address,undefined`) then `--fuzz` -> `fuzz ok`
- Screenshots: `--shot mode level frames out.bmp`, modes `brief play card screen title select end help crys art`. Env: `TRUST_W=720` (wide window), `TRUST_SHAKE`, `TRUST_NOHUD`, `TRUST_FX=0|1|2` (force crystal effect in `crys` mode). `--shot art 1 0` box scene (480x640), `--shot art 1 1` preview scene (640x480).
- Level dump: `--dump`; `SOLVE_T=1` with `--solve` prints win times.
- Ship: copy binary to `pkg/devilsdetour/devilsdetour`, copy sources to `pkg/source/src`, rezip (`zip -qr devilsdetour.zip "Devils Detour.sh" devilsdetour port.json gameinfo.xml README.md DEVELOPMENT.md source`), then push to `Desktop/troll`.
- Latest shipped binary: 413528 bytes, zip 252069 bytes.

## 7. Delivery gotchas (important)
- `device_commit_files` once pushed a STALE file when a staged path was reused. Rule: stage under a NEW path each time, then verify on the device (sizes, and for images compare the picture).
- The bridge re-encodes PNGs, so the SD-card box/preview PNGs are bigger than my originals (83729 / 80194 bytes) but pixel-identical.
- SD card path: `/Volumes/MustardOS/MUOS/info/catalogue/External - Ports/{box,preview,text}/Devil's Detour.{png,txt}`. If the handheld shows the old picture, reboot muOS or refresh the catalogue.
- The `text` description is `art/desc.txt` (unchanged since the first version).

## 8. Tuning knobs (where to change feel)
- Physics (`world.h`): GRAV 1600, JUMP 560, RUN 190, coyote 0.09 s, jump buffer 0.12 s.
- Clock (`levels.h`, clock block `Rng TX`): period `P = 5 + 4*rf`; flip count 2-6; ghost blink chance `rf(&TX) < 0.5`; gravity windows 1-2; speed window 1.4x.
- Torch (`world.h`/`trust.c`): lit while `fmod(t + ph, 4) < 2.6`, radius 170 px (`torch_reveal`), half lie (`h & 1`).
- Crystal: reveal 6 s (`revealT`), slow 4 s at 0.5x (`world.h`, `slowT`), claim toast 5 s (`toastT`).
- Door count: `nextra` in the doors block of `build_try`.
- Colours: `PALS` in `levels.h`; sky/sun/mountain generation in `gen_world_art()` in `art.h`.
- Help texts: `HELP[]` in `trust.c`. Claim texts: `CTXT` in `levels.h`.

## 9. What is verified and what is not
Verified (headless): all 495 platform levels have a solver-proven path including the clock and the crystal's slow effect; stats show at least 6 lies and at least 1 truth per level; ASAN/UBSAN fuzz clean; screenshots checked for the HUD, icon guide, torch light, crystal effects, door styles, gravity ceiling, 720x480 stretch, box art and all 11 world palettes; SD card art matches the final images.
NOT verified:
- Anything on the real handheld (feel, speed, controller mapping for R1, audio, text size at 13 px, the stretched look).
- The solver samples every 4 frames, real play runs at 1/120 s substeps, so a "provable" path can be tight in real time.
- Blinking invisible floors and clock rules are hard by design; if it feels unfair, lengthen windows or lower the blink chance.
- Deaths animation for the hidden ceiling and the portal arrival were never reviewed visually.
- Contrast between platforms and background on some worlds (crimson and orange worlds have platforms close in colour to the hills; the grass tops are the clue). Darken the near mountains (`mt[1]` factor 0.85 in `gen_world_art`) if needed.

## 10. Known leftovers
- Unused fields: `World.pops`, `Pop.done`, `fleeStage`, `teleShown`, `jumped`, `Level.tip`.
- Coins can be re-collected by replaying a level.
- Shield idea dropped, so coins only count; a crystal costing coins is still an option.
- Title screen text is still gold (not the orange gradient of the box art).
- Some 8x8 icons are rough; the A-button prompt looks dim.

## 11. Ideas for next time
- Test on the real RG34XX SP / RG35XX Pro first and note anything unfair, too hard, or hard to read. Feed that list back and tune the knobs above.
- Spend coins (crystal price, extra hint, skip a level).
- A cue when a rule flips (icon or flash) and an optional clock hint.
- Portal arrival effect, door-specific sounds.
- Title screen in the new colours, per-world title card.
- Claims about the clock itself ("the floor will vanish").
- Achievements or a stats screen (deaths, coins, time).
- Frequency tuning table: ceiling zones 125 levels, ghost pits 237, doors per level 1 (89) / 2 (225) / 3 (91) / 4 (8), FLEE single-door 82.

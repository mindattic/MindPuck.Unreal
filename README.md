# MindPuck.Unreal

MindPuck for Unreal Engine 5.8: move a puck into the goal with your eyes.

A puck sits on an air-hockey table and your gaze moves it. There are three control schemes. F1
(or Tab) steps through them, and a label at the bottom centre of the screen always names the one
that is live ("Mode 2 of 3  STEER  F1 next"):

1. **Magnetic** (the default): the puck is pulled to where you look. To win, look at the pad.
2. **Steer**: keep your eyes on the puck and shift them toward the edge on the side it should go,
   and it travels that way. Look at its centre to let it be. Your eyes stay on the puck the whole
   time.
3. **Repulsor**: the point you look at pushes the puck away, like a black hole in reverse. Look
   behind the puck to push it, and look far away to let it glide.

Bring the puck to rest on the green pad. Every level is generated from a seed, and a solver bot
wins each one in all three modes, with an unsteady gaze, before you see it, so every level is known
to be possible each way. The bot's time in the mode you are playing is the par.

The gaze comes from a webcam through [MindAttic.Webcam](../MindAttic.Webcam). With no
tracker connected the mouse stands in, so the game is playable without a camera.

This is the Unreal version of the web game in [MindPuck](../MindPuck). The rules are the same code,
ported.

## What you need

- Unreal Engine 5.8 (the scripts expect `D:\Games\UE_5.8`, or set `UE_ROOT`).
- This repo and `MindAttic.Webcam` cloned side by side. The project loads the gaze plugin from
  `..\MindAttic.Webcam\unreal` (see `AdditionalPluginDirectories` in `MindPuck.uproject`).
- For eye tracking: Node, a webcam, and a browser.

## Run it

1. Start the bridge and the tracker (skip this to play with the mouse):

   ```
   cd D:\Projects\MindAttic\MindAttic.Webcam
   npm run build
   npm run bridge
   ```

   Then open `http://localhost:8787/tracker/` in a browser and allow the camera.

2. Start the game, either way:

   - `.\run.ps1` compiles the project and plays it in a 1600 by 900 window.
   - Or open `MindPuck.uproject` in the Unreal Editor and press Play. The editor opens the
     engine's empty map; the game builds the table when play starts.

3. Press **C** to calibrate: look at each of the nine dots until it fills, follow the moving dot
   with your eyes, then do the three head-movement steps. The game window should
   stay the same size afterwards, because the gaze is mapped to the area the dots covered.

`run.ps1` options: `-BuildOnly`, `-Editor`, `-SkipBuild`, and `-Extra` for game arguments.

| Game argument | Meaning |
| --- | --- |
| `-Seed=12345` | Play this run. Without it the seed is random; it is shown top right. |
| `-Level=6` | Start at this level (the first is 1). |
| `-Bot` | Start with the solver bot playing. |
| `-Mode=attract`, `-Mode=steer` or `-Mode=repel` | Play this run in that mode, whatever was saved. |
| `-GazeUrl=ws://host:8787/gaze` | The bridge to connect to. |
| `-GazeRoom=name` | The bridge room. |
| `-Calibrate` | Start a calibration as soon as a tracker is present. |
| `-Shot=C:/shots/a.png -ShotAfter=4,20` | Save the viewport after 4 and after 20 seconds (the second file is `a_2.png`), then quit. |
| `-QuitAfter=30` | Quit after 30 seconds. |
| `-DumpLevels=12` | Log the first twelve levels of the run, to compare with `node tools/levels.mjs` on the web. |

## Controls

| Key | Action |
| --- | --- |
| F1 or Tab | Step to the next mode: magnetic, steer, repulsor. The attempt restarts. |
| R | Restart the level |
| N | Skip to the next level |
| B | Bot demo: the solver plays, and its route and gaze are drawn |
| C | Calibrate the eye tracker (C again cancels) |
| M | Mouse stand-in on or off while a tracker is connected |

The mode is remembered between runs (`Mode` in the `[MindPuck]` section of
`Saved/Config/.../GameUserSettings.ini`). The mode label, the gaze reticle and the ring around it
share the mode's colour: pink for magnetic, green for steer, blue for the repulsor. The reticle is
full size for the repulsor, smaller for the magnet and smallest for steer. The editor keeps F1 for
itself during Play In Editor, so use Tab there.

Calibration (C) has three parts, about a minute in all:

1. Nine dots. Look at each until it fills, head still.
2. Follow the dot. One dot moves slowly round the whole screen for 24 seconds; follow it with your
   eyes and keep your head still. The instruction and the seconds left are at the top centre. This
   samples the whole area instead of nine places and improves accuracy more than anything else.
3. Three head-movement steps with an amber dot: keep your eyes on the dot and move your head as the
   text says. They teach the tracker to allow for head movement.

The flow, the path of the moving dot and the messages to the tracker are the plugin's
(`UGazeCalibrator`: `bPursuit`, `PursuitSeconds`, `bHeadMovement`); the game's HUD only draws them.

The mouse is the gaze whenever no tracker is connected, and the status line says so:
"waiting for tracker - open http://localhost:8787/tracker/".

At the start of every attempt the puck is held still, so you can find it before your gaze starts
pushing it: 0.9 seconds, or 1.7 seconds in steer mode, where the HUD says "Look at the puck". When
the puck rests on the pad the game shows GOAL and loads the next level of the same run.

### Steer anchors itself to the puck

Steer goes by where you look relative to the puck, and an eye tracker is rarely accurate to within
the width of the puck. But in steer mode you are looking at the puck by definition. So the game
measures the standing offset between the gaze point and the puck and subtracts it:

- While the puck is held at the start of an attempt, the offset is followed with a time constant of
  0.3 seconds, so by the end of the 1.7-second hold the gaze point sits on the puck.
- During play it is followed with a time constant of 6 seconds, slowly enough that a deliberate
  look at one edge does its work before it is absorbed, and only while the corrected gaze is within
  4 table units of the puck, so a glance elsewhere does not drag it.

What is left is the movement of your eyes toward one edge. The reticle is drawn at the corrected
point. Half a second before the hold ends, once per attempt, the game also tells the tracker that
you are looking at the puck (`CalibHint` at the puck's place on the screen, weight 0.3), which is a
free calibration point. The log has one line for it ("Steer anchor at the end of the hold").

The mouse stand-in goes through the same correction, as on the web, but no hint is sent for it. The
offset is kept from one attempt to the next and dropped when the gaze changes hands between the
mouse and the tracker. The solver bot's gaze is exact and is not touched. All of this is in the
game layer (`AMindPuckGameMode::AnchorSteer`), not in the core, so the pars are unaffected. The
numbers are properties of the game mode, under MindPuck, Steer.

## How it is built

There are no authored assets: no `Content/` folder, no map, no Blueprint, no UMG. Everything is
made at runtime from C++.

| Path | What it is |
| --- | --- |
| `Source/MindPuck/Core/` | The rules, in plain C++ (namespace `MindPuck`, no engine types). |
| `Source/MindPuck/Game/MindPuckGameMode` | The game loop: level, simulation, bot, gaze source, keys. |
| `Source/MindPuck/Game/MindPuckTable` | The table actor: surface, rails, obstacles, goal pad, puck, reticle, lights. |
| `Source/MindPuck/Game/MindPuckMesh` | Ear-clipping triangulation and extrusion of the level's polygons. |
| `Source/MindPuck/Game/MindPuckPawn` | The camera above the table. |
| `Source/MindPuck/Game/MindPuckHUD` | All text, the gaze ring and the calibration targets, drawn on the canvas. |
| `Source/MindPuck/Tests/` | Automation tests for the core (`MindPuck.Core.*`, `MindPuck.Game.*`). |

The table is 16 by 9 units with the origin at the top-left and y pointing down, as on the web. One
table unit is 100 Unreal units. Table x runs along world +Y and table y along world -X, so the
camera, which looks down with world +X at the top of the screen, sees the table the way the web
version draws it.

The gaze arrives as a normalized point (0..1, y down) that covers the whole viewport. The game
turns it into a ray through that pixel and intersects the ray with the table plane, so the slight
camera tilt costs nothing in accuracy. While the tracker reports zero confidence (a blink, no face)
or the last sample is older than 0.4 seconds, nothing pushes the puck.

Materials are dynamic instances of the engine's `BasicShapeMaterial` with its `Color` parameter
set. The puck, pad and reticle are the engine's cylinder mesh. The obstacles are procedural meshes:
each level polygon is triangulated by ear clipping (the L shape is concave) and pushed up into a
solid.

## How the port mirrors the web core

Each file in `Source/MindPuck/Core` ports the file of the same name in `MindPuck.Web/src/core`:

| Web | Unreal | Contents |
| --- | --- | --- |
| `rng.ts` | `Rng.h/.cpp` | `mulberry32`, `mixSeed`, `range`, `gaussian` |
| `geometry.ts` | `Geometry.h/.cpp` | Points, polygons, distances, the shape builders |
| `level.ts` | `Level.h` | The table's constants and `FLevel` |
| `physics.ts` | `Physics.h/.cpp` | `EGazeMode`, `FPhysicsParams`, `RepelAccel`, `PullAccel`, `SteerAccel`, `FPuckSim` |
| `navgrid.ts` | `NavGrid.h/.cpp` | `FNavGrid`: the free-cell grid and A* |
| `bot.ts` | `Bot.h/.cpp` | `FSolverBot`, `SolveLevel` |
| `levelgen.ts` | `LevelGen.h/.cpp` | `GenerateLevel` and its solver check |
| `index.ts` | `MindPuckCore.h` | Includes all of the above |

What is kept the same:

- **The numbers.** Every constant and default is the original's. Everything is `double`.
- **The random sequence.** `mulberry32` and `mixSeed` use unsigned 32-bit arithmetic, which wraps
  exactly as `Math.imul` and `>>> 0` do, so the same seed gives the same draws.
- **The order of the draws.** The original calls `rng()` inside argument lists, where JavaScript
  evaluates left to right. C++ does not promise that, so the port takes each draw into a local
  first, in the original's order.
- **The algorithms.** The fixed 1/120 s step, the collision passes, the hand-written binary heap in
  A* (so ties break the same way), the bot's decisions and bisection, the 60 attempts and the open
  table fallback.
- **Rounding.** `Math.round` is `floor(x + 0.5)`, as in JavaScript.

The module is compiled with strict IEEE floating point (`FPSemantics = Precise` in
`MindPuck.Build.cs`). The one thing no port can pin down is the last bit of `sin`, `cos`, `atan2`,
`exp` and `hypot`, which differ between JavaScript engines and the C++ runtime. The bot's run is a
long chaotic simulation, so one such bit can change its time. `-DumpLevels` exists to check this.

Measured for seed 12345, levels 1 to 12, when only the repulsor existed: every level had the same
kind, start, goal and obstacle count as the web, and the same repulsor par for all but level 12,
where the bot took 19.5 s here and 15.2 s on the web. The magnetic and steer pars were
compared on 2026-11-19 with the three-mode generator and matched the web for all twelve levels
(steer zone 1.5 to 4 radii at the time; it is now 2.2 to 6, as on the web, where level 1's steer
par is still 4.7 s and level 6's is 6.1 s). The web's figures for the same run are printed by
`node tools/levels.mjs 12345 12` in MindPuck.Web.

Three things the game layer does differently from the web's `main.ts`:

- The steer offset is dropped when the gaze changes hands between the mouse and the tracker. The
  web keeps it.
- The bot demo plays with the gaze-noise seed that proved the level in the current mode, so it
  always wins and takes exactly that mode's par time. The web demo uses a fixed seed.
- `FPuckSim::AdvanceWith` is an addition to the port: `Advance` with a gaze point asked for before
  every step, which is the web's frame loop for the live bot.

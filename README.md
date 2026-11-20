# MindPuck.Unreal

MindPuck for Unreal Engine 5.8: move a puck into the goal by looking where you do **not** want it
to go.

A puck sits on an air-hockey table. The point you look at repels it, like a black hole in reverse.
Look behind the puck to push it, look far away to let it glide, and bring it to rest on the green
pad. Every level is generated from a seed, and a solver bot wins each one with an unsteady gaze
before you see it, so every level is known to be possible. The bot's time is the par.

The gaze comes from a webcam through [MindAttic.EyeTracker](../MindAttic.EyeTracker). With no
tracker connected the mouse stands in, so the game is playable without a camera.

This is the Unreal version of the web game in [MindPuck](../MindPuck). The rules are the same code,
ported.

## What you need

- Unreal Engine 5.8 (the scripts expect `D:\Games\UE_5.8`, or set `UE_ROOT`).
- This repo and `MindAttic.EyeTracker` cloned side by side. The project loads the gaze plugin from
  `..\MindAttic.EyeTracker\unreal` (see `AdditionalPluginDirectories` in `MindPuck.uproject`).
- For eye tracking: Node, a webcam, and a browser.

## Run it

1. Start the bridge and the tracker (skip this to play with the mouse):

   ```
   cd D:\Projects\MindAttic\MindAttic.EyeTracker
   npm run build
   npm run bridge
   ```

   Then open `http://localhost:8787/tracker/` in a browser and allow the camera.

2. Start the game, either way:

   - `.\run.ps1` compiles the project and plays it in a 1600 by 900 window.
   - Or open `MindPuck.uproject` in the Unreal Editor and press Play. The editor opens the
     engine's empty map; the game builds the table when play starts.

3. Press **C** to calibrate: look at each of the nine dots until it fills. The game window should
   stay the same size afterwards, because the gaze is mapped to the area the dots covered.

`run.ps1` options: `-BuildOnly`, `-Editor`, `-SkipBuild`, and `-Extra` for game arguments.

| Game argument | Meaning |
| --- | --- |
| `-Seed=12345` | Play this run. Without it the seed is random; it is shown top right. |
| `-Level=6` | Start at this level (the first is 1). |
| `-Bot` | Start with the solver bot playing. |
| `-GazeUrl=ws://host:8787/gaze` | The bridge to connect to. |
| `-GazeRoom=name` | The bridge room. |
| `-Calibrate` | Start a calibration as soon as a tracker is present. |
| `-Shot=C:/shots/a.png -ShotAfter=4,20` | Save the viewport after 4 and after 20 seconds (the second file is `a_2.png`), then quit. |
| `-QuitAfter=30` | Quit after 30 seconds. |
| `-DumpLevels=12` | Log the first twelve levels of the run, to compare with `node tools/levels.mjs` on the web. |

## Controls

| Key | Action |
| --- | --- |
| R | Restart the level |
| N | Skip to the next level |
| B | Bot demo: the solver plays, and its route and gaze are drawn |
| C | Calibrate the eye tracker (C again cancels) |
| M | Mouse stand-in on or off while a tracker is connected |

The mouse is the gaze whenever no tracker is connected, and the status line says so:
"waiting for tracker - open http://localhost:8787/tracker/".

At the start of every attempt the puck is held still for 0.9 seconds, so you can find it before
your gaze starts pushing it. When the puck rests on the pad the game shows GOAL and loads the next
level of the same run.

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
| `physics.ts` | `Physics.h/.cpp` | `FPhysicsParams`, `RepelAccel`, `FPuckSim` |
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
Measured for seed 12345, levels 1 to 12: every level has the same kind, start, goal and obstacle
count as the web, and the same par for all but level 12, where the bot took 19.5 s here and 15.2 s
on the web. The layouts match; only that par differs.

Two things the game layer does differently from the web's `main.ts`:

- The bot demo plays with the gaze-noise seed that proved the level, so it always wins and takes
  exactly the par time. The web demo uses a fixed seed.
- `FPuckSim::AdvanceWith` is an addition to the port: `Advance` with a gaze point asked for before
  every step, which is the web's frame loop for the live bot.

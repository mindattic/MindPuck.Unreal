# MindAttic project agent entrypoint

Read the shared protocol at ..\mindattic-agent-standard\AGENTS.md and this project's own
documentation (README.md). Common prompt commands are implemented by the shared runner; do not add
a second copy under a provider-specific command folder.

## This repo's own rules

- **Unreal Engine 5.8 C++ project, no authored assets.** Everything on screen is built at runtime
  from `Source/MindPuck`. There is no `Content/` folder; do not add `.uasset` or `.umap` files.
  The startup map is the engine's own `/Engine/Maps/Entry`.
- **`Source/MindPuck/Core` is a port, not a design.** It mirrors `..\MindPuck\src\core` file for
  file. Rule changes are made in the web core first and then ported here, keeping the same
  constants, the same order of random draws and the same algorithms.
- **The gaze plugin lives in another repo.** `MindAtticEyeTracker` is loaded from
  `..\MindAttic.EyeTracker\unreal` (see `AdditionalPluginDirectories` in `MindPuck.uproject`).
  Changes to it are commits in MindAttic.EyeTracker.
- Check work by building (`run.ps1 -BuildOnly`). Tests are run only when the user asks.

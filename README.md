# Level Generator Downloader (Unreal Engine Plugin)

`LevelGeneratorDownloader` is an editor plugin that opens the web-based level generator, imports exported `.glb` files, and runs the plugin import workflow inside Unreal Editor.

## Requirements

- Unreal Engine `5.7` (or a compatible local engine version)
- A C++ project (or any project that can compile editor plugin modules)

## Add to any Unreal Engine project

1. Close Unreal Editor.
2. In the target project, create a `Plugins` folder if it does not exist:
   - `<YourProject>/Plugins`
3. Copy this folder into the project plugins folder:
   - `LevelGeneratorDownloader`
   - Final path should be: `<YourProject>/Plugins/LevelGeneratorDownloader`
4. Open the project (`.uproject`) in Unreal Editor.
5. If prompted, rebuild modules. If not prompted, regenerate project files and build from IDE.
6. In Unreal Editor, enable the plugin if needed:
   - `Edit > Plugins` and search for `Level Generator Downloader`
7. Restart Unreal Editor if requested.

## Open the plugin window

- In Unreal Editor: `Window > Level Generator Downloader`

## Distribution options

- **Per-project install (recommended):** keep plugin under `<Project>/Plugins/`
- **Engine-wide install:** copy to `<UE_Install>/Engine/Plugins/` (affects all projects using that engine install)

## Source control tips

For plugin repositories, keep source/config/content files and ignore generated build outputs such as `Binaries/`, `Intermediate/`, and `Saved/`.

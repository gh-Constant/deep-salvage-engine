# Built-in FSR3 custom editor

Branch `fsr3-builtin` is based on the updated `fsr3-custom-editor` branch,
including upstream Godot master `e810f3f7af`. FSR 3.1.4 is now compiled into
`modules/fsr3/` and is available through ordinary project and viewport settings.
No per-project addon or autoload is required. The module is included in matching
editor and export-template builds on Linux and Windows with 3D support.

Select **Rendering → Scaling 3D → Mode → FSR 3.1.4** with Forward+.
Use scale 0.67 for Quality-style upscaling or 1.0 for Native AA.
See [modules/fsr3/README.md](modules/fsr3/README.md) for backend settings, migration,
builds, tests and limitations. Remove the old FSR3 addon before opening an addon
project with this branch; the old extension registers the same class names.

The generic custom-upscaler hooks are retained. Native source and optional
FidelityFX Vulkan runtime resources reuse them. Source is the default on both
platforms. No external AMD DLL, private FSR4 bridge or frame generation is included.
Native Linux FSR4 and automatic Adrenalin upgrades are not claimed.

The former companion under `misc/fsr3-gdextension` retains historical extension
sources for reference; current native backend development belongs in the module.
Its demo now uses built-in FSR3 and has no addons directory. Updated packaging
ships built-in editors and the standalone demo without extension libraries.
The full automated build/publish pipeline is separate work.

## Sources and credits

- **Bastiaan Olij — custom-upscaler API:**
  https://github.com/godotengine/godot/pull/114670
  Imported implementation commit `6de618aaadcf3f0a34024f94b0148685231ed856`
  (https://github.com/BastiaanOlij/godot/tree/custom_upscaler), adapted to current
  master. The scene registration include conflict was resolved by retaining both
  ViewportUpscaler and World2D includes.
- **HydrogenC — Godot FSR 3.1.4 integration:**
  https://github.com/godotengine/godot/pull/111631
  Reference revision `a2f49a1676608b29a0ce39fe20bc43119b609008`. The companion
  extension ports its FidelityFX host/backend integration and shader wrappers
  from private engine classes to public GDExtension RenderingDevice calls.
  Its engine-integrated FSR implementation is not merged into the engine here.
- **Bastiaan Olij — custom-upscaler example:**
  https://codeberg.org/BastiaanOlij/test-custom-upscaler
  Reference revision `9d89b245dd4be30eda80622efb8389888391f8d7`. Used to understand
  callback/viewport integration and as a reference for the minimal compute path.
- **Godot C++ bindings:** https://github.com/godotengine/godot-cpp
  Pinned submodule `8070599885b6df675fcad033dd032ce44be2b850`.
- **AMD FidelityFX SDK 1.1.4:**
  https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/tag/v1.1.4
  SDK reference `c6efa6bf7f2027b3ec94f28578bb5965eabb9e55`. Source FSR3 host/GPU
  code is the Godot-adapted version from HydrogenC's implementation. Existing
  Godot/AMD/Khronos license notices are retained; see the companion's
  THIRD_PARTY_NOTICES.md and license files.


## Engine integration

The fsr3 module registers native Resource classes and uses native RenderingDevice
structures. Viewport provides an FSR3 mode mapped internally to the generic Custom
renderer mode, owns a backend per viewport and updates jitter before scale changes.
The managed resource is excluded from the serialized custom-resource property.
External Vulkan contexts and loaded libraries remain retained until GPU completion.
ViewportUpscaler property types and jitter forwarding are corrected for native use.

Existing template-source checkouts and published releases are from the extension
branch and are not updated by changing this source branch. Rebuild export templates
from this branch before exporting a project using the built-in classes.

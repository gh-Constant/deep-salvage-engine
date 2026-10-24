# Custom FSR3 editor

This branch combines the experimental custom-upscaler API with a companion FSR
3.1.4 GDExtension and diagnostic demo. The engine exposes general rendering APIs;
the FSR algorithm and shaders remain in `misc/fsr3-gdextension`.
The current upstream base is Godot 4.8 beta; the addon bindings are generated
from this exact patched editor, rather than the older experimental 4.7 build.

The branch is based on upstream Godot master at
`c8256a8ecec068340753869e423dd478c84ef167`, incorporating the 24 upstream commits
that were newer than the original local master when this branch was prepared.
The original custom-upscaler and engine-integrated FSR3 worktrees were preserved.

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

## Additional engine changes

Beyond the imported custom-upscaler proposal:

1. Bind RenderSceneDataRD.get_temporal_upscaler_data(), exposing renderer jitter,
   frame delta, camera near/far/FOV and unjittered reprojection.
2. Guard the Custom-mode jitter lookup when no resource is assigned and retain
   the previous upscaler until its replacement has been queued to the renderer.
3. Expose RenderingDevice.native_compute_callback_add() and the Vulkan command
   buffer handle for compatible external GPU runtimes, track image dependencies,
   and retain captured callbacks/contexts until the frame's fence completes.
   This third change is optional for source FSR3 and supports the binary backend.

The companion adds source/basic/binary upscaler classes, GPU resource/history
management, normalized input metadata, plugin/autoload setup, shader embedding,
an artifact-lab demo and meaningful integration tests. Its original engine shader
logic is preserved; this is not an image-quality or ghosting fix.

## Build and release

Follow misc/fsr3-gdextension/README.md. Git source contains no editor binaries,
generated API dumps, build caches, compiled addon libraries, test captures,
Windows AMD driver DLLs or toolchain downloads. Release assets contain the tested
Linux editor, ready-to-run demo/addon, licenses and checksums separately from Git.

Linux FSR 3.1.4 / Forward+ / Vulkan is the release target. Native Linux FSR4 is
not implemented. The experimental API is not compatible with stock Godot builds.

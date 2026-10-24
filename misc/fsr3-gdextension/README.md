# FSR 3.1.4 GDExtension and Temporal Artifact Lab

This companion addon implements source FSR 3.1.4 outside the Godot renderer.
The custom editor in this repository exposes the experimental ViewportUpscaler
API and the additional temporal metadata needed by the addon. This is not a
standalone addon for stock Godot releases.

Native Linux x86_64, Forward+, Vulkan is the primary tested configuration.
The demo defaults to source FSR 3.1.4 on Linux. No frame generation is included.
Ghosting/image-quality changes are outside this implementation's scope.

## Build from this repository

At the repository root:

```sh
git submodule update --init --recursive
scons platform=linuxbsd target=editor dev_build=no debug_symbols=no -j6
cd misc/fsr3-gdextension
python3 tools/generate_api.py --engine ../../bin/godot.linuxbsd.editor.x86_64
scons platform=linux arch=x86_64 target=template_debug -j6
scons platform=linux arch=x86_64 target=template_release -j6
python3 tools/package_addon.py --platform linux
../../bin/godot.linuxbsd.editor.x86_64 --editor --path demo --rendering-method forward_plus --rendering-driver vulkan
```

Let the editor import the project, then press F5. The HUD reports actual source
dispatches and GPU passes. Controls: 1 Quality-style 0.67, 2 Native AA, 3 basic
compute, 4 bilinear, 5 scale 0.5, 6 native raster; F1–F4 station close-ups, F5
overview, Space pause, C camera motion, R reset history. See demo/ARTIFACT_LAB.md.

For another project, extract `build/fsr3-linux-x86_64.zip` into its root and enable
the **FidelityFX Upscaling** editor plugin. The autoload applies the viewport
settings without manually editing project scripts.

## Verification

From this directory, with a display and GPU available:

```sh
python3 tests/run_gpu_tests.py --engine ../../bin/godot.linuxbsd.editor.x86_64
python3 tests/run_gpu_tests.py --engine ../../bin/godot.linuxbsd.editor.x86_64 --library release
python3 tests/test_fidelityfx_library.py
python3 tests/test_binary_backend.py --engine ../../bin/godot.linuxbsd.editor.x86_64
```

The source suite checks actual GPU passes, Quality/Native AA/0.5 scaling, motion,
resizing, upscaler switching, and reset/sharpness changes. The external native
command test uses an explicitly synthetic provider, not AMD FSR4.

The optional FidelityFXUpscaler class dynamically loads compatible public-API
Vulkan runtimes on Windows; no AMD Windows DLL, custom FSR4 bridge, driver binary,
Wine runtime or portable compiler is distributed here. The native Linux addon
rejects Windows PE DLLs. Native Linux FSR4 is not supported by this release.

## Provenance and engine dependency

See ../../FSR3_EDITOR.md for source links, pinned revisions, additional engine
changes and release instructions. Algorithm source/notices are retained under
thirdparty/amd-ffx; Godot wrappers/shaders were adapted from HydrogenC's Godot
FSR3 implementation. godot-cpp is a pinned submodule. No generated extension API,
compiled library, object file, editor binary or test capture is versioned.

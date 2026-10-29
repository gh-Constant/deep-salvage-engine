# Built-in FSR 3.1.6

This module compiles the source FSR 3.1.6 upscaler and its shaders into the editor
and export templates. It is enabled by default for Linux and Windows builds with
3D support; `module_fsr3_enabled=no` excludes it. No addon, autoload, godot-cpp
bindings or AMD runtime binary is required for source FSR3.

## Project and viewport settings

Use Forward+ and select **FSR 3.1.6** under **Rendering → Scaling 3D → Mode**.
Choose the existing scale and FSR sharpness settings. Scale 1.0 provides Native
AA; 0.67 is Quality-style upscaling. FSR3 clamps scales above 1.0 to 1.0.
The viewport owns the backend, configures jitter and suppresses the separate TAA
pass while FSR3 is selected. The requested TAA setting is restored on leaving it.

From code, select `Viewport.SCALING_3D_MODE_FSR3`. `get_fsr3_upscaler()` exposes
the managed resource for diagnostics or `reset_history()` after a camera cut.
It is not the user-supplied `scaling_3d_custom_upscaler` property and is not saved
as a resource default. Each viewport has its own history. Ordinary custom
upscalers remain available through `SCALING_3D_MODE_CUSTOM`.

The native mode is a Viewport-level convenience over the existing generic
RenderingServer Custom mode. RenderingServer clients creating raw viewport RIDs
can continue assigning a built-in `FSR3Upscaler` resource through those hooks.

See [SDK_2_4.md](SDK_2_4.md) for the FSR 3.1.6 port and native FSR4 investigation.

## Optional external runtime

Under **Rendering → Scaling 3D → FSR3**, select the external AMD runtime backend
only when a compatible native **Vulkan** public-API library is available. Set
`library_path`, optionally `preferred_version`, and leave `allow_source_fallback`
enabled to retain source FSR3 when the provider cannot be used. The same options
are exposed as `fsr3_*` viewport properties. Source is the default on both platforms.

Libraries must be external files. On Windows, an empty path checks for
`amd_fidelityfx_loader_vk.dll`, then `amd_fidelityfx_vk.dll`, beside the executable.
No addon directory is used. Dependencies/providers stay alongside the loader.
The Windows editor release package includes AMD FidelityFX SDK 2.4.0's
redistributable Vulkan loader and upscaler DLLs plus AMD's license; the source
module itself does not distribute runtime binaries. A compatible AMD Vulkan
driver and the Microsoft Visual C++ runtime are also required. DLL naming alone
does not establish compatibility; DX12-only providers need a separate integration.
Windows PE DLLs cannot be used by native Linux. Native Linux FSR4 is not claimed.

## Migration and builds

Remove `addons/fsr3` and disable/remove its plugin and `FSR3Addon` autoload before
opening an old addon project with this branch. The old extension registers the
same class names as this module and must not be loaded alongside it. The prior
GDExtension implementation remains available on branch `fsr3-custom-editor`.

Build with the ordinary engine commands, for example:

```sh
scons platform=linuxbsd target=editor dev_build=no debug_symbols=no -j6
scons platform=linuxbsd target=template_debug debug_symbols=no -j6
scons platform=linuxbsd target=template_release debug_symbols=no -j6
```

Rebuild matching export templates to export games using this feature. Existing
templates from the GDExtension branch do not include these built-in classes.
The Windows build uses the existing LLVM-MinGW toolchain with Vulkan enabled.
Runtime GPU validation has been performed on native Linux and with the Windows
editor under Wine. Native Windows execution requires separate testing.
Native-AA, scale changes, resizing, mode switching,
SubViewports, source fallback and external callback lifetime are covered by tests.

```sh
python3 modules/fsr3/tests/run_builtin_test.py
python3 modules/fsr3/tests/test_fidelityfx_library.py
python3 modules/fsr3/tests/test_binary_backend.py --engine bin/godot.linuxbsd.editor.x86_64
python3 modules/fsr3/tests/run_export_test.py
```

## Release builds

Build all six targets at the same source commit: Linux and Windows editors,
plus `template_debug` and `template_release` for each platform. Linux releases
must include both X11 and Wayland in one editor. The backend is selected at
startup or with the editor's **Prefer Wayland** setting and a restart.

For portable Linux binaries, use an isolated Ubuntu 22.04 build environment
with `build-essential`, `pkg-config`, `python3`, `python3-pip`, `git`,
`libwayland-bin`, SCons 4.11.0 and the Python `zstandard` module. Build with `x11=yes wayland=yes` and keep
its objects separate from host builds. Windows uses the pinned LLVM-MinGW
20261020 compiler with Vulkan enabled and `d3d12=no`.

After all targets have been built, run:

```sh
git submodule update --init thirdparty/FidelityFX-SDK
python3 misc/fsr3-gdextension/tools/package_release.py
```

The SDK submodule is pinned to AMD's `v2.4.0` release
(`7539ae8b6568ad29ed78a753404351daf3b5cca7`). Git records the exact commit, so
ordinary editor checkouts do not move to a newer SDK release.

Initialize the pinned SDK submodule with
`git submodule update --init thirdparty/FidelityFX-SDK` before packaging.
The packager uses this submodule as its only default SDK source; an alternate
checkout can be explicitly selected with `--fidelityfx-sdk /path/to/FidelityFX-SDK`.
It validates the two SDK 2.4.0 runtime DLL hashes and includes their license
notice. Export templates and exported games do not contain these DLLs
automatically. To use the optional runtime in an exported Windows game, copy the
two DLLs and `AMD-FSR-SDK-LICENSE.md` from the editor package beside the game
executable. Native Linux FSR4 is not provided by these Windows runtime files.

The output contains editor ZIPs, per-platform debug/release template ZIPs,
the addon-free demo, complete source ZIP/tar.gz and SHA256SUMS.txt.
BUILD_INFO.json records the revision and each Linux binary's required glibc
version. Template ZIPs are selected with the export preset's Custom Template
fields; they are not all-platform TPZ installers. Complete native GPU/export
validation before publishing with `gh release create`.

The binary callback test uses an explicitly synthetic green-output provider.
It proves Vulkan interop and lifetime handling, not AMD FSR4 execution.

## Provenance and licenses

Ported from this fork's companion GDExtension to native Godot RenderingDevice
types and engine Resource classes. The algorithm and shader logic are preserved.
The backend and vendored AMD code originate from HydrogenC's FSR 3.1.4 port,
commit `a2f49a1676608b29a0ce39fe20bc43119b609008`, based on FidelityFX SDK 1.1.4:

- https://github.com/godotengine/godot/pull/111631
- https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/tag/v1.1.4

The generic custom-upscaler hooks originate from Bastiaan Olij's proposal:
https://github.com/godotengine/godot/pull/114670

Godot/AMD MIT notices remain in the sources and the root LICENSE/COPYRIGHT files.
SDK assertion symbols are prefixed in this module to coexist with Godot's FSR2
SDK. No private bridge, AMD driver binary, frame generation or Wine is included.

The FSR 3.1.6 update is sourced from AMD SDK 2.4.0 (`7539ae8`). The retained
Godot host-ABI and shader adaptation boundaries are documented in
`thirdparty/amd-ffx/GODOT_PORT.md`. The external Vulkan adapter uses SDK 2.4
headers and device-aware provider enumeration; actual AMD FSR4 execution is
not claimed by the synthetic provider tests.

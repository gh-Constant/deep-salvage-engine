# Built-in FSR 3.1.4

This module compiles the source FSR 3.1.4 upscaler and its shaders into the editor
and export templates. It is enabled by default for Linux and Windows builds with
3D support; `module_fsr3_enabled=no` excludes it. No addon, autoload, godot-cpp
bindings or AMD runtime binary is required for source FSR3.

## Project and viewport settings

Use Forward+ and select **FSR 3.1.4** under **Rendering → Scaling 3D → Mode**.
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

## Optional external runtime

Under **Rendering → Scaling 3D → FSR3**, select the external AMD runtime backend
only when a compatible native **Vulkan** public-API library is available. Set
`library_path`, optionally `preferred_version`, and leave `allow_source_fallback`
enabled to retain source FSR3 when the provider cannot be used. The same options
are exposed as `fsr3_*` viewport properties. Source is the default on both platforms.

Libraries must be external files. On Windows, an empty path checks for
`amd_fidelityfx_loader_vk.dll`, then `amd_fidelityfx_vk.dll`, beside the executable.
No addon directory is used. Dependencies/providers stay alongside the loader.
AMD runtime/driver DLLs are not distributed by this module. DLL naming alone does
not establish compatibility; DX12-only providers need a separate integration.
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
Runtime GPU validation has been performed on Linux; native Windows execution
requires separate testing. Native-AA, scale changes, resizing, mode switching,
SubViewports, source fallback and external callback lifetime are covered by tests.

```sh
python3 modules/fsr3/tests/run_builtin_test.py
python3 modules/fsr3/tests/test_fidelityfx_library.py
python3 modules/fsr3/tests/test_binary_backend.py --engine bin/godot.linuxbsd.editor.x86_64
python3 modules/fsr3/tests/run_export_test.py
```

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

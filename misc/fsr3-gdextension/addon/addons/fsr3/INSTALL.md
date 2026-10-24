# FidelityFX Upscaling addon

This prototype needs this fork's Godot 4.8 custom-upscaler editor with the
temporal metadata, Custom-mode lifetime, and native-compute-callback patches from fsr3-gdextension.
Official Godot builds and the original engine-integrated FSR3 build cannot load
this addon. The source backend is GPU-tested on Linux x86_64, Forward+, Vulkan, mono
perspective cameras and fixed exposure. This release contains native Linux binaries only.

1. Extract the archive so this folder is `res://addons/fsr3/` in your project.
2. Open the project with the patched custom-upscaler engine.
3. Select Forward+ and Vulkan.
4. Enable **FidelityFX Upscaling** in Project Settings → Plugins.

The plugin installs a persistent autoload that configures the root viewport.
Project settings under `rendering/fsr3` control enabled, render_scale (0.67),
native_aa (scale 1.0), and sharpness (0–1). Disabling the plugin removes its
autoload. No changes to project scripts are needed.

`FSR3Upscaler` is also a selectable resource for Custom-mode SubViewports.
Call `configure_scale(scale)` before changing viewport scale from code, and
`reset_history()` for camera cuts. Use a separate resource for each viewport
scale. Missing engine metadata is reported explicitly; there is no silent
fallback. SDK licenses and source notices are under `licenses/`.


The optional `FidelityFXUpscaler` class can dynamically load compatible Windows
Vulkan runtimes, but this Linux release ships no Windows runtime DLL or FSR4
provider. Native Linux uses source FSR 3.1.4. Windows PE DLLs cannot be loaded
by the native Linux addon.

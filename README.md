# Godot Engine

This fork's `fsr3-builtin` branch includes the built-in FSR 3.1.6 upscaler module.
Project-specific build, runtime, release and validation details are documented
below and in [modules/fsr3/README.md](modules/fsr3/README.md).

<p align="center">
  <a href="https://godotengine.org">
    <img src="misc/logo/logo_outlined.svg" width="400" alt="Godot Engine logo">
  </a>
</p>

## 2D and 3D cross-platform game engine

**[Godot Engine](https://godotengine.org) is a feature-packed, cross-platform
game engine to create 2D and 3D games from a unified interface.** It provides a
comprehensive set of [common tools](https://godotengine.org/features), so that
users can focus on making games without having to reinvent the wheel. Games can
be exported with one click to a number of platforms, including the major desktop
platforms (Linux, macOS, Windows), mobile platforms (Android, iOS), as well as
Web-based platforms and [consoles](https://godotengine.org/consoles).

## Free, open source and community-driven

Godot is completely free and open source under the very permissive [MIT license](https://godotengine.org/license).
No strings attached, no royalties, nothing. The users' games are theirs, down
to the last line of engine code. Godot's development is fully independent and
community-driven, empowering users to help shape their engine to match their
expectations. It is supported by the [Godot Foundation](https://godot.foundation/)
not-for-profit.

Before being open sourced in [February 2014](https://github.com/godotengine/godot/commit/0b806ee0fc9097fa7bda7ac0109191c9c5e0a1ac),
Godot had been developed by [Juan Linietsky](https://github.com/reduz) and
[Ariel Manzur](https://github.com/punto-) for several years as an in-house
engine, used to publish several work-for-hire titles.

![Screenshot of a 3D scene in the Godot Engine editor](https://raw.githubusercontent.com/godotengine/godot-design/master/screenshots/editor_tps_demo_1920x1080.jpg)

## Getting the engine

### Binary downloads

Official binaries for the Godot editor and the export templates can be found
[on the Godot website](https://godotengine.org/download).

### Compiling from source

[See the official docs](https://docs.godotengine.org/en/latest/engine_details/development/compiling)
for compilation instructions for every supported platform.

## Community and contributing

Godot is not only an engine but an ever-growing community of users and engine
developers. The main community channels are listed [on the homepage](https://godotengine.org/community).

The best way to get in touch with the core engine developers is to join the
[Godot Contributors Chat](https://chat.godotengine.org).

To get started contributing to the project, see the [contributing guide](CONTRIBUTING.md).
This document also includes guidelines for reporting bugs.

## Documentation and demos

The official documentation is hosted on [Read the Docs](https://docs.godotengine.org).
It is maintained by the Godot community in its own [GitHub repository](https://github.com/godotengine/godot-docs).

The [class reference](https://docs.godotengine.org/en/latest/classes/)
is also accessible from the Godot editor.

We also maintain official demos in their own [GitHub repository](https://github.com/godotengine/godot-demo-projects)
as well as the [Asset Store](https://store.godotengine.org/).

There are also a number of other
[learning resources](https://docs.godotengine.org/en/latest/community/tutorials.html)
provided by the community, such as text and video tutorials, demos, etc.
Consult the [community channels](https://godotengine.org/community)
for more information.

[![Code Triagers Badge](https://www.codetriage.com/godotengine/godot/badges/users.svg)](https://www.codetriage.com/godotengine/godot)
[![Translate on Weblate](https://hosted.weblate.org/widgets/godot-engine/-/godot/svg-badge.svg)](https://hosted.weblate.org/engage/godot-engine/?utm_source=widget)

# Built-in FSR3 custom editor

Branch `fsr3-builtin` is based on upstream Godot master `d40e9472f6`. The built-in
FSR 3.1.6 source upscaler is compiled into `modules/fsr3/` and is available
through ordinary project and viewport settings.
No per-project addon or autoload is required. The module is included in matching
editor and export-template builds on Linux and Windows with 3D support.

Select **Rendering → Scaling 3D → Mode → FSR 3.1.6** with Forward+.
Use scale 0.67 for Quality-style upscaling or 1.0 for Native AA.
See [modules/fsr3/README.md](modules/fsr3/README.md) for backend settings, migration,
builds, tests and limitations. Remove the old FSR3 addon before opening an addon
project with this branch; the old extension registers the same class names.

The generic custom-upscaler hooks are retained. Native source and optional
FidelityFX Vulkan runtime resources reuse them. Source is the default on both
platforms. The Windows editor release bundles the redistributable AMD FidelityFX
SDK 2.4.0 Vulkan loader and upscaler DLLs, with AMD's license. These are not
included in Linux releases, export-template packages, or exported games.
The SDK 2.4 public Vulkan API integration is experimental. FSR 4.1.2 rendering
has been tested under Wine on an RX 9070 XT/RADV; native Windows GPU execution
and driver overrides remain unvalidated. Native Linux FSR4 is not provided.
See [modules/fsr3/SDK_2_4.md](modules/fsr3/SDK_2_4.md) for the actual native-loader
test and remaining runtime/device-feature requirements.

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
- **AMD FidelityFX SDK 2.4.0:**
  https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK/releases/tag/v2.4.0
  Pinned as the `thirdparty/FidelityFX-SDK` submodule at commit
  `7539ae8b6568ad29ed78a753404351daf3b5cca7`. This is the source reference for
  the analytical FSR 3.1.6 update and the SDK 2.4 public Vulkan API headers used
  by the optional external-runtime adapter. The Windows editor package includes
  the SDK's redistributable `amd_fidelityfx_loader_vk.dll` and
  `amd_fidelityfx_upscaler_vk.dll`, with AMD's redistribution license; the
  packager verifies both DLL hashes. These Windows runtime binaries are not
  included in Linux packages or export templates. Exported games require manual
  deployment of the DLLs and license beside the executable. The SDK does not
  provide a native Linux FSR4 runtime. See
  [GODOT_PORT.md](thirdparty/amd-ffx/GODOT_PORT.md) for the analytical source
  changes and retained Godot adaptation boundaries, and
  [SDK_2_4.md](modules/fsr3/SDK_2_4.md) for runtime limitations and validation.

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

The analytical source was updated to FSR 3.1.6 from AMD SDK 2.4.0, revision
`7539ae8b6568ad29ed78a753404351daf3b5cca7`, while retaining the original Godot
RenderingDevice adaptation. See `thirdparty/amd-ffx/GODOT_PORT.md`.

## FSR versions in this fork

- **Built-in source upscaler:** FSR 3.1.6, included in the editor and export
  templates. This is the default backend on Linux and Windows.
- **AMD SDK source and optional Vulkan runtime:** pinned FidelityFX SDK `v2.4.0`
  at `7539ae8b6568ad29ed78a753404351daf3b5cca7` as the
  `thirdparty/FidelityFX-SDK` submodule. The Windows editor package includes its
  redistributable Vulkan loader and upscaler DLLs.
- **FSR 4 runtime validation:** FSR 4.1.2 dispatched under Wine on an RX 9070
  XT/RADV. This does not establish native Windows validation; native Linux FSR4
  is not provided by the SDK's Windows DLLs.

The original HydrogenC port was based on FidelityFX SDK 1.1.4; the current
FSR 3.1.6 source update and optional runtime packaging use SDK 2.4.0. The 1.1.4
reference below is historical provenance, not the SDK version pinned by this
fork.

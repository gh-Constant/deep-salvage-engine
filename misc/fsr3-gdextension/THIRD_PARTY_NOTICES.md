The FidelityFX SDK CPU scheduling code and GPU headers under thirdparty/amd-ffx
come from HydrogenC's Godot FSR 3.1.4 implementation at a2f49a1676. They retain
AMD's MIT license notices (Copyright 2024 Advanced Micro Devices, Inc.). This is
the Godot-adapted SDK 1.1.4 code, including its existing shader changes, rather
than a new implementation of the FSR algorithm.

src/ffx_common.* and src/fsr3_backend.* are adapted from that same Godot branch
and retain the Godot Engine MIT notices. LICENSE-Godot.txt contains the license.

The rendering flow was studied from BastiaanOlij/test-custom-upscaler commit
9d89b245dd4be30eda80622efb8389888391f8d7. LICENSE-Demo.txt preserves its MIT license.
The basic C++ shader supports arbitrary input/output ratios rather than the
demo's fixed 2x upscale.

godot-cpp is MIT licensed; see godot-cpp/LICENSE.md. Exact upstream source
revisions and the engine dependency are recorded in README.md.


No Windows runtime DLL is distributed in this source tree or the Linux release.
The MIT-licensed public API headers in thirdparty/amd-fidelityfx-api are from
that same tag; only the export annotation was disabled for dynamic consumers
and stddef.h added for portable wchar_t declaration. The separate downloaded
SDK 2.3.0 checkout is a documentation reference, not linked or redistributed.

thirdparty/vulkan-headers contains C Vulkan headers from the preserved Godot
custom-upscaler checkout. Its upstream LICENSE.md and per-file notices remain.
The original Godot-adapted FFX message implementation uses std::swprintf for
MinGW compatibility; the upscaling algorithm is unchanged by that portability fix.

The synthetic native compute provider is test-only; its build outputs are
excluded from source control and release packages. No compiler toolchain or
private driver binary is distributed.

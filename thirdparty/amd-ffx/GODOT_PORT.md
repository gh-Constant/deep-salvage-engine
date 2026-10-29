# Godot analytical FSR 3.1.6 port

Source reference: AMD FSR SDK 2.4.0, commit
`7539ae8b6568ad29ed78a753404351daf3b5cca7` (tag `v2.4.0`).
The initial Godot adaptation came from HydrogenC's FSR 3.1.4 port of SDK 1.1.4.

The 3.1.6 update was compared against the SDK 1.1.4 source baseline, not inferred
from DLL version strings. The shared FSR3 shader headers are largely unchanged.
The published algorithm fix changes half-precision history reprojection to use
`PreviousFrameUpscaleSize()` rather than the current `UpscaleSize()`. The updated
HLSL bindings/core helpers and dispatch parameter validation are also ported.
Godot's sharpening permutation now defines the FSR3 sharpening flag instead of
the unrelated FSR2 flag.

The RenderingDevice adapter retains its SDK 1.x host callback ABI. SDK 2.x host
changes for precompiled shader blobs, public provider APIs, resource-description
helpers and memory-estimation queries are not prerequisites for this native
shader port. They are not substituted into the older callback ABI. The newer
UAV transitions before alias-resource discard are handled by RenderingDevice's
per-pass resource dependencies; this adapter allocates separate textures rather
than physically aliasing those allocations. Godot's reprojection matrix, reactive
alpha handling, invalid-motion reconstruction and reverse-Z conventions remain.

Godot currently selects FP32 source permutations. Thus the half-history fix is
present in the shared source but is not an assertion of a visible quality change
in this configuration. No claim is made that AMD's opaque signed-DLL output is
bit-for-bit identical to this source adaptation. `get_source_version()` reports
the compiled analytical effect version (3.1.6), not the optional runtime version.

The separate `thirdparty/amd-fidelityfx-api` consumer headers are from SDK 2.4.0.
Only consumer portability, include paths and whitespace are adjusted: API entry
points are resolved dynamically on Linux and Windows rather than exported from
Godot. The optional Vulkan adapter uses the new backend descriptor/device-aware
provider queries. These headers do not contain the FSR4 ML implementation.

`modules/fsr3/SCsub` prefixes the private SDK's `Ffx*` C++ types as
`GodotFSR3_Ffx*`, in both adapter and vendor translation units. Godot's FSR2 SDK
declares global types with the same names but different layouts. Without this
isolation, emitted template functions such as
`LocalVector<FfxGpuJobDescription>::reserve` have identical linker names and can
allocate the wrong element size, causing heap corruption. This was reproduced
with the Windows LLVM-MinGW build under Wine, including the earlier 3.1.4 build;
the isolated 3.1.6 build passes the Vulkan demo. This changes private C++ symbol
names, not the external AMD public-API ABI or shader resource layouts.

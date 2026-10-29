# SDK 2.4 and native FSR4 investigation

The source backend is the Godot adaptation of analytical FSR 3.1.6. See
`thirdparty/amd-ffx/GODOT_PORT.md` for the exact port boundaries and provenance.

The optional external Vulkan adapter consumes the SDK 2.4 API. It passes a
`VkDeviceContext` to version enumeration and uses the new Vulkan backend type
`0x01000003`. An empty preferred version means no `ffxOverrideVersion`, retaining
AMD's automatic provider choice. Explicit selections pin a provider; active
provider names, including driver-provided ` *` suffixes, are reported unchanged.
The synthetic native provider test covers device-aware enumeration, explicit
selection, unpinned Auto, GPU output, resizing, fallback and deferred destruction.
It is not an AMD FSR4 implementation or an AMD hardware-compatibility test.

## Actual native Linux test

Run:

```sh
python3 modules/fsr3/tests/test_sdk24_native.py
```

This attempts Linux `dlopen` on the real `amd_fidelityfx_loader_vk.dll` and
`amd_fidelityfx_upscaler_vk.dll`, without Wine. Both fail with `invalid ELF header`.
They are Windows AMD64 PE binaries; the upscaler also imports `vulkan-1.dll`,
MSVCP140 and VCRUNTIME140 DLLs. Their Windows ABI/runtime dependencies remain
necessary even though their GPU backend is Vulkan.

The probe also launches native Linux Godot with the real SDK loader path and
checks the active backend and version. It must show source 3.1.6, successful
source GPU work and **zero external FSR4 dispatches**. This prevents mistaking a
working fallback for FSR4 support.

No ELF ML runtime or complete FSR4 ML implementation source is included in this
SDK. The open source under `upscalers/fsr3` implements the analytical FSR2/FSR3
providers. Building those sources into a `.so` does not create FSR4. Renaming or
repackaging the Windows DLL likewise does not supply the Windows runtime or its
calling conventions. A native Linux FSR4 implementation needs a native AMD ML
provider, sufficient implementation sources, or a separately engineered runtime
compatibility solution. The latter is not provided or tested here.

## Windows status and remaining validation

The adapter is updated and Windows editors/templates can be cross-compiled, but
real AMD FSR4 GPU output still requires testing on native Windows. Our Windows
builds retain Vulkan and `d3d12=no`; this work does not add a DX12 integration.
The Windows editor and analytical source FSR3.1.6 demo pass under Wine 11.17
after isolating the private SDK's C++ types from Godot's FSR2 types. This does not
validate the optional AMD FSR4 provider; see `tests/RESULTS.md` for the checks.
Deployment needs the SDK 2.4 Vulkan loader/upscaler pair and their Microsoft C++
runtime dependencies. Do not substitute renamed DX12 binaries or the older
custom bridge when validating the official Vulkan provider.

Real-provider validation must also audit Vulkan logical-device features before
device creation. AMD's sample enables integer dot products, cooperative matrices
and, when supported, shader float8/cooperative-matrix float8. These feature paths
are not all negotiated by our Godot driver. The provider's exact model/device
requirements and required features must be validated before claiming working
FSR4; successful header compilation or synthetic dispatch is insufficient.
Frame generation and swapchain/pacing integration remain separate work.

References:
- https://gpuopen.com/amd-fsr-sdk/
- https://gpuopen.com/learn/amd-fsr-sdk-2-4-rx-6000-ryzen-apus/
- SDK `Samples/Upscalers/FidelityFX_FSR/vk/fsrapirendermodule.cpp`
- SDK `Kits/Cauldron2/framework/render/vk/device_vk.cpp`

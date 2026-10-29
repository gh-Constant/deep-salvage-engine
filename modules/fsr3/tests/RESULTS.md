# FSR 3.1.6 / SDK 2.4 validation — 29 October 2026

Native Linux x86_64, RX 9070 XT / RADV, Forward+ / Vulkan 1.4.354.

| Check | Result |
| --- | --- |
| Built-in source version | `get_source_version()` reports 3.1.6; fresh-project assertion passed |
| Linux editor + debug/release templates | All built in the isolated Ubuntu 22.04 environment; glibc baseline retained |
| Windows editor + debug/release templates | All cross-built using LLVM-MinGW 20261020; editor tested under Wine 11.17; native Windows execution untested |
| Windows editor under Wine / Vulkan | Demo imported, scene opened and saved with safe-save enabled, editor image captured, exit 0 |
| Windows source FSR3 under Wine / Vulkan | 269 FSR frames, 2,152 GPU passes, 5 contexts, 45 basic-compute frames, 0 failures; resize, Native AA and mode switching passed |
| Settings-only source GPU test | 149 FSR frames, 1,192 GPU passes, 6 contexts, 0 failures |
| Independent SubViewport | 189 source frames with separate history |
| Native AA, scale/size changes, history reset and switching | Passed |
| Missing runtime fallback | 30 successful source FSR3.1.6 frames |
| Public-API loader checks | Loading, missing exports, PE rejection and replacement passed |
| SDK 2.4 synthetic Vulkan provider | 89 dispatches, green output, device-aware query, pinned selection, unpinned Auto, resize, fallback and balanced lifetime passed |
| Actual SDK 2.4 DLL native Linux `dlopen` | Both Vulkan loader and upscaler fail with `invalid ELF header` |
| Actual SDK path in native Godot | Explicit PE rejection, active source version 3.1.6, successful source GPU work, zero FSR4 dispatches |
| Linux debug/release demo exports | Both exported and ran on native Vulkan without an FSR extension library |
| Windows debug/release demo exports | Both exported and ran under Wine / Vulkan: 269 FSR frames, 2,152 GPU passes, 0 failures each; native Windows untested |
| Class-reference XML | No warnings or errors; 5 existing code-sample parity notices |

The synthetic provider is not AMD FSR4. Updating its ABI does not demonstrate
actual AMD FSR4 execution. Native Windows tests and the logical-device feature
audit remain necessary; see `../SDK_2_4.md`. The native Linux checks above do not
use Wine. The separate Windows execution checks use an isolated Wine prefix.

Wine testing exposed a private-SDK C++ type collision with Godot's FSR2 SDK.
Both SDKs instantiated template symbols with the same names but different
element layouts, causing heap corruption when enabling source FSR3. The earlier
3.1.4 Windows build failed the same control test. `SCsub` now prefixes the private
SDK's `Ffx*` types, and the rebuilt 3.1.6 Windows demo passes. Native Linux
regression testing also passes after this change.

Wine still reports unsupported Windows toast notification activation
(`0x80040154`) and SDR white-level queries. They do not prevent these checks from
passing. Initial editor safe-save errors did not recur in the final short-path
`C:\fsr-test` check after rebuilding; normal safe-save was enabled and saving
returned `OK`.
The tests do not establish native Windows driver or AMD FSR4 runtime support.

See `thirdparty/amd-ffx/GODOT_PORT.md` for exact source-port boundaries. The current
Godot source adapter uses FP32 permutations; output equivalence with AMD's signed
DLL or an improvement in every ghosting scenario is not claimed.

Current build/test logs are copied to the ignored `bin/fsr316-validation/` folder.
The older release build logs under `bin/release-build-2026-10-28/` are historical;
the export validation helper was reused against the newly rebuilt binaries.

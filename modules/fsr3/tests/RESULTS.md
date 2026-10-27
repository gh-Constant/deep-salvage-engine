# Built-in FSR3 validation

Native Linux x86_64, RX 9070 XT / RADV, Forward+ / Vulkan 1.4.354.

| Check | Result |
| --- | --- |
| Linux editor build | Passed |
| Windows x86_64 editor cross-build, Vulkan | Passed; PE imports checked |
| Linux release export-template build | Passed |
| Fresh project, settings-only FSR3, no addons directory | Passed |
| Source resource is registered as native core API | Passed |
| Quality / Native AA / 0.5 scale, resize, sharpness, reset, mode switching | Passed |
| Fresh-project source test | 149 FSR frames, 1,192 GPU passes, 6 contexts, 0 failures |
| Independent SubViewport | 189 FSR frames; separate resource/history |
| Missing external runtime with source fallback | 30 successful source frames |
| Native public-API loader tests | Loading, missing exports, Windows PE rejection on Linux, provider replacement passed |
| Synthetic external Vulkan provider | 89 dispatches; green GPU output, resize, provider selection, fallback and balanced context destruction passed |
| Addon-free artifact lab in editor build | 269 FSR frames, 2,152 GPU passes, 5 contexts, 0 failures |
| Exported addon-free release game | Same artifact-lab counts, 0 failures; executable + PCK, no extension .so/.dll |
| API reference XML | No warnings/errors with all module/platform reference types loaded |

Logs, captures and exported test binaries are under the ignored engine `bin/`
and companion `tests/output/` directories. The synthetic provider is not AMD FSR4.
Windows GPU execution, NVIDIA hardware and real external AMD runtime execution
have not been validated here. No Wine was used.

These are development builds on the host Linux distribution. They are not a
portable release baseline: an older supported Linux build environment is still
needed before broad binary distribution. Linux/Windows debug export templates
and the Windows release template are not part of this validation run. Existing
template-source checkouts and earlier GitHub releases remain on the old revision.
The unified editor/template build and GitHub publishing pipeline remains future work.

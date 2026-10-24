# Validation — 24 October 2026

Base: upstream Godot `c8256a8ecec068340753869e423dd478c84ef167` (4.8 beta), plus
this branch's custom-upscaler import and isolated engine APIs/fixes. Bindings
were generated from the actual built editor. Host: Nobara Linux, RX 9070 XT,
Mesa RADV, Forward+/Vulkan 1.4.354. No Wine or driver changes were used.

| Check | Result |
| --- | --- |
| Native Linux editor build, 6 jobs | Passed |
| Linux debug/release addon builds | Passed |
| Source FSR3 debug GPU suite | 269 FSR frames, 2,152 GPU passes, 5 contexts, 0 failures |
| Source FSR3 release GPU suite | Same counts, 0 failures |
| Quality 0.67 / Native AA 1.0 / scale 0.5 | Passed |
| Camera/object motion, window resize, upscaler switching, history reset | Passed |
| Native external Vulkan callback, explicitly synthetic provider | 89 dispatches, GPU green output verified, resize/fallback/context lifetime passed |
| Public-API library loading/export/version tests | Passed |
| Fresh-project install from Linux addon ZIP | Plugin/autoload configured FSR3 without project script changes |
| Engine API XML checks | No XML warnings/errors |
| Whitespace check for authored code | Passed; upstream vendor/diff formatting preserved |

Seven captures per source suite were saved locally; the release suite's
disocclusion capture was visually inspected. Logs/captures, compiled libraries,
generated API JSON and caches are ignored and excluded from Git.

The source suite requires real GPU compute dispatches and rejects Godot/script
errors even when the engine exits with code zero. This is functional testing,
not a ghosting/image-quality fix or a performance benchmark. The native external
callback test uses TEST_GREEN_COMPUTE_ONLY, not AMD FSR4. Native Linux FSR4 and
Windows execution are not part of this release. Khronos validation layers were
not available on the host.

See ../README.md for reproduction commands.

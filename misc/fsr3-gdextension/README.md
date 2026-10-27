# Built-in FSR3 companion demo and packaging

On branch `fsr3-builtin`, FSR 3.1.4 is compiled into `modules/fsr3/`.
This folder keeps its historical name so existing workspace paths remain valid.
The old extension source is reference material; it is not loaded or packaged
with the built-in editor. The original extension remains on `fsr3-custom-editor`.

See [the native module guide](../../modules/fsr3/README.md) for configuration,
optional external Vulkan runtimes, migration, builds, tests and provenance.
There is no per-project addon, autoload or godot-cpp build requirement.

## Run the artifact lab

From the engine repository root:

```sh
scons platform=linuxbsd target=editor dev_build=no debug_symbols=no -j6
bin/godot.linuxbsd.editor.x86_64 --editor --path misc/fsr3-gdextension/demo --rendering-method forward_plus --rendering-driver vulkan
```

After import, press F5. The HUD reports actual source dispatches and GPU passes.
Controls: 1 Quality-style 0.67, 2 Native AA, 3 basic compute, 4 bilinear,
5 scale 0.5, 6 native raster; F1–F4 close-ups, F5 overview, Space pause,
C camera motion, R history reset. See `demo/ARTIFACT_LAB.md`.
The project enables built-in FSR3 in its rendering settings and has no addons folder.

## Validate

```sh
python3 modules/fsr3/tests/run_builtin_test.py
python3 modules/fsr3/tests/test_fidelityfx_library.py
python3 modules/fsr3/tests/test_binary_backend.py --engine bin/godot.linuxbsd.editor.x86_64
python3 misc/fsr3-gdextension/tests/run_gpu_tests.py --engine bin/godot.linuxbsd.editor.x86_64
python3 modules/fsr3/tests/run_export_test.py
```

The export test requires a matching native Linux release template built with
`target=template_release`. Older extension-branch templates lack the native classes.
The legacy `--library release` addon test and addon installer tests are for the
old branch; use the native export test to validate built-in release runtimes.

## Package local builds

After building Linux and Windows editors and committing the source:

```sh
python3 misc/fsr3-gdextension/tools/package_release.py
```

This writes built-in editor ZIPs, an addon-free demo, complete source ZIP/tar.gz
archives and SHA-256 checksums to `bin/fsr3-builtin-release/`.
The source archives include the pinned legacy godot-cpp checkout for completeness;
compiling the native engine/module does not require those bindings.
This packager does not yet build/package all export templates or publish a GitHub
release. A unified build/publish workflow remains separate work.

The native source backend is GPU-tested on Linux Forward+/Vulkan. Windows editor
binaries are cross-compiled; native Windows GPU execution is not validated here.
No AMD runtime/driver DLLs, native Linux FSR4, frame generation or Wine are included.

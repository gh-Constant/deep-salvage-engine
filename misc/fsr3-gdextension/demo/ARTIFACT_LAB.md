# Temporal Artifact Lab

Open this project with `godot-custom-upscaler-fsr-api` and press F5. The default
scene is `artifact_lab.tscn`; the earlier block-grid scene is still `main.tscn`.
All geometry, materials and GPU particles are generated locally, with no asset
downloads. No FSR implementation or image-quality settings were changed.

| View | Stress case | What to look for |
| --- | --- | --- |
| F1 | Dark slats and a bright moving sphere over fine checker detail | Smears, retained silhouettes, and lost background detail just after it becomes visible |
| F2 | Thin fence, diagonal line and rotating 22-spoke wheel | Shimmer, moiré, unstable coverage, disappearing wires, edge breakup |
| F3 | Moving alpha-blended glass, animated translucent ribbon, 256 GPU sparks | Particle trails, transparent-edge ghosts, delayed or unstable background detail |
| F4 | Low-roughness metallic spheres/torus, moving small light, HDR emissive bar | Specular sparkle, trailing highlights, halos/ringing, delayed lighting changes |
| F5 | Overview of all four bays | Camera-pan instability and differences across content types |

Start with F1 and **1** (Quality-style 0.67). Press **C** to stop camera motion
and isolate the moving objects. Use **[** to slow motion to 0.5x, 0.25x or 0.125x.
Then compare **5** (Performance 0.5), **2** (Native AA), and **6** (native raster,
full resolution without temporal AA). **4** gives bilinear at the same 0.67 scale;
**3** gives the basic compute reference. The HUD reports the actual active mode.

**Space** pauses scene animation, particles and camera motion. Rendering and
history accumulation continue, so differences that disappear after stopping
motion are easier to distinguish from static aliasing. Resume to observe the
history response. FSR remains at its configured default sharpness of 0.2 during
interactive use.

**X** jumps to the next bay without resetting FSR history on purpose. Compare
with **R**, which explicitly resets it. F1–F5 view selection resets history so
ordinary station selection does not start with a stale-camera transient.

The checker backplates deliberately contain high-frequency procedural detail.
They can alias even in native raster mode; this is a stress input, not proof that
every visible shimmer comes from FSR. Compare modes at the same camera and motion
speed. An artifact is not guaranteed on every GPU, scale, or frame. Glow is off
so bloom does not hide temporal trails.

The inherited `--verify` sequence tours these bays while testing FSR Quality,
Native AA, half scale, odd-size viewport resizing, basic/bilinear switching,
sharpness changes and explicit history reset. Captures and logs go under
`tests/output/artifact-lab/`. The scene is for exposing behavior; no ghosting
correction, frame generation or FSR4 implementation is included.

from pathlib import Path
import re


def generate(target, source, env):
    root = Path(str(target[0])).resolve().parent

    def expand(path, stack=()):
        path = path.resolve()
        if path in stack:
            raise RuntimeError(f"Recursive shader include: {path}")

        def include(match):
            name = match.group(1)
            included = root.parent.parent / name if name.startswith("thirdparty/") else path.parent / name
            return expand(included, (*stack, path))

        return re.sub(r'^\s*#include "([^"\n]+)".*$', include, path.read_text(), flags=re.M)

    text = "// Generated FidelityFX shaders. Do not edit.\n#pragma once\n"
    for path in sorted((root / "shaders/fsr3").glob("*.glsl")):
        shader = expand(path).replace("#[compute]", "").replace("#VERSION_DEFINES", "")
        # A byte array, not a raw string literal: MSVC rejects string literals over 16 KB (C2026),
        # and these shaders are hundreds of KB once their includes are expanded.
        data = shader.encode("utf-8") + b"\0"
        rows = [",".join(str(b) for b in data[i : i + 64]) for i in range(0, len(data), 64)]
        text += f"inline const char {path.stem}_data[] = {{\n" + ",\n".join(rows) + "\n};\n"
        text += f"inline const char *{path.stem} = {path.stem}_data;\n"
    Path(str(target[0])).write_text(text)

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
        text += f'inline const char *{path.stem} = R"FSR_SHADER({shader})FSR_SHADER";\n'
    Path(str(target[0])).write_text(text)

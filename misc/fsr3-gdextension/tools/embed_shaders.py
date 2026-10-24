from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
def expand(path, stack=()):
    path = path.resolve()
    if path in stack:
        raise RuntimeError(f"Recursive shader include: {path}")
    source = path.read_text()
    def include(m):
        name = m.group(1)
        target = ROOT / name if name.startswith("thirdparty/") else path.parent / name
        return expand(target, (*stack, path))
    return re.sub(r'^\s*#include "([^"\n]+)".*$', include, source, flags=re.M)
def generate():
    files = sorted((ROOT / "shaders/fsr3").glob("*.glsl"))
    text = "// Generated from the vendored FidelityFX GLSL. Do not edit.\n#pragma once\n"
    for path in files:
        source = expand(path).replace("#[compute]", "").replace("#VERSION_DEFINES", "")
        text += f'inline const char *{path.stem} = R"FSR_SHADER({source})FSR_SHADER";\n'
    (ROOT / "build/shaders.gen.h").write_text(text)
if __name__ == "__main__":
    generate()

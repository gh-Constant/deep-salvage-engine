#!/usr/bin/env python3
"""Test a fresh project with built-in FSR3 enabled only by project settings."""
from pathlib import Path
import argparse
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, default=ROOT / "bin/godot.linuxbsd.editor.x86_64")
    args = parser.parse_args()
    output = ROOT / "bin/fsr3-builtin-tests"
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="fsr3-builtin-") as temporary:
        project = Path(temporary)
        shutil.copy2(Path(__file__).with_name("builtin_test.gd"), project / "main.gd")
        (project / "main.tscn").write_text('[gd_scene load_steps=2 format=3]\n[ext_resource type="Script" path="res://main.gd" id="1"]\n[node name="BuiltinTest" type="Node3D"]\nscript=ExtResource("1")\n')
        (project / "project.godot").write_text('config_version=5\n[application]\nconfig/name="Built-in FSR3 test"\nrun/main_scene="res://main.tscn"\nconfig/features=PackedStringArray("4.8")\n[display]\nwindow/size/viewport_width=960\nwindow/size/viewport_height=540\n[rendering]\nrenderer/rendering_method="forward_plus"\nscaling_3d/mode=7\nscaling_3d/scale=0.67\nanti_aliasing/quality/use_taa=true\n')
        base = [str(args.engine.resolve()), "--path", str(project), "--rendering-method", "forward_plus", "--rendering-driver", "vulkan", "--audio-driver", "Dummy"]
        for name, extra in [("import", ["--editor", "--import", "--quit"]), ("run", ["--quit-after", "300"])]:
            with (output / f"{name}.log").open("w") as log:
                result = subprocess.run(base + extra, stdout=log, stderr=subprocess.STDOUT, timeout=180)
            text = (output / f"{name}.log").read_text()
            print(text)
            if result.returncode or any(token in text for token in ["ERROR:", "SCRIPT ERROR:", "BUILTIN_FSR3_TEST FAIL"]):
                raise SystemExit(f"{name} failed; see {output}")
            if name == "run" and "BUILTIN_FSR3_TEST PASS" not in text:
                raise SystemExit("No passing built-in FSR3 result was emitted")


if __name__ == "__main__":
    main()

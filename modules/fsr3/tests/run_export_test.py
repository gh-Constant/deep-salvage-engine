#!/usr/bin/env python3
"""Export the addon-free lab and run it with the built-in Linux release runtime."""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, default=ROOT / "bin/godot.linuxbsd.editor.x86_64")
    parser.add_argument("--template", type=Path, default=ROOT / "bin/godot.linuxbsd.template_release.x86_64")
    args = parser.parse_args()
    output = ROOT / "bin/fsr3-export-test"
    output.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="fsr3-builtin-export-") as temporary:
        project = Path(temporary)
        for source in (ROOT / "misc/fsr3-gdextension/demo").iterdir():
            if source.is_file() and not source.is_symlink():
                shutil.copy2(source, project / source.name)
        (project / "export_presets.cfg").write_text(
            '[preset.0]\nname="Linux Builtin Test"\nplatform="Linux"\nrunnable=true\n'
            'advanced_options=false\ndedicated_server=false\ncustom_features=""\n'
            'export_filter="all_resources"\ninclude_filter=""\nexclude_filter=""\n'
            'export_path=""\n[preset.0.options]\ncustom_template/release="' +
            str(args.template.resolve()) + '"\nbinary_format/architecture="x86_64"\nbinary_format/embed_pck=false\n')
        for name, extra in [("import", ["--editor", "--import", "--quit"]),
                            ("export", ["--export-release", "Linux Builtin Test", str(output / "demo.x86_64")])]:
            with (output / (name + ".log")).open("w") as log:
                result = subprocess.run([str(args.engine.resolve()), "--headless", "--path", str(project)] + extra,
                                        stdout=log, stderr=subprocess.STDOUT, timeout=180)
            text = (output / (name + ".log")).read_text()
            print(name, text[-1200:], flush=True)
            if result.returncode or "ERROR:" in text:
                raise SystemExit(f"{name} failed; see {output}")
        if list(output.glob("*.so")) or list(output.glob("*.dll")):
            raise SystemExit("Export unexpectedly contains native extension libraries")
        with (output / "run.log").open("w") as log:
            result = subprocess.run([str(output / "demo.x86_64"), "--rendering-method", "forward_plus", "--rendering-driver", "vulkan",
                                     "--audio-driver", "Dummy", "--quit-after", "360", "--", "--verify"],
                                    env=dict(os.environ, FSR3_TEST_OUTPUT=str(output)),
                                    stdout=log, stderr=subprocess.STDOUT, timeout=180)
        text = (output / "run.log").read_text()
        print(text[-2400:])
        if result.returncode or "ERROR:" in text or "FSR3_TEST_RESULT PASS" not in text:
            raise SystemExit("Exported game failed; see " + str(output))
        print("Exported release game PASS: built-in FSR3, no addon, no extension .so/.dll.")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Package a native Linux or Windows editor with built-in FSR3."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT.parent.parent


def demo_files():
    for pattern in ['*.gd', '*.gd.uid', '*.tscn', '*.godot', '*.md']:
        for path in sorted((ROOT / 'demo').glob(pattern)):
            if path.is_file():
                yield path


def package_editor(platform, release):
    label = 'Linux' if platform == 'linux' else 'Windows'
    name = 'godot.linuxbsd.editor.x86_64' if platform == 'linux' else 'godot.windows.editor.x86_64.llvm.exe'
    binary = ENGINE / 'bin' / name
    if not binary.is_file():
        raise SystemExit(f'Build the {platform} built-in editor first')
    release.mkdir(parents=True, exist_ok=True)
    output = release / f'Godot-FSR3-Builtin-Editor-{label}-x86_64.zip'
    commit = subprocess.check_output(['git', '-C', str(ENGINE), 'rev-parse', 'HEAD'], text=True).strip()
    if commit.encode() not in binary.read_bytes():
        raise SystemExit(f'{binary.name} does not embed the release commit. Rebuild/relink this editor before packaging.')
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        archive.write(binary, binary.name)
        if platform == 'windows':
            helper = ENGINE / 'bin/godot.windows.editor.x86_64.llvm.console.exe'
            if not helper.is_file():
                raise SystemExit('Build the Windows console helper first')
            archive.write(helper, helper.name)
        for filename in ['LICENSE.txt', 'COPYRIGHT.txt', 'FSR3_EDITOR.md']:
            archive.write(ENGINE / filename, filename)
        archive.write(ENGINE / 'modules/fsr3/README.md', 'FSR3-BUILTIN.md')
        validation = 'Native Linux Vulkan GPU rendering tested on RX 9070 XT / RADV.' if platform == 'linux' else 'Cross-compiled native Windows binaries; native Windows GPU execution untested.'
        archive.writestr('README.txt', f'Godot built-in FSR 3.1.4 editor ({label} x86_64).\nRun {name} and open your project. Select Forward+ and Rendering > Scaling 3D > Mode > FSR 3.1.4.\nNo FSR addon, autoload or AMD runtime DLL is needed. Remove the old addon before using this branch.\nDownload the demo separately. Exporting requires matching built-in export templates.\nNative Linux FSR4 and frame generation are not included.\n{validation}\nSource commit: {commit}\n')
    return [output]


def write_checksums(paths, release):
    checksums = release / 'SHA256SUMS.txt'
    checksums.write_text(''.join(hashlib.sha256(path.read_bytes()).hexdigest() + '  ' + path.name + '\n' for path in paths))
    return checksums


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=['linux', 'windows', 'all'], default='linux')
    parser.add_argument('--output-dir', type=Path, default=ENGINE / 'bin/fsr3-builtin-release')
    args = parser.parse_args()
    paths = []
    for platform in (['linux', 'windows'] if args.platform == 'all' else [args.platform]):
        paths.extend(package_editor(platform, args.output_dir))
    paths.append(write_checksums(paths, args.output_dir))
    for path in paths:
        print(path)


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Package built-in editors, debug/release templates, demo and complete source."""
import argparse
from pathlib import Path
import subprocess
import tempfile
import zipfile
import tarfile
import io
import stat
import json

from package_editor import (DEFAULT_FIDELITYFX_SDK, ROOT, ENGINE, build_info,
                            demo_files, package_editor, write_checksums)


def package_templates(platform, release):
    label = 'Linux' if platform == 'linux' else 'Windows'
    engine_platform = 'linuxbsd' if platform == 'linux' else 'windows'
    suffix = '' if platform == 'linux' else '.llvm.exe'
    commit = subprocess.check_output(['git', '-C', str(ENGINE), 'rev-parse', 'HEAD'], text=True).strip()
    output = release / f'Godot-FSR3-Builtin-Export-Templates-{label}-x86_64.zip'
    metadata = []
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for target in ['template_debug', 'template_release']:
            binary = ENGINE / 'bin' / f'godot.{engine_platform}.{target}.x86_64{suffix}'
            if not binary.is_file() or commit.encode() not in binary.read_bytes():
                raise SystemExit(f'Build {binary.name} at the release commit before packaging')
            archive.write(binary, binary.name)
            metadata.append(build_info(binary, platform, commit))
            if platform == 'windows':
                helper = binary.with_name(binary.name.replace('.exe', '.console.exe'))
                if not helper.is_file():
                    raise SystemExit(f'Missing Windows console helper {helper.name}')
                archive.write(helper, helper.name)
        for filename in ['LICENSE.txt', 'COPYRIGHT.txt']:
            archive.write(ENGINE / filename, filename)
        archive.writestr('BUILD_INFO.json', json.dumps(metadata, indent=2) + '\n')
        archive.writestr('README.txt',
            f'Godot built-in FSR 3.1.6 export templates ({label} x86_64).\n'
            'Extract this ZIP to a permanent directory. In the matching editor, open Export, '
            'add the platform preset, then set Custom Template > Debug and Release to the '
            'template_debug and template_release binaries. Keep Windows console helpers beside their EXEs.\n'
            'This is a per-platform custom-template ZIP, not an installable all-platform TPZ.\n'
            'Forward+/Vulkan and source FSR3 are included. No FSR addon or AMD runtime DLL is required.\n'
            f'Source commit: {commit}\n')
    return [output]


def package_source(release):
    # Archive committed trees, never the working directory or compiler outputs.
    if subprocess.check_output(['git', '-C', str(ENGINE), 'status', '--porcelain'], text=True).strip():
        raise SystemExit('Commit or remove outstanding source changes before packaging a release')
    submodule = ROOT / 'godot-cpp'
    expected = subprocess.check_output(['git', '-C', str(ENGINE), 'rev-parse', 'HEAD:misc/fsr3-gdextension/godot-cpp'], text=True).strip()
    actual = subprocess.check_output(['git', '-C', str(submodule), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != expected or subprocess.check_output(['git', '-C', str(submodule), 'status', '--porcelain'], text=True).strip():
        raise SystemExit('godot-cpp must be clean and match the pinned submodule revision')
    output = release / 'Godot-FSR3-Builtin-Source.zip'
    prefix = 'Godot-FSR3-Builtin-Source/'
    subprocess.run(['git', '-C', str(ENGINE), 'archive', '--format=zip', '--prefix=' + prefix, '--output=' + str(output.resolve()), 'HEAD'], check=True)
    with tempfile.TemporaryDirectory(prefix='godot-fsr3-source-') as temporary:
        module_zip = Path(temporary) / 'godot-cpp.zip'
        subprocess.run(['git', '-C', str(submodule), 'archive', '--format=zip', '--output=' + str(module_zip), expected], check=True)
        with zipfile.ZipFile(output, 'a', zipfile.ZIP_DEFLATED) as archive, zipfile.ZipFile(module_zip) as module:
            for info in module.infolist():
                target = zipfile.ZipInfo(prefix + 'misc/fsr3-gdextension/godot-cpp/' + info.filename, info.date_time)
                target.external_attr = info.external_attr
                target.create_system = info.create_system
                target.compress_type = zipfile.ZIP_DEFLATED
                archive.writestr(target, module.read(info))
            commit = subprocess.check_output(['git', '-C', str(ENGINE), 'rev-parse', 'HEAD'], text=True).strip()
            archive.writestr(prefix + 'SOURCE_COMMIT.txt', commit + '\nIncludes the pinned godot-cpp sources. Skip git submodule update when building this source archive.\n')
    tar_output = output.with_suffix('.tar.gz')
    with zipfile.ZipFile(output) as archive, tarfile.open(tar_output, 'w:gz') as tar:
        for entry in archive.infolist():
            item = tarfile.TarInfo(entry.filename.rstrip('/'))
            item.mode = (entry.external_attr >> 16) & 0o777 or 0o644
            if entry.is_dir():
                item.type = tarfile.DIRTYPE
                tar.addfile(item)
            elif stat.S_ISLNK(entry.external_attr >> 16):
                item.type = tarfile.SYMTYPE
                item.linkname = archive.read(entry).decode()
                tar.addfile(item)
            else:
                data = archive.read(entry)
                item.size = len(data)
                tar.addfile(item, io.BytesIO(data))
    return [output, tar_output]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output-dir', type=Path, default=ENGINE / 'bin/fsr3-builtin-release')
    parser.add_argument('--fidelityfx-sdk', type=Path, default=DEFAULT_FIDELITYFX_SDK,
                        help='AMD FidelityFX SDK 2.4.0 checkout (used for Windows editor package)')
    args = parser.parse_args()
    release = args.output_dir.resolve()
    release.mkdir(parents=True, exist_ok=True)
    paths = package_editor('linux', release)
    paths.extend(package_editor('windows', release, args.fidelityfx_sdk))
    paths.extend(package_templates('linux', release))
    paths.extend(package_templates('windows', release))
    demo = release / 'FSR3-Temporal-Artifact-Lab.zip'
    with zipfile.ZipFile(demo, 'w', zipfile.ZIP_DEFLATED) as archive:
        for path in demo_files():
            archive.write(path, 'FSR3-Temporal-Artifact-Lab/' + path.name)
        archive.writestr('FSR3-Temporal-Artifact-Lab/README.txt', 'Open project.godot with the matching built-in FSR3 custom editor, import, then press F5.\nUses built-in source FSR 3.1.6 through Forward+/Vulkan. No addon or native extension DLL is required.\n')
    paths.append(demo)
    paths.extend(package_source(release))
    paths.append(write_checksums(paths, release))
    for path in paths:
        print(path)


if __name__ == '__main__':
    main()

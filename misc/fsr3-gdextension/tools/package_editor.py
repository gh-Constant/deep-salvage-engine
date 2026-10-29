#!/usr/bin/env python3
"""Package a native Linux or Windows editor with built-in FSR3."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT.parent.parent
FIDELITYFX_SUBMODULE = ENGINE / 'thirdparty/FidelityFX-SDK'
DEFAULT_FIDELITYFX_SDK = FIDELITYFX_SUBMODULE
FIDELITYFX_SDK_VERSION = '2.4.0'
FIDELITYFX_LICENSE_SHA256 = '32357259b965d6a8001e9e508ba60eda6034027c5923a7d226ffbaff813c2bfb'
FIDELITYFX_VULKAN_DLLS = {
    'amd_fidelityfx_loader_vk.dll': '38a56a01fb3565c0757d9f5c1e4e69de4b65e5f2e5769b323b200dc1f142bab9',
    'amd_fidelityfx_upscaler_vk.dll': '4a7e895af6e0b615d40a84d59eb8c2b1054ad91cdd0c739567940270d51bdab7',
}


def build_info(binary, platform, commit):
    info = {'source_commit': commit, 'platform': platform, 'architecture': 'x86_64',
            'fsr': '3.1.6 source, built in', 'binary': binary.name,
            'sha256': hashlib.sha256(binary.read_bytes()).hexdigest()}
    if platform == 'linux':
        versions = subprocess.check_output(['readelf', '--version-info', str(binary)], text=True)
        glibc = re.findall(r'GLIBC_(\d+(?:\.\d+)+)', versions)
        info['minimum_glibc'] = max(glibc, key=lambda version: tuple(map(int, version.split('.'))))
    return info


def demo_files():
    for pattern in ['*.gd', '*.gd.uid', '*.tscn', '*.godot', '*.md']:
        for path in sorted((ROOT / 'demo').glob(pattern)):
            if path.is_file():
                yield path


def fidelityfx_vulkan_runtime(sdk_path):
    signedbin = sdk_path / 'Kits/FidelityFX/signedbin'
    runtime_info = {}
    binaries = []
    for filename, expected_hash in FIDELITYFX_VULKAN_DLLS.items():
        binary = signedbin / filename
        if not binary.is_file():
            raise SystemExit(f'Missing AMD FidelityFX SDK {FIDELITYFX_SDK_VERSION} runtime: {binary}')
        actual_hash = hashlib.sha256(binary.read_bytes()).hexdigest()
        if actual_hash != expected_hash:
            raise SystemExit(f'{binary} does not match the pinned AMD FidelityFX SDK {FIDELITYFX_SDK_VERSION} binary')
        binaries.append((binary, filename))
        runtime_info[filename] = actual_hash

    license_file = sdk_path / 'Kits/FidelityFX/docs/license.md'
    if not license_file.is_file():
        raise SystemExit(f'Missing AMD FidelityFX SDK redistribution license: {license_file}')
    license_hash = hashlib.sha256(license_file.read_bytes()).hexdigest()
    if license_hash != FIDELITYFX_LICENSE_SHA256:
        raise SystemExit(f'{license_file} does not match the pinned AMD FidelityFX SDK {FIDELITYFX_SDK_VERSION} license')
    return binaries, license_file, {
        'version': FIDELITYFX_SDK_VERSION,
        'license_sha256': license_hash,
        'binaries': runtime_info,
    }


def add_fidelityfx_vulkan_runtime(archive, validated_runtime):
    binaries, license_file, runtime_info = validated_runtime
    for binary, filename in binaries:
        archive.write(binary, filename)
    archive.write(license_file, 'AMD-FSR-SDK-LICENSE.md')
    return runtime_info


def package_editor(platform, release, fidelityfx_sdk=DEFAULT_FIDELITYFX_SDK):
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
    info = build_info(binary, platform, commit)
    validated_runtime = fidelityfx_vulkan_runtime(fidelityfx_sdk) if platform == 'windows' else None
    if platform == 'linux':
        probe = subprocess.run([str(binary), '--display-driver', 'fsr3-backend-check'],
                               capture_output=True, text=True)
        available = probe.stdout.lower() + probe.stderr.lower()
        if 'x11' not in available or 'wayland' not in available:
            raise SystemExit('The release Linux editor must include both X11 and Wayland')
        info['display_backends'] = ['x11', 'wayland', 'headless']
    with zipfile.ZipFile(output, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        archive.write(binary, binary.name)
        if platform == 'windows':
            info['amd_fidelityfx_vulkan_runtime'] = add_fidelityfx_vulkan_runtime(
                archive, validated_runtime)
        if platform == 'windows':
            helper = ENGINE / 'bin/godot.windows.editor.x86_64.llvm.console.exe'
            if not helper.is_file():
                raise SystemExit('Build the Windows console helper first')
            archive.write(helper, helper.name)
        for filename in ['LICENSE.txt', 'COPYRIGHT.txt', 'FSR3_EDITOR.md']:
            archive.write(ENGINE / filename, filename)
        archive.write(ENGINE / 'modules/fsr3/README.md', 'FSR3-BUILTIN.md')
        archive.writestr('BUILD_INFO.json', json.dumps(info, indent=2) + '\n')
        if platform == 'windows':
            runtime_note = (
                'AMD FidelityFX SDK 2.4.0 Vulkan upscaler DLLs are bundled beside the editor. '
                'To use them, select the External AMD runtime backend under Rendering > Scaling 3D > FSR3; '
                'the default remains built-in source FSR 3.1.6. A supported AMD GPU/driver and Microsoft '
                'Visual C++ runtime are required. For exported games, copy both DLLs and '
                'AMD-FSR-SDK-LICENSE.md beside the exported executable yourself.\n'
                'Windows runtime rendering was tested under Wine on an RX 9070 XT/RADV; native '
                'Windows GPU execution is not yet validated.\n')
        else:
            runtime_note = (
                'No AMD runtime DLL is bundled. Native Linux FSR4 is not included; '
                'the Windows AMD runtime cannot be loaded by native Linux.\n')
        validation = 'Native Linux Vulkan GPU rendering tested on RX 9070 XT / RADV.' if platform == 'linux' else 'Cross-compiled native Windows binaries.'
        archive.writestr('README.txt', f'Godot built-in FSR 3.1.6 editor ({label} x86_64).\nRun {name} and open your project. Select Forward+ and Rendering > Scaling 3D > Mode > FSR 3.1.6.\nNo FSR addon or autoload is needed. Remove the old addon before using this branch.\nDownload the demo separately. Exporting requires matching built-in export templates.\n{runtime_note}{validation}\nSource commit: {commit}\n')
    return [output]


def write_checksums(paths, release):
    checksums = release / 'SHA256SUMS.txt'
    checksums.write_text(''.join(hashlib.sha256(path.read_bytes()).hexdigest() + '  ' + path.name + '\n' for path in paths))
    return checksums


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--platform', choices=['linux', 'windows', 'all'], default='linux')
    parser.add_argument('--output-dir', type=Path, default=ENGINE / 'bin/fsr3-builtin-release')
    parser.add_argument('--fidelityfx-sdk', type=Path, default=DEFAULT_FIDELITYFX_SDK,
                        help='AMD FidelityFX SDK 2.4.0 checkout (used for Windows packages)')
    args = parser.parse_args()
    paths = []
    for platform in (['linux', 'windows'] if args.platform == 'all' else [args.platform]):
        paths.extend(package_editor(platform, args.output_dir, args.fidelityfx_sdk))
    paths.append(write_checksums(paths, args.output_dir))
    for path in paths:
        print(path)


if __name__ == '__main__':
    main()

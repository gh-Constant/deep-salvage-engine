#!/usr/bin/env python3
"""Package the built addon; never include the engine or development symlinks."""
from pathlib import Path
import argparse
import zipfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--platform', choices=['linux','windows','all'], default='linux')
parser.add_argument('--output', type=Path)
args = parser.parse_args()
if args.output is None:
    label = 'desktop' if args.platform == 'all' else args.platform
    args.output = root / f'build/fsr3-{label}-x86_64.zip'
for platform in (['linux','windows'] if args.platform == 'all' else [args.platform]):
    extension = 'so' if platform == 'linux' else 'dll'
    for target in ['debug','release']:
        path = root / f'addon/addons/fsr3/bin/libfsr3.{platform}.template_{target}.x86_64.{extension}'
        if not path.is_file(): raise SystemExit(f'Build the extension first: {path}')
args.output.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(args.output, 'w', zipfile.ZIP_DEFLATED) as archive:
    archive.writestr('addons/fsr3/licenses/.gdignore', '')
    for path in sorted((root / 'addon').rglob('*')):
        if path.is_file() and '.godot' not in path.parts:
            if 'bin' in path.parts:
                if 'amd' in path.parts and args.platform == 'linux': continue
                if args.platform == 'linux' and '.windows.' in path.name: continue
                if args.platform == 'windows' and '.linux.' in path.name: continue
            archive.write(path, path.relative_to(root / 'addon'))
    for name in ['README.md', 'LICENSE-Godot.txt', 'LICENSE-Demo.txt', 'THIRD_PARTY_NOTICES.md']:
        path = root / name
        if path.is_file(): archive.write(path, f'addons/fsr3/licenses/{name}')
    archive.write(root/'thirdparty/vulkan-headers/LICENSE.md', 'addons/fsr3/licenses/LICENSE-Vulkan-Headers.md')
    for vendor in ['amd-ffx', 'amd-fidelityfx-api']:
      for path in (root / 'thirdparty' / vendor).rglob('*'):
        if path.is_file() and ('license' in path.name.lower() or path.suffix in ['.cpp','.h','.glsl']):
            # Preserve all vendored notices in the source bundle shipped with it.
            archive.write(path, Path('addons/fsr3/licenses/source') / path.relative_to(root))
print(args.output)

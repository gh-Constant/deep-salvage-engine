#!/usr/bin/env python3
"""Package the tested native Linux editor and demo as a GitHub release asset."""
import hashlib
from pathlib import Path
import subprocess
import zipfile

root = Path(__file__).resolve().parents[1]
engine = root.parent.parent
binary = engine / 'bin/godot.linuxbsd.editor.x86_64'
addon = root / 'build/fsr3-linux-x86_64.zip'
if not binary.is_file() or not addon.is_file():
    raise SystemExit('Build/test the editor and Linux addon ZIP first')
release = engine / 'bin/fsr3-release'
release.mkdir(parents=True, exist_ok=True)
archive_path = release / 'Godot-FSR3-Custom-Editor-Linux-x86_64.zip'
with zipfile.ZipFile(archive_path, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
    archive.write(binary, binary.name)
    for name in ['LICENSE.txt', 'COPYRIGHT.txt', 'FSR3_EDITOR.md']:
        archive.write(engine / name, name)
    for pattern in ['*.gd', '*.gd.uid', '*.tscn', '*.godot', '*.md']:
        for path in (root / 'demo').glob(pattern):
            archive.write(path, 'demo/' + path.name)
    with zipfile.ZipFile(addon) as source:
        for entry in source.infolist():
            archive.writestr('demo/' + entry.filename, source.read(entry))
    launcher = zipfile.ZipInfo('Open-FSR3-Lab.sh')
    launcher.create_system = 3
    launcher.external_attr = 0o100755 << 16
    archive.writestr(launcher, '#!/usr/bin/env bash\nset -euo pipefail\nrelease_dir=$(cd -- "$(dirname -- "$0")" && pwd)\nexec "$release_dir/godot.linuxbsd.editor.x86_64" --editor --path "$release_dir/demo" --rendering-method forward_plus --rendering-driver vulkan "$@"\n')
    commit = subprocess.check_output(['git', '-C', str(engine), 'rev-parse', 'HEAD'], text=True).strip()
    archive.writestr('README.txt', 'Experimental Godot custom FSR3 editor (Linux x86_64).\nExtract the ZIP and run ./Open-FSR3-Lab.sh, let the project import, then press F5.\nThe demo uses the included FSR 3.1.4 GDExtension through Forward+/Vulkan.\nUse this editor with another project by installing the separate Linux addon ZIP.\nFSR3 is supplied by the addon, not enabled globally in every project.\nNative Linux FSR4 and frame generation are not included.\nSource commit: ' + commit + '\n')
addon_copy = release / addon.name
addon_copy.write_bytes(addon.read_bytes())
checksums = release / 'SHA256SUMS.txt'
checksums.write_text(''.join(hashlib.sha256(path.read_bytes()).hexdigest() + '  ' + path.name + '\n' for path in [archive_path, addon_copy]))
print(archive_path)
print(addon_copy)
print(checksums)

#!/usr/bin/env python3
"""Try the real SDK DLLs on native Linux and verify an explicit source fallback.

This does not use Wine or emulate Windows. A passing test means the real DLLs
were rejected as Windows binaries and Godot continued with source FSR3, not FSR4.
"""
import argparse
import ctypes
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sdk', type=Path, default=ROOT / 'thirdparty/FidelityFX-SDK')
    parser.add_argument('--engine', type=Path, default=ROOT / 'bin/godot.linuxbsd.editor.x86_64')
    args = parser.parse_args()
    if not sys.platform.startswith('linux'):
        raise SystemExit('This probe specifically tests native Linux without a Windows runtime.')
    output = ROOT / 'bin/fsr316-validation/sdk24-native'
    output.mkdir(parents=True, exist_ok=True)
    binaries = args.sdk.resolve() / 'Kits/FidelityFX/signedbin'
    results = []
    for name in ['amd_fidelityfx_loader_vk.dll', 'amd_fidelityfx_upscaler_vk.dll']:
        library = binaries / name
        if library.read_bytes()[:2] != b'MZ':
            raise SystemExit(f'Expected an actual SDK PE binary: {library}')
        try:
            ctypes.CDLL(str(library))
        except OSError as error:
            results.append({'file': name, 'native_load': str(error)})
        else:
            raise SystemExit(f'Unexpectedly loaded {name}; investigate before claiming FSR4.')
    script = '''extends Node3D
var frame: int = 0
var upscaler: FidelityFXUpscaler
func _ready() -> void:
    var camera: Camera3D = Camera3D.new()
    camera.position.z = 4.0
    add_child(camera)
    var mesh: MeshInstance3D = MeshInstance3D.new()
    mesh.mesh = SphereMesh.new()
    add_child(mesh)
    get_window().scaling_3d_mode = Viewport.SCALING_3D_MODE_FSR3
    get_window().scaling_3d_scale = 0.67
    get_window().fsr3_library_path = SDK_LIBRARY
    get_window().fsr3_backend = 1
    upscaler = get_window().get_fsr3_upscaler()
func _process(_delta: float) -> void:
    frame += 1
    if frame == 90:
        assert(upscaler.get_active_backend() == "Source FSR3")
        assert(upscaler.get_active_version() == "3.1.6")
        assert(upscaler.get_binary_error().contains("Windows PE DLL"))
        assert(upscaler.get_binary_dispatch_count() == 0)
        assert(upscaler.get_gpu_dispatch_count() > 0)
        assert(upscaler.get_failure_count() == 0)
        print("SDK24_NATIVE_LINUX_PROBE PASS: real AMD DLL rejected; source 3.1.6 GPU fallback active; FSR4 dispatches=0")
        get_tree().quit()
'''.replace('SDK_LIBRARY', json.dumps(str(binaries / 'amd_fidelityfx_loader_vk.dll'))).replace('    ', '\t')
    with tempfile.TemporaryDirectory(prefix='fsr-sdk24-native-') as temporary:
        project = Path(temporary)
        (project / 'probe.gd').write_text(script)
        (project / 'probe.tscn').write_text('[gd_scene load_steps=2 format=3]\n[ext_resource type="Script" path="res://probe.gd" id="1"]\n[node name="Probe" type="Node3D"]\nscript=ExtResource("1")\n')
        (project / 'project.godot').write_text('config_version=5\n[application]\nconfig/name="Native SDK 2.4 probe"\nrun/main_scene="res://probe.tscn"\nconfig/features=PackedStringArray("4.8")\n[display]\nwindow/size/viewport_width=640\nwindow/size/viewport_height=360\n')
        base = [str(args.engine.resolve()), '--path', str(project), '--rendering-driver', 'vulkan', '--rendering-method', 'forward_plus', '--audio-driver', 'Dummy']
        for phase, flags in [('import', ['--editor', '--import', '--quit']), ('run', ['--quit-after', '130'])]:
            log = output / (phase + '.log')
            with log.open('w') as stream:
                process = subprocess.run(base + (['--headless'] if phase == 'import' else []) + flags, stdout=stream, stderr=subprocess.STDOUT, timeout=180)
            text = log.read_text()
            if process.returncode or 'ERROR:' in text or (phase == 'run' and 'SDK24_NATIVE_LINUX_PROBE PASS' not in text):
                print(text)
                raise SystemExit(f'{phase} failed; see {log}')
            print(text[-1500:])
    (output / 'native-load-results.json').write_text(json.dumps(results, indent=2) + '\n')
    print('Native Linux FSR4 is not running: actual SDK DLL loading failed. Source FSR3.1.6 works.')


if __name__ == '__main__':
    main()

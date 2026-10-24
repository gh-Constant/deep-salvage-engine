#!/usr/bin/env python3
"""Run the real Forward+/Vulkan integration test; headless rendering is insufficient."""
from pathlib import Path
import argparse, os, subprocess, sys, shutil

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--engine', type=Path, required=True)
parser.add_argument('--simple', action='store_true')
parser.add_argument('--scene', default='res://artifact_lab.tscn', choices=['res://artifact_lab.tscn', 'res://main.tscn'])
parser.add_argument('--library', choices=['debug','release'], default='debug')
args = parser.parse_args()
engine = args.engine.resolve()
scene_name = 'artifact-lab' if args.scene.endswith('artifact_lab.tscn') else 'fsr3'
if args.simple:
    scene_name = 'artifact-lab-simple' if scene_name == 'artifact-lab' else 'simple'
output = root / 'tests/output' / (scene_name + ('-release' if args.library == 'release' else ''))
output.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, FSR3_TEST_OUTPUT=str(output))
project = root/'demo'
if args.library == 'release':
    project = root/'build/release-demo'
    project.mkdir(parents=True, exist_ok=True)
    for name in ['main.gd','main.tscn','artifact_lab.gd','artifact_lab.tscn','project.godot']:
        shutil.copy2(root/'demo'/name, project/name)
    shutil.copytree(root/'addon/addons', project/'addons', dirs_exist_ok=True)
    manifest = project/'addons/fsr3/fsr3.gdextension'
    manifest.write_text(manifest.read_text().replace('bin/libfsr3.linux.template_debug.x86_64.so', 'bin/libfsr3.linux.template_release.x86_64.so'))
base = [str(engine), '--path', str(project), '--rendering-method', 'forward_plus', '--rendering-driver', 'vulkan', '--audio-driver', 'Dummy']
for name, command in [('import', base+['--editor','--import','--quit']), ('run', base+[args.scene,'--quit-after','360','--','--verify']+(['--simple'] if args.simple else []))]:
    with (output/f'{name}.log').open('w') as log:
        result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, env=env, timeout=180)
    text = (output/f'{name}.log').read_text()
    print(text)
    if result.returncode or any(token in text for token in ['SCRIPT ERROR:', 'ERROR:', 'FSR3_TEST_RESULT FAIL']):
        raise SystemExit(f'{name} failed (code {result.returncode}); see {output}')
    if name == 'run' and 'FSR3_TEST_RESULT PASS' not in text:
        raise SystemExit('No passing GPU test result was emitted')
print(f'GPU integration passed; captures: {output}')

#!/usr/bin/env python3
"""Check plugin/autoload installation without manually setting an upscaler."""
from pathlib import Path
import argparse, subprocess, shutil, zipfile
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser()
parser.add_argument('--engine',type=Path,required=True)
parser.add_argument('--archive',type=Path)
args=parser.parse_args()
out=root/('build/addon-archive-install-test' if args.archive else 'build/addon-install-test')
out.mkdir(parents=True,exist_ok=True)
if args.archive:
 with zipfile.ZipFile(args.archive) as archive: archive.extractall(out)
else:
 shutil.copytree(root/'addon/addons',out/'addons',dirs_exist_ok=True)
(out/'project.godot').write_text((root/'demo/project.godot').read_text().replace('res://artifact_lab.tscn', 'res://main.tscn')+'''\n[editor_plugins]\nenabled=PackedStringArray("res://addons/fsr3/plugin.cfg")\n''')
shutil.copy2(root/'demo/main.tscn',out/'main.tscn')
s=(root/'demo/main.gd').read_text()
s=s.replace('var fsr: FSR3Upscaler','var fsr: FSR3Upscaler').replace('\tfsr = FSR3Upscaler.new()\n\tsimple = SimpleComputeUpscaler.new()\n','\tfsr = get_viewport().scaling_3d_custom_upscaler\n')
s=s.replace('\tselect_upscaler(simple if basic_only else fsr, 0.67)\n','')
a=s.index('func _process(')
s=s[:a]+'''func _process(_delta: float) -> void:
\tframe += 1
\tif frame == 60:
\t\tvar passed := fsr != null and fsr.get_dispatch_count() > 0 and fsr.get_failure_count() == 0
\t\tprint("FSR3_ADDON_INSTALL ", "PASS" if passed else "FAIL")
\t\tget_tree().quit(0 if passed else 1)
'''
(out/'main.gd').write_text(s)
logs=root/('tests/output/addon-archive-install' if args.archive else 'tests/output/addon-install')
logs.mkdir(parents=True,exist_ok=True)
base=[str(args.engine.resolve()),'--path',str(out),'--rendering-method','forward_plus','--rendering-driver','vulkan','--audio-driver','Dummy']
for name,command in [('import',base+['--editor','--import','--quit']),('enable',base+['--editor','--quit-after','20']),('run',base+['--quit-after','90'])]:
 with (logs/f'{name}.log').open('w') as log: result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,timeout=120)
 text=(logs/f'{name}.log').read_text();print(text)
 if result.returncode or 'ERROR:' in text:raise SystemExit(f'{name} failed')
 if name=='run' and 'FSR3_ADDON_INSTALL PASS' not in text:raise SystemExit('Addon did not configure the viewport')

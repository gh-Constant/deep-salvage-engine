#!/usr/bin/env python3
from pathlib import Path
import argparse, os, subprocess
root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--engine', type=Path, required=True)
args = parser.parse_args()
(root/'api').mkdir(exist_ok=True)
(root/'build').mkdir(exist_ok=True)
# Explicit log path makes dumping work in sandboxed environments, too.
subprocess.run([str(args.engine.resolve()), '--headless', '--log-file', str(root/'build/api-dump.log'), '--dump-extension-api'], cwd=root/'api', check=True)
print(root/'api/extension_api.json')

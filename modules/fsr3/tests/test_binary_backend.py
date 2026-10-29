#!/usr/bin/env python3
"""Native Vulkan bridge test with an explicitly synthetic provider, never an FSR claim."""
from pathlib import Path
import argparse,os,shutil,subprocess
root=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--engine',type=Path,required=True);args=p.parse_args()
out=root.parents[1]/'bin/fsr3-native-binary-test';out.mkdir(parents=True,exist_ok=True)
logs=root.parents[1]/'bin/fsr3-native-binary-logs';logs.mkdir(parents=True,exist_ok=True)
for name in ['artifact_lab.gd','main.gd','artifact_lab.tscn','main.tscn','project.godot']:shutil.copy2(root.parents[1]/'misc/fsr3-gdextension/demo'/name,out/name)
# Built-in classes are provided by the engine. Never install a GDExtension here.
(out/'project.godot').write_text((out/'project.godot').read_text().replace('res://artifact_lab.tscn','res://native_test.tscn'))
(out/'native_test.tscn').write_text('[gd_scene load_steps=2 format=3]\n[ext_resource type="Script" path="res://native_test.gd" id="1"]\n[node name="NativeBridgeTest" type="Node3D"]\nscript=ExtResource("1")\n')
(out/'native_test.gd').write_text('''extends "res://artifact_lab.gd"
var binary_resource: FidelityFXUpscaler
var checked := 0
func _ready() -> void:
    super._ready()
    RenderingServer.call_on_render_thread(_write_shader)
    binary_resource = FidelityFXUpscaler.new()
    binary_resource.library_path = OS.get_environment("FSR3_TEST_BINARY")
    binary_resource.allow_source_fallback = false
    binary_resource.preferred_version = "TEST_GREEN_COMPUTE_ONLY"
    fsr = binary_resource
    select_upscaler(fsr, 0.67)
func _write_shader() -> void:
    var source := RDShaderSource.new()
    source.source_compute = """#version 450
layout(local_size_x=8,local_size_y=8) in;
layout(rgba16f,set=0,binding=0) uniform writeonly image2D target;
void main(){ivec2 p=ivec2(gl_GlobalInvocationID.xy);if(all(lessThan(p,imageSize(target))))imageStore(target,p,vec4(0,1,0,1));}
"""
    var spirv := RenderingServer.get_rendering_device().shader_compile_spirv_from_source(source)
    var file := FileAccess.open(OS.get_environment("FSR3_MOCK_SPIRV"),FileAccess.WRITE)
    file.store_buffer(spirv.get_stage_bytecode(RenderingDevice.SHADER_STAGE_COMPUTE))
func _process(delta: float) -> void:
    super._process(delta)
    if frame == 45 or frame == 75:
        check_green()
    if frame == 60:
        get_window().size = Vector2i(1001,563)
    if frame == 80:
        binary_resource.preferred_version = ""
    if frame == 85:
        check_green()
    if frame == 90:
        binary_resource.allow_source_fallback = true
        binary_resource.preferred_version = "UNAVAILABLE_PROVIDER"
    if frame == 125:
        var passed := checked == 3 and binary_resource.get_binary_dispatch_count() >= 80 and binary_resource.get_binary_failure_count() == 0 and binary_resource.get_active_backend() == "Source FSR3"
        print("NATIVE_BINARY_TEST ", "PASS" if passed else "FAIL", " native_dispatches=", binary_resource.get_binary_dispatch_count(), " active=", binary_resource.get_active_backend())
        get_tree().quit(0 if passed else 1)
func check_green() -> void:
    await RenderingServer.frame_post_draw
    var image := get_viewport().get_texture().get_image()
    var pixel := image.get_pixel(image.get_width()/2, image.get_height()*3/4)
    if pixel.g > 0.7 and pixel.r < 0.2 and pixel.b < 0.2:
        checked += 1
    else:
        push_error("Native compute output did not reach post-processing: " + str(pixel))
    image.save_png(OS.get_environment("FSR3_TEST_OUTPUT").path_join("native_%d.png" % frame))
'''.replace('    ','\t'))
library=root.parents[1]/'bin/mock-fidelityfx.so'
subprocess.run(['g++','-std=c++17','-shared','-fPIC','-I',str(root),'-I',str(root.parents[1]),'-I',str(root.parents[1]/'thirdparty/vulkan'),str(root/'tests/mock_fidelityfx.cpp'),'-o',str(library),'-lvulkan'],check=True)
trace=logs/'lifetime.log';trace.unlink(missing_ok=True)
env=dict(os.environ,FSR3_TEST_BINARY=str(library),FSR3_MOCK_TRACE=str(trace),FSR3_MOCK_SPIRV=str(out/'green.spv'),FSR3_TEST_OUTPUT=str(logs))
base=[str(args.engine.resolve()),'--path',str(out),'--rendering-method','forward_plus','--rendering-driver','vulkan','--audio-driver','Dummy']
for name,command in [('import',base+['--editor','--import','--quit']),('run',base+['--quit-after','160'])]:
 with (logs/f'{name}.log').open('w') as f:r=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT,env=env,timeout=180)
 text=(logs/f'{name}.log').read_text();print(text)
 if r.returncode or 'ERROR:' in text:raise SystemExit(f'{name} failed')
 if name=='run' and 'NATIVE_BINARY_TEST PASS' not in text:raise SystemExit('No passing native bridge result')
events=trace.read_text().splitlines()
assert events.count('CREATE')==events.count('DESTROY') and events.count('CREATE')>=2,events
assert events.count('DISPATCH')>=80,events
assert 'AUTO' in events and 'PINNED' in events,events
print('Native Vulkan command dispatch, post-processing output, resize, explicit provider selection, source fallback and deferred context lifetime passed. This provider is test-only; AMD Windows DLL execution is untested here.')

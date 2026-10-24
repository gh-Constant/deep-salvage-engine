#pragma once
#include <godot_cpp/classes/rendering_device.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/rd_texture_format.hpp>
#include <godot_cpp/classes/rd_texture_view.hpp>
#include <godot_cpp/classes/rd_sampler_state.hpp>
#include <godot_cpp/classes/rd_uniform.hpp>
#include <godot_cpp/classes/rd_shader_source.hpp>
#include <godot_cpp/classes/rd_shader_spirv.hpp>
#include <godot_cpp/classes/uniform_set_cache_rd.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cstring>
#include <cmath>
using namespace godot;
using RD = godot::RenderingDevice;

inline RenderingDevice *render_device() { return RenderingServer::get_singleton()->get_rendering_device(); }
inline Ref<RDUniform> make_uniform(RD::UniformType type, int binding, RID rid) {
    Ref<RDUniform> u; u.instantiate(); u->set_uniform_type(type); u->set_binding(binding); u->add_id(rid); return u;
}
inline RID compile_compute(const char *code, const String &defines = String()) {
    String source(code);
    int64_t newline = source.find("\n", source.find("#version"));
    source = source.substr(0, newline + 1) + defines + source.substr(newline + 1);
    Ref<RDShaderSource> shader_source; shader_source.instantiate();
    shader_source->set_stage_source(RD::SHADER_STAGE_COMPUTE, source);
    Ref<RDShaderSPIRV> spirv = render_device()->shader_compile_spirv_from_source(shader_source);
    String error = spirv->get_stage_compile_error(RD::SHADER_STAGE_COMPUTE);
    if (!error.is_empty()) { ERR_PRINT(error); return RID(); }
    return render_device()->shader_create_from_spirv(spirv);
}

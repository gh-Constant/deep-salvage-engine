/**************************************************************************/
/*  rd_adapter.h                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/error/error_macros.h"
#include "core/math/projection.h"
#include "core/object/callable_mp.h"
#include "core/templates/local_vector.h"
#include "core/variant/typed_array.h"
#include "servers/rendering/renderer_rd/uniform_set_cache_rd.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"

#include <cmath>
#include <cstring>

inline RenderingDevice *render_device() {
	return RenderingDevice::get_singleton();
}

inline RD::Uniform make_uniform(RD::UniformType p_type, int p_binding, RID p_rid) {
	RD::Uniform uniform;
	uniform.uniform_type = p_type;
	uniform.binding = p_binding;
	uniform.append_id(p_rid);
	return uniform;
}

inline RID compile_compute(const char *p_code, const String &p_defines = String()) {
	String source(p_code);
	int64_t newline = source.find("\n", source.find("#version"));
	source = source.substr(0, newline + 1) + p_defines + source.substr(newline + 1);
	String error;
	RD::ShaderStageSPIRVData stage;
	stage.shader_stage = RD::SHADER_STAGE_COMPUTE;
	stage.spirv = render_device()->shader_compile_spirv_from_source(RD::SHADER_STAGE_COMPUTE, source, RD::SHADER_LANGUAGE_GLSL, &error);
	ERR_FAIL_COND_V_MSG(!error.is_empty(), RID(), error);
	Vector<RD::ShaderStageSPIRVData> stages;
	stages.push_back(stage);
	return render_device()->shader_create_from_spirv(stages);
}

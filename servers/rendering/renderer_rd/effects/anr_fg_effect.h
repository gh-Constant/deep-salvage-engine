/**************************************************************************/
/*  anr_fg_effect.h                                                       */
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

#include "drivers/anr_fg/anr_frame_generation.h"
#include "servers/display/display_server_enums.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/renderer_rd/shaders/effects/motion_vector_decode.glsl.gen.h"

class RenderDataRD;

namespace RendererRD {

// ANR FG frame generation for the viewport drawn to a window: the 3D renderer hands over its depth,
// motion vectors (any temporal upscaler, or TAA, or none) and camera once per frame
// (capture_scene), and the window blit runs the provider on the final image and asks the Vulkan
// driver to present the generated frame first (process_screen_blit).
class AnrFgEffect {
	static AnrFgEffect *singleton;

	MotionVectorDecodeShaderRD decode_shader;
	RID decode_version;
	RID decode_pipeline;

	struct Scene {
		RID render_target;
		RID depth;
		RID motion;
		Size2i internal_size;
		uint64_t frame = UINT64_MAX;
		AnrFrameGeneration::EvaluateParams params; // Camera data; images filled at blit time.
	} scene;

	RID output_interpolated;
	RID output_real; // Only with GODOT_ANR_FG_OUTPUT_REAL=1: nothing reads it.
	Size2i output_size;
	RD::DataFormat output_format = RD::DATA_FORMAT_MAX;

	// The generated frame after the window's blit (the swap chain's size and format): what the
	// driver copies into the swap chain image it presents first.
	RID screen_texture;
	RID screen_framebuffer;
	Size2i screen_size;
	RD::DataFormat screen_format = RD::DATA_FORMAT_MAX;

	// Read by the driver callback when the frame's graph is recorded.
	struct Pending {
		AnrFrameGeneration::EvaluateParams params;
		RDD::SwapChainID swap_chain;
		uint64_t output_image = 0;
		uint32_t output_format = 0;
		Size2i output_size;
		Rect2i dst_rect;
	} pending;

	RID last_render_target;
	uint64_t last_generated_frame = UINT64_MAX;
	bool provider_unavailable_reported = false;

	void _free_outputs();
	void _free_screen_target();
	bool _ensure_screen_target(const Size2i &p_size, RD::DataFormat p_format);
	static AnrFrameGeneration::Image _image(RID p_texture, bool p_depth);
	static void _evaluate_callback(RenderingDeviceDriver *p_driver, RenderingDeviceDriver::CommandBufferID p_command_buffer, void *p_userdata);
	static void _layout_callback(RenderingDeviceDriver *p_driver, RenderingDeviceDriver::CommandBufferID p_command_buffer, void *p_userdata);

public:
	static AnrFgEffect *get_singleton() { return singleton; }
	static AnrFgEffect *get_or_create();
	static void free_singleton();

	// 3D renderer, after the upscaler: makes the motion vectors valid everywhere (the sky has none)
	// and keeps this frame's inputs for the window blit.
	void capture_scene(const RenderDataRD *p_render_data);
	// Window blit, once per frame before anything else: releases what the GPU is done with.
	void process_frame();
	// Window blit, before the blit's draw list: runs the provider on p_render_target's final image
	// when its 3D scene was captured this frame. Returns the generated frame (the render target's
	// size and format) for the compositor to blit into get_generated_screen_framebuffer() like the
	// real one, then end_generated_screen_blit(); an invalid RID when no frame is generated.
	// p_present false (tests, GODOT_ANR_FG_OFFSCREEN): generate without presenting.
	RID process_screen_blit(DisplayServerEnums::WindowID p_screen, RID p_render_target, bool p_present = true);
	RID get_generated_screen_framebuffer() const { return screen_framebuffer; }
	void end_generated_screen_blit();

	AnrFgEffect();
	~AnrFgEffect();
};

} // namespace RendererRD

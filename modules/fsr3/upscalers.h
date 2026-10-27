/**************************************************************************/
/*  upscalers.h                                                 */
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
#include "fsr3_backend.h"
#include "rd_adapter.h"

#include "scene/resources/viewport_upscaler.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/storage/render_data.h"
#include "servers/rendering/storage/render_scene_data.h"

#include <atomic>
#include <map>
#include <memory>

class SimpleComputeUpscaler : public ViewportUpscaler {
	GDCLASS(SimpleComputeUpscaler, ViewportUpscaler)
	RID shader;
	RID pipeline;
	std::atomic<int64_t> dispatch_count{ 0 };

protected:
	static void _bind_methods();

public:
	void render_callback(const RenderData *data) override;
	int64_t get_dispatch_count() const { return dispatch_count.load(); }
	~SimpleComputeUpscaler();
};

class FSR3Upscaler : public ViewportUpscaler {
	GDCLASS(FSR3Upscaler, ViewportUpscaler)
	struct ViewState {
		std::unique_ptr<RendererRD::FSR3UpscalerContext> context;
		Vector2i internal_size;
		Vector2i target_size;
		uint64_t reset_serial = 0;
		uint64_t last_frame = 0;
	};
	// Each RenderSceneBuffersRD and eye needs separate temporal history.
	std::map<std::pair<uint64_t, uint32_t>, ViewState> views;
	std::unique_ptr<RendererRD::FSR3UpscalerEffect> backend;
	std::atomic<float> sharpness{ 0.2f };
	std::atomic<uint64_t> reset_serial{ 1 };
	std::atomic<int64_t> dispatch_count{ 0 };
	std::atomic<int64_t> context_count{ 0 };
	std::atomic<int64_t> gpu_dispatch_count{ 0 };
	std::atomic<int64_t> failure_count{ 0 };
	bool missing_api_reported = false;

protected:
	static void _bind_methods();

public:
	FSR3Upscaler();
	~FSR3Upscaler();
	void render_callback(const RenderData *data) override;
	void set_sharpness(float value) { sharpness.store(CLAMP(value, 0.0f, 1.0f)); }
	float get_sharpness() const { return sharpness.load(); }
	virtual void reset_history() { reset_serial.fetch_add(1); }
	void configure_scale(double scale);
	virtual int64_t get_dispatch_count() const { return dispatch_count.load(); }
	int64_t get_gpu_dispatch_count() const { return gpu_dispatch_count.load(); }
	int64_t get_context_count() const { return context_count.load(); }
	virtual int64_t get_failure_count() const { return failure_count.load(); }
};

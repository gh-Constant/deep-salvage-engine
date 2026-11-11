/**************************************************************************/
/*  metal_fx.cpp                                                          */
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

#ifdef METAL_ENABLED

#include "metal_fx.h"

#include "drivers/metal/pixel_formats.h"
#include "drivers/metal/rendering_device_driver_metal3.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"

#include <MetalFX/MetalFX.hpp>

using namespace RendererRD;

#pragma mark - Spatial Scaler

MFXSpatialContext::~MFXSpatialContext() {
	if (scaler) {
		scaler->release();
	}
}

MFXSpatialEffect::MFXSpatialEffect() {
}

MFXSpatialEffect::~MFXSpatialEffect() {
}

void MFXSpatialEffect::callback(RDD *p_driver, RDD::CommandBufferID p_command_buffer, CallbackArgs *p_userdata) {
	MDCommandBufferBase *obj = (MDCommandBufferBase *)(p_command_buffer.id);
	obj->end();

	MTL::Texture *src_texture = reinterpret_cast<MTL::Texture *>(p_userdata->src.id);
	MTL::Texture *dst_texture = reinterpret_cast<MTL::Texture *>(p_userdata->dst.id);

	MTLFX::SpatialScalerBase *scaler = p_userdata->scaler;
	scaler->setFence(obj->external_pass_fence());
	scaler->setColorTexture(src_texture);
	scaler->setOutputTexture(dst_texture);
	MTLFX::SpatialScaler *s = static_cast<MTLFX::SpatialScaler *>(scaler);
	MTL3::MDCommandBuffer *cmd = (MTL3::MDCommandBuffer *)(p_command_buffer.id);
	s->encodeToCommandBuffer(cmd->get_command_buffer());
	obj->retain_resource(scaler);

	CallbackArgs::free(&p_userdata);
}

void MFXSpatialEffect::ensure_context(Ref<RenderSceneBuffersRD> p_render_buffers) {
	p_render_buffers->ensure_mfx(this);
}

void MFXSpatialEffect::process(Ref<RenderSceneBuffersRD> p_render_buffers, RID p_src, RID p_dst) {
	MFXSpatialContext *ctx = p_render_buffers->get_mfx_spatial_context();
	DEV_ASSERT(ctx); // this should have been done by the caller via ensure_context

	CallbackArgs *userdata = args_allocator.alloc(
			this,
			RDD::TextureID(RD::get_singleton()->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_src)),
			RDD::TextureID(RD::get_singleton()->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_dst)),
			*ctx);
	RD::CallbackResource res[2] = {
		{ .rid = p_src, .usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE },
		{ .rid = p_dst, .usage = RD::CALLBACK_RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE }
	};
	RD::get_singleton()->driver_callback_add((RDD::DriverCallback)MFXSpatialEffect::callback, userdata, VectorView<RD::CallbackResource>(res, 2));
}

MFXSpatialContext *MFXSpatialEffect::create_context(CreateParams p_params) const {
	DEV_ASSERT(RD::get_singleton()->has_feature(RD::SUPPORTS_METALFX_SPATIAL));

	RenderingDeviceDriverMetal *rdd = (RenderingDeviceDriverMetal *)RD::get_singleton()->get_device_driver();
	PixelFormats &pf = rdd->get_pixel_formats();
	MTL::Device *dev = rdd->get_device();

	NS::SharedPtr<MTLFX::SpatialScalerDescriptor> desc = NS::TransferPtr(MTLFX::SpatialScalerDescriptor::alloc()->init());
	desc->setInputWidth((NS::UInteger)p_params.input_size.width);
	desc->setInputHeight((NS::UInteger)p_params.input_size.height);

	desc->setOutputWidth((NS::UInteger)p_params.output_size.width);
	desc->setOutputHeight((NS::UInteger)p_params.output_size.height);

	desc->setColorTextureFormat((MTL::PixelFormat)pf.getMTLPixelFormat(p_params.input_format));
	desc->setOutputTextureFormat((MTL::PixelFormat)pf.getMTLPixelFormat(p_params.output_format));
	desc->setColorProcessingMode(MTLFX::SpatialScalerColorProcessingModeLinear);

	MFXSpatialContext *context = memnew(MFXSpatialContext);
	context->scaler = desc->newSpatialScaler(dev);

	return context;
}

#ifdef METAL_MFXTEMPORAL_ENABLED

#pragma mark - Temporal Scaler

MFXTemporalContext::~MFXTemporalContext() {
	if (scaler) {
		scaler->release();
	}
}

MFXTemporalEffect::MFXTemporalEffect() {}
MFXTemporalEffect::~MFXTemporalEffect() {}

MFXTemporalContext *MFXTemporalEffect::create_context(CreateParams p_params) const {
	DEV_ASSERT(RD::get_singleton()->has_feature(RD::SUPPORTS_METALFX_TEMPORAL));

	RenderingDeviceDriverMetal *rdd = (RenderingDeviceDriverMetal *)RD::get_singleton()->get_device_driver();
	PixelFormats &pf = rdd->get_pixel_formats();
	MTL::Device *dev = rdd->get_device();

	NS::SharedPtr<MTLFX::TemporalScalerDescriptor> desc = NS::TransferPtr(MTLFX::TemporalScalerDescriptor::alloc()->init());
	desc->setInputWidth((NS::UInteger)p_params.input_size.width);
	desc->setInputHeight((NS::UInteger)p_params.input_size.height);

	desc->setOutputWidth((NS::UInteger)p_params.output_size.width);
	desc->setOutputHeight((NS::UInteger)p_params.output_size.height);

	desc->setColorTextureFormat((MTL::PixelFormat)pf.getMTLPixelFormat(p_params.input_format));
	desc->setDepthTextureFormat((MTL::PixelFormat)pf.getMTLPixelFormat(p_params.depth_format));
	desc->setMotionTextureFormat((MTL::PixelFormat)pf.getMTLPixelFormat(p_params.motion_format));
	desc->setAutoExposureEnabled(false);

	desc->setOutputTextureFormat((MTL::PixelFormat)pf.getMTLPixelFormat(p_params.output_format));

	MFXTemporalContext *context = memnew(MFXTemporalContext);
	context->scaler = desc->newTemporalScaler(dev);
	context->scaler->setMotionVectorScaleX(p_params.motion_vector_scale.x);
	context->scaler->setMotionVectorScaleY(p_params.motion_vector_scale.y);
	context->scaler->setDepthReversed(true); // Godot uses reverse Z per https://github.com/godotengine/godot/pull/88328

	return context;
}

void MFXTemporalEffect::process(RendererRD::MFXTemporalContext *p_ctx, RendererRD::MFXTemporalEffect::Params p_params) {
	CallbackArgs *userdata = args_allocator.alloc(
			this,
			RDD::TextureID(RD::get_singleton()->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_params.src)),
			RDD::TextureID(RD::get_singleton()->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_params.depth)),
			RDD::TextureID(RD::get_singleton()->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_params.motion)),
			p_params.exposure.is_valid() ? RDD::TextureID(RD::get_singleton()->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_params.exposure)) : RDD::TextureID(),
			p_params.jitter_offset,
			RDD::TextureID(RD::get_singleton()->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_params.dst)),
			*p_ctx,
			p_params.reset);
	RD::CallbackResource res[5] = {
		{ .rid = p_params.src, .usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE },
		{ .rid = p_params.depth, .usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE },
		{ .rid = p_params.motion, .usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE },
		{ .rid = p_params.dst, .usage = RD::CALLBACK_RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE },
		{ .rid = p_params.exposure, .usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE },
	};
	uint32_t res_count = p_params.exposure.is_valid() ? 5 : 4;
	RD::get_singleton()->driver_callback_add((RDD::DriverCallback)MFXTemporalEffect::callback, userdata, VectorView<RD::CallbackResource>(res, res_count));
}

void MFXTemporalEffect::callback(RDD *p_driver, RDD::CommandBufferID p_command_buffer, CallbackArgs *p_userdata) {
	MDCommandBufferBase *obj = (MDCommandBufferBase *)(p_command_buffer.id);
	obj->end();

	MTL::Texture *src_texture = reinterpret_cast<MTL::Texture *>(p_userdata->src.id);
	MTL::Texture *depth = reinterpret_cast<MTL::Texture *>(p_userdata->depth.id);
	MTL::Texture *motion = reinterpret_cast<MTL::Texture *>(p_userdata->motion.id);
	MTL::Texture *exposure = reinterpret_cast<MTL::Texture *>(p_userdata->exposure.id);

	MTL::Texture *dst_texture = reinterpret_cast<MTL::Texture *>(p_userdata->dst.id);

	MTLFX::TemporalScalerBase *scaler = p_userdata->scaler;
	scaler->setFence(obj->external_pass_fence());
	scaler->setReset(p_userdata->reset);
	scaler->setColorTexture(src_texture);
	scaler->setDepthTexture(depth);
	scaler->setMotionTexture(motion);
	scaler->setExposureTexture(exposure);
	scaler->setJitterOffsetX(p_userdata->jitter_offset.x);
	scaler->setJitterOffsetY(p_userdata->jitter_offset.y);
	scaler->setOutputTexture(dst_texture);
	MTLFX::TemporalScaler *s = static_cast<MTLFX::TemporalScaler *>(scaler);
	MTL3::MDCommandBuffer *cmd = (MTL3::MDCommandBuffer *)(p_command_buffer.id);
	s->encodeToCommandBuffer(cmd->get_command_buffer());
	obj->retain_resource(scaler);

	CallbackArgs::free(&p_userdata);
}

#pragma mark - Frame Interpolation

MFXFrameInterpolationContext::~MFXFrameInterpolationContext() {
	if (interpolator) {
		interpolator->release();
	}
	RenderingDevice *rd = RD::get_singleton();
	if (previous_color.is_valid()) {
		rd->free_rid(previous_color);
	}
	if (output.is_valid()) {
		rd->free_rid(output);
	}
}

MFXFrameInterpolationEffect::MFXFrameInterpolationEffect() {}
MFXFrameInterpolationEffect::~MFXFrameInterpolationEffect() {}

MFXFrameInterpolationContext *MFXFrameInterpolationEffect::create_context(const CreateParams &p_params) const {
	ERR_FAIL_COND_V(!RD::get_singleton()->has_feature(RD::SUPPORTS_METALFX_FRAME_INTERPOLATION), nullptr);

	RenderingDevice *rd = RD::get_singleton();
	RenderingDeviceDriverMetal *rdd = (RenderingDeviceDriverMetal *)rd->get_device_driver();
	PixelFormats &pf = rdd->get_pixel_formats();
	MTL::Device *dev = rdd->get_device();

	MFXFrameInterpolationContext *context = memnew(MFXFrameInterpolationContext);
	context->input_size = p_params.input_size;
	context->output_size = p_params.output_size;
	context->color_format = p_params.color_format;
	context->depth_format = p_params.depth_format;
	context->motion_format = p_params.motion_format;

	if (__builtin_available(macOS 26.0, iOS 26.0, tvOS 26.0, *)) {
		NS::SharedPtr<MTLFX::FrameInterpolatorDescriptor> desc = NS::TransferPtr(MTLFX::FrameInterpolatorDescriptor::alloc()->init());
		// Color in and out at the output size; depth and motion at the render size.
		desc->setInputWidth((NS::UInteger)p_params.input_size.width);
		desc->setInputHeight((NS::UInteger)p_params.input_size.height);
		desc->setOutputWidth((NS::UInteger)p_params.output_size.width);
		desc->setOutputHeight((NS::UInteger)p_params.output_size.height);
		desc->setColorTextureFormat((MTL::PixelFormat)pf.getMTLPixelFormat(p_params.color_format));
		desc->setOutputTextureFormat((MTL::PixelFormat)pf.getMTLPixelFormat(p_params.color_format));
		desc->setDepthTextureFormat((MTL::PixelFormat)pf.getMTLPixelFormat(p_params.depth_format));
		desc->setMotionTextureFormat((MTL::PixelFormat)pf.getMTLPixelFormat(p_params.motion_format));
		context->interpolator = desc->newFrameInterpolator(dev);
	}
	if (context->interpolator == nullptr) {
		memdelete(context);
		ERR_FAIL_V_MSG(nullptr, "MetalFX: could not create the frame interpolator.");
	}

	RD::TextureFormat tf;
	tf.format = p_params.color_format;
	tf.width = p_params.output_size.width;
	tf.height = p_params.output_size.height;
	tf.usage_bits = RD::TEXTURE_USAGE_SAMPLING_BIT | RD::TEXTURE_USAGE_STORAGE_BIT | RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_TO_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT;
	context->previous_color = rd->texture_create(tf, RD::TextureView());
	rd->set_resource_name(context->previous_color, "MetalFX frame interpolation: previous color");
	context->output = rd->texture_create(tf, RD::TextureView());
	rd->set_resource_name(context->output, "MetalFX frame interpolation: generated frame");

	return context;
}

bool MFXFrameInterpolationEffect::process(MFXFrameInterpolationContext *p_ctx, RID p_color, const MFXFrameInterpolationInputs &p_inputs, uint64_t p_frame) {
	RenderingDevice *rd = RD::get_singleton();
	// Interpolate only between two consecutive frames, with this frame's depth and motion.
	bool interpolate = p_ctx->last_frame != 0 && p_ctx->last_frame + 1 == p_frame && p_inputs.frame == p_frame && p_inputs.depth.is_valid() && p_inputs.motion.is_valid();

	if (interpolate) {
		CallbackArgs *userdata = args_allocator.alloc();
		userdata->owner = this;
		userdata->interpolator = p_ctx->interpolator;
		userdata->color = RDD::TextureID(rd->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_color));
		userdata->previous_color = RDD::TextureID(rd->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_ctx->previous_color));
		userdata->depth = RDD::TextureID(rd->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_inputs.depth));
		userdata->motion = RDD::TextureID(rd->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_inputs.motion));
		userdata->output = RDD::TextureID(rd->get_driver_resource(RDC::DRIVER_RESOURCE_TEXTURE, p_ctx->output));
		userdata->inputs = p_inputs;

		RD::CallbackResource res[5] = {
			{ .rid = p_color, .usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE },
			{ .rid = p_ctx->previous_color, .usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE },
			{ .rid = p_inputs.depth, .usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE },
			{ .rid = p_inputs.motion, .usage = RD::CALLBACK_RESOURCE_USAGE_TEXTURE_SAMPLE },
			{ .rid = p_ctx->output, .usage = RD::CALLBACK_RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE },
		};
		rd->draw_command_begin_label("MetalFX Frame Interpolation");
		rd->driver_callback_add((RDD::DriverCallback)MFXFrameInterpolationEffect::callback, userdata, VectorView<RD::CallbackResource>(res, 5));
		rd->draw_command_end_label();
	}

	// This frame becomes the previous one of the next interpolation.
	Vector3 size(p_ctx->output_size.width, p_ctx->output_size.height, 1);
	rd->texture_copy(p_color, p_ctx->previous_color, Vector3(), Vector3(), size, 0, 0, 0, 0);
	p_ctx->last_frame = p_frame;

	return interpolate;
}

void MFXFrameInterpolationEffect::callback(RDD *p_driver, RDD::CommandBufferID p_command_buffer, CallbackArgs *p_userdata) {
	MDCommandBufferBase *obj = (MDCommandBufferBase *)(p_command_buffer.id);
	obj->end();

	if (__builtin_available(macOS 26.0, iOS 26.0, tvOS 26.0, *)) {
		const MFXFrameInterpolationInputs &in = p_userdata->inputs;
		MTLFX::FrameInterpolator *fi = p_userdata->interpolator;
		fi->setFence(obj->external_pass_fence());
		fi->setColorTexture(reinterpret_cast<MTL::Texture *>(p_userdata->color.id));
		fi->setPrevColorTexture(reinterpret_cast<MTL::Texture *>(p_userdata->previous_color.id));
		fi->setDepthTexture(reinterpret_cast<MTL::Texture *>(p_userdata->depth.id));
		fi->setMotionTexture(reinterpret_cast<MTL::Texture *>(p_userdata->motion.id));
		fi->setOutputTexture(reinterpret_cast<MTL::Texture *>(p_userdata->output.id));
		fi->setMotionVectorScaleX(in.motion_vector_scale.x);
		fi->setMotionVectorScaleY(in.motion_vector_scale.y);
		fi->setJitterOffsetX(in.jitter_offset.x);
		fi->setJitterOffsetY(in.jitter_offset.y);
		fi->setDeltaTime(in.delta_time);
		fi->setNearPlane(in.near_plane);
		fi->setFarPlane(in.far_plane);
		fi->setFieldOfView(in.fov_degrees);
		fi->setAspectRatio(in.aspect_ratio);
		fi->setDepthReversed(true); // Godot uses reverse Z, as for the temporal scaler.
		fi->setShouldResetHistory(in.reset);
		MTL3::MDCommandBuffer *cmd = (MTL3::MDCommandBuffer *)(p_command_buffer.id);
		fi->encodeToCommandBuffer(cmd->get_command_buffer());
		obj->retain_resource(fi);
	}

	CallbackArgs::free(&p_userdata);
}

#endif

#endif

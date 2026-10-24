#pragma once
#include "rd_adapter.h"
#include "fsr3_backend.h"
#include <godot_cpp/classes/viewport_upscaler.hpp>
#include <godot_cpp/classes/render_data.hpp>
#include <godot_cpp/classes/render_scene_buffers_rd.hpp>
#include <godot_cpp/classes/render_scene_data.hpp>
#include <atomic>
#include <map>
#include <memory>

class SimpleComputeUpscaler : public ViewportUpscaler {
    GDCLASS(SimpleComputeUpscaler, ViewportUpscaler)
    RID shader;
    RID pipeline;
    std::atomic<int64_t> dispatch_count{0};
protected:
    static void _bind_methods();
public:
    void _render_callback(RenderData *data) override;
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
    std::atomic<float> sharpness{0.2f};
    std::atomic<uint64_t> reset_serial{1};
    std::atomic<int64_t> dispatch_count{0};
    std::atomic<int64_t> context_count{0};
    std::atomic<int64_t> gpu_dispatch_count{0};
    std::atomic<int64_t> failure_count{0};
    bool missing_api_reported = false;
protected:
    static void _bind_methods();
public:
    FSR3Upscaler();
    ~FSR3Upscaler();
    void _render_callback(RenderData *data) override;
    void set_sharpness(float value) { sharpness.store(CLAMP(value, 0.0f, 1.0f)); }
    float get_sharpness() const { return sharpness.load(); }
    virtual void reset_history() { reset_serial.fetch_add(1); }
    void configure_scale(double scale);
    virtual int64_t get_dispatch_count() const { return dispatch_count.load(); }
    int64_t get_gpu_dispatch_count() const { return gpu_dispatch_count.load(); }
    int64_t get_context_count() const { return context_count.load(); }
    virtual int64_t get_failure_count() const { return failure_count.load(); }
};

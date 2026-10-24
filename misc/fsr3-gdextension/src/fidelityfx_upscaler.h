#pragma once
#include "upscalers.h"
#include <mutex>
#include <godot_cpp/variant/packed_string_array.hpp>

struct FidelityFXBinaryState;
struct FidelityFXBinaryStats;

// The public AMD ABI supplies the algorithm. FSR3Upscaler remains the native
// source fallback, and owns the common jitter/sharpness/history configuration.
class FidelityFXUpscaler : public FSR3Upscaler {
    GDCLASS(FidelityFXUpscaler, FSR3Upscaler)
    mutable std::mutex settings_mutex;
    String library_path;
    String preferred_version;
    bool allow_source_fallback = true;
    std::atomic<uint64_t> settings_revision{1};
    std::atomic<uint64_t> binary_reset_serial{1};
    uint64_t applied_revision = 0;
    bool initialized = false;
    std::unique_ptr<FidelityFXBinaryState> binary;
    std::shared_ptr<FidelityFXBinaryStats> stats;
protected:
    static void _bind_methods();
public:
    FidelityFXUpscaler();
    ~FidelityFXUpscaler();
    void _render_callback(RenderData *data) override;
    void reset_history() override;
    int64_t get_dispatch_count() const override;
    int64_t get_failure_count() const override;
    void set_library_path(const String &value);
    String get_library_path() const;
    void set_preferred_version(const String &value);
    String get_preferred_version() const;
    void set_allow_source_fallback(bool value);
    bool get_allow_source_fallback() const;
    String get_active_backend() const;
    String get_active_version() const;
    String get_binary_error() const;
    PackedStringArray get_available_versions() const;
    int64_t get_binary_dispatch_count() const;
    int64_t get_binary_failure_count() const;
};

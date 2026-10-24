#include "upscalers.h"
#include "fidelityfx_upscaler.h"
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/rd_pipeline_specialization_constant.hpp>
using namespace godot;
template <typename... Types> static void register_engine_bindings() {
    // Make registration explicit before render-thread callbacks. The tested
    // Clang/MinGW build did not populate these through the SDK's unused inline
    // registration helpers; returned engine objects then logged missing bindings.
    (internal::EngineClassRegistration<Types>::callback(), ...);
}
static void initialize(ModuleInitializationLevel level) {
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
    register_engine_bindings<Object, RefCounted, Resource, ViewportUpscaler,
            RenderData, RenderSceneBuffers, RenderSceneBuffersRD, RenderSceneData,
            RenderingServer, RenderingDevice, RDTextureFormat, RDTextureView,
            RDSamplerState, RDUniform, RDShaderSource, RDShaderSPIRV,
            UniformSetCacheRD, Engine, Time, OS, RDPipelineSpecializationConstant,
            ProjectSettings>();
    GDREGISTER_CLASS(SimpleComputeUpscaler);
    GDREGISTER_CLASS(FSR3Upscaler);
    GDREGISTER_CLASS(FidelityFXUpscaler);
}
static void terminate(ModuleInitializationLevel) {}
extern "C" GDExtensionBool GDE_EXPORT fsr3_library_init(GDExtensionInterfaceGetProcAddress get_proc_address,
        GDExtensionClassLibraryPtr library, GDExtensionInitialization *initialization) {
    GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(initialize); init.register_terminator(terminate);
    init.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}

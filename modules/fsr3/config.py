def can_build(env, platform):
    return platform in ("linuxbsd", "windows") and not env["disable_3d"]


def configure(env):
    pass


def get_doc_classes():
    return ["FSR3Upscaler", "FidelityFXUpscaler", "SimpleComputeUpscaler"]


def get_doc_path():
    return "doc_classes"

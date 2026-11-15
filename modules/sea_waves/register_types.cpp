/**************************************************************************/
/*  register_types.cpp                                                    */
/**************************************************************************/
/* Deep Salvage: the sea's long waves on the CPU (SeaWaveField).          */
/**************************************************************************/

#include "register_types.h"

#include "sea_wave_field.h"

#include "core/object/class_db.h"

void initialize_sea_waves_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	GDREGISTER_CLASS(SeaWaveField);
}

void uninitialize_sea_waves_module(ModuleInitializationLevel p_level) {
}

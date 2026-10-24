@tool
extends EditorPlugin

const SETTINGS := {
	"rendering/fsr3/enabled": true,
	"rendering/fsr3/render_scale": 0.67,
	"rendering/fsr3/native_aa": false,
	"rendering/fsr3/sharpness": 0.2,
	"rendering/fsr3/prefer_amd_binaries": true,
	"rendering/fsr3/amd_library_path": "",
	"rendering/fsr3/amd_version": "",
}

func _enter_tree() -> void:
	for key in SETTINGS:
		if not ProjectSettings.has_setting(key):
			ProjectSettings.set_setting(key, SETTINGS[key])
		ProjectSettings.set_initial_value(key, SETTINGS[key])
	if not ProjectSettings.has_setting("autoload/FSR3Addon"):
		add_autoload_singleton("FSR3Addon", "res://addons/fsr3/autoload.gd")
	ProjectSettings.save()

# _exit_tree also runs when the editor closes. Remove persistent autoloads
# only when the user disables the plugin.
func _disable_plugin() -> void:
	remove_autoload_singleton("FSR3Addon")
	ProjectSettings.save()

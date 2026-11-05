#include "browser-dock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QMainWindow>
#include <QPointer>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

// Edit here to change the dock id. If you change it, OBS treats it as a new dock:
// 1. its saved size and position are lost, 2. it starts hidden until opened from the Docks menu.
constexpr const char *kDockId = "browser-source-manager";

QPointer<BrowserDock> dock;

void OnFrontendEvent(enum obs_frontend_event event, void *)
{
	if (dock)
		dock->HandleFrontendEvent(event);
}

}

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("DockTitle");
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("Description");
}

bool obs_module_load(void)
{
	auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto *widget = new BrowserDock(mainWindow);

	if (!obs_frontend_add_dock_by_id(kDockId, obs_module_text("DockTitle"), widget)) {
		delete widget;
		blog(LOG_WARNING, "[%s] could not add dock", PLUGIN_NAME);
		return false;
	}

	dock = widget;
	obs_frontend_add_event_callback(OnFrontendEvent, nullptr);
	blog(LOG_INFO, "[%s] loaded (version %s)", PLUGIN_NAME, PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(OnFrontendEvent, nullptr);
}

/*
Browser Source Manager
Copyright (C) 2026 Moh-01

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "browser-dock.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QMainWindow>
#include <QPointer>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

constexpr const char *kDockId = "browser-source-manager";

// The main window owns the dock and deletes it on shutdown.
QPointer<BrowserDock> dock;

void OnFrontendEvent(enum obs_frontend_event event, void *)
{
	if (dock)
		dock->HandleFrontendEvent(event);
}

} // namespace

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

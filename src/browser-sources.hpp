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

#pragma once

#include <string>
#include <vector>

enum class BrowserState {
	OnAir,         // Shown in the program output
	Preview,       // Shown somewhere (studio mode preview, projector) but not on air
	RunningHidden, // Not shown anywhere, but the page is still loaded and rendering
	Unloaded,      // Not shown anywhere and shut down until it is shown again
};

struct BrowserSourceInfo {
	std::string uuid;
	std::string name;
	std::string url; // Web address, or file path for local files
	bool isLocalFile = false;
	int width = 0;
	int height = 0;
	int fps = 0;
	bool customFps = false;
	bool shutdownWhenHidden = false;
	BrowserState state = BrowserState::Unloaded;
	std::vector<std::string> scenes;
};

std::vector<BrowserSourceInfo> CollectBrowserSources();
int CanvasFps();

bool RefreshBrowserSource(const std::string &uuid);
bool SetShutdownWhenHidden(const std::string &uuid, bool enabled);
void OpenSourceProperties(const std::string &uuid);
void OpenSourceInteraction(const std::string &uuid);

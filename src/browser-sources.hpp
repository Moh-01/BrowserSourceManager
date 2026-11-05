#pragma once

#include <string>
#include <vector>

enum class BrowserState {
	OnAir,
	Preview,
	RunningHidden,
	Unloaded,
};

struct BrowserSourceInfo {
	std::string uuid;
	std::string name;
	std::string url;
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

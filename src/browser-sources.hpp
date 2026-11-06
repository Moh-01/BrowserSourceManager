#pragma once

#include <optional>
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
	bool restartWhenActive = false;
	bool rerouteAudio = false;
	BrowserState state = BrowserState::Unloaded;
	std::vector<std::string> scenes;
};

struct BrowserEdit {
	std::optional<int> width;
	std::optional<int> height;
	std::optional<bool> customFps;
	std::optional<int> fps;
	std::optional<bool> shutdownWhenHidden;
	std::optional<bool> restartWhenActive;
	std::optional<bool> rerouteAudio;

	bool ReloadsPage() const { return customFps || fps || shutdownWhenHidden || restartWhenActive || rerouteAudio; }
	bool IsEmpty() const { return !width && !height && !ReloadsPage(); }
};

std::vector<BrowserSourceInfo> CollectBrowserSources();
std::vector<std::string> SceneNames();
int CanvasFps();

bool RefreshBrowserSource(const std::string &uuid);
bool SetShutdownWhenHidden(const std::string &uuid, bool enabled);
bool ApplyBrowserEdit(const std::string &uuid, const BrowserEdit &edit);
void OpenSourceProperties(const std::string &uuid);
void OpenSourceInteraction(const std::string &uuid);

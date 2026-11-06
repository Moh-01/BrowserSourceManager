#include "browser-sources.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace {

constexpr const char *kBrowserSourceId = "browser_source";

using SceneMap = std::unordered_map<std::string, std::vector<std::string>>;

struct SceneWalk {
	SceneMap *map;
	const char *sceneName;
};

bool IsBrowserSource(obs_source_t *source)
{
	const char *id = obs_source_get_unversioned_id(source);
	return id && strcmp(id, kBrowserSourceId) == 0;
}

bool AddSceneItem(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto *walk = static_cast<SceneWalk *>(param);

	if (obs_sceneitem_is_group(item))
		obs_sceneitem_group_enum_items(item, AddSceneItem, param);

	obs_source_t *source = obs_sceneitem_get_source(item);
	if (!IsBrowserSource(source))
		return true;

	auto &scenes = (*walk->map)[obs_source_get_uuid(source)];
	if (std::find(scenes.begin(), scenes.end(), walk->sceneName) == scenes.end())
		scenes.emplace_back(walk->sceneName);
	return true;
}

SceneMap MapScenes()
{
	SceneMap map;
	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);

	for (size_t i = 0; i < scenes.sources.num; i++) {
		obs_source_t *sceneSource = scenes.sources.array[i];
		SceneWalk walk{&map, obs_source_get_name(sceneSource)};
		obs_scene_enum_items(obs_scene_from_source(sceneSource), AddSceneItem, &walk);
	}

	obs_frontend_source_list_free(&scenes);
	return map;
}

bool AddBrowserSource(void *param, obs_source_t *source)
{
	if (!IsBrowserSource(source))
		return true;

	auto *list = static_cast<std::vector<BrowserSourceInfo> *>(param);
	OBSDataAutoRelease settings = obs_source_get_settings(source);

	BrowserSourceInfo info;
	info.uuid = obs_source_get_uuid(source);
	info.name = obs_source_get_name(source);
	info.isLocalFile = obs_data_get_bool(settings, "is_local_file");
	info.url = obs_data_get_string(settings, info.isLocalFile ? "local_file" : "url");
	info.width = (int)obs_data_get_int(settings, "width");
	info.height = (int)obs_data_get_int(settings, "height");
	info.customFps = obs_data_get_bool(settings, "fps_custom");
	info.fps = info.customFps ? (int)obs_data_get_int(settings, "fps") : CanvasFps();
	info.shutdownWhenHidden = obs_data_get_bool(settings, "shutdown");
	info.restartWhenActive = obs_data_get_bool(settings, "restart_when_active");
	info.rerouteAudio = obs_data_get_bool(settings, "reroute_audio");

	if (obs_source_active(source))
		info.state = BrowserState::OnAir;
	else if (obs_source_showing(source))
		info.state = BrowserState::Preview;
	else if (info.shutdownWhenHidden)
		info.state = BrowserState::Unloaded;
	else
		info.state = BrowserState::RunningHidden;

	list->push_back(std::move(info));
	return true;
}

OBSSourceAutoRelease GetBrowserSource(const std::string &uuid)
{
	OBSSourceAutoRelease source = obs_get_source_by_uuid(uuid.c_str());
	if (source && !IsBrowserSource(source))
		return nullptr;
	return source;
}

}

std::vector<BrowserSourceInfo> CollectBrowserSources()
{
	std::vector<BrowserSourceInfo> list;
	obs_enum_sources(AddBrowserSource, &list);

	SceneMap scenes = MapScenes();
	for (auto &info : list) {
		auto it = scenes.find(info.uuid);
		if (it != scenes.end())
			info.scenes = std::move(it->second);
	}
	return list;
}

std::vector<std::string> SceneNames()
{
	std::vector<std::string> names;
	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++)
		names.emplace_back(obs_source_get_name(scenes.sources.array[i]));
	obs_frontend_source_list_free(&scenes);
	return names;
}

int CanvasFps()
{
	obs_video_info ovi = {};
	if (!obs_get_video_info(&ovi) || ovi.fps_den == 0)
		return 30;
	return (int)std::lround((double)ovi.fps_num / ovi.fps_den);
}

bool RefreshBrowserSource(const std::string &uuid)
{
	OBSSourceAutoRelease source = GetBrowserSource(uuid);
	if (!source)
		return false;

	obs_properties_t *props = obs_source_properties(source);
	obs_property_t *button = obs_properties_get(props, "refreshnocache");
	if (button)
		obs_property_button_clicked(button, source);
	obs_properties_destroy(props);
	return button != nullptr;
}

bool SetShutdownWhenHidden(const std::string &uuid, bool enabled)
{
	OBSSourceAutoRelease source = GetBrowserSource(uuid);
	if (!source)
		return false;

	OBSDataAutoRelease update = obs_data_create();
	obs_data_set_bool(update, "shutdown", enabled);
	obs_source_update(source, update);
	return true;
}

bool ApplyBrowserEdit(const std::string &uuid, const BrowserEdit &edit)
{
	OBSSourceAutoRelease source = GetBrowserSource(uuid);
	if (!source || edit.IsEmpty())
		return false;

	OBSDataAutoRelease update = obs_data_create();
	if (edit.width)
		obs_data_set_int(update, "width", *edit.width);
	if (edit.height)
		obs_data_set_int(update, "height", *edit.height);
	if (edit.customFps)
		obs_data_set_bool(update, "fps_custom", *edit.customFps);
	if (edit.fps)
		obs_data_set_int(update, "fps", *edit.fps);
	if (edit.shutdownWhenHidden)
		obs_data_set_bool(update, "shutdown", *edit.shutdownWhenHidden);
	if (edit.restartWhenActive)
		obs_data_set_bool(update, "restart_when_active", *edit.restartWhenActive);
	if (edit.rerouteAudio)
		obs_data_set_bool(update, "reroute_audio", *edit.rerouteAudio);
	obs_source_update(source, update);
	return true;
}

void OpenSourceProperties(const std::string &uuid)
{
	OBSSourceAutoRelease source = GetBrowserSource(uuid);
	if (source)
		obs_frontend_open_source_properties(source);
}

void OpenSourceInteraction(const std::string &uuid)
{
	OBSSourceAutoRelease source = GetBrowserSource(uuid);
	if (source)
		obs_frontend_open_source_interaction(source);
}

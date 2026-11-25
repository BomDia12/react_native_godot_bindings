#pragma once

#include "rn_theme_metrics.h"

#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

class RNGameServicesHandle;
class RNMainThreadQueue;

// Measures BBCode for layout. RichTextLabel only shapes text inside the scene
// tree, so a hidden label on the main thread measures, as Godot would lay out
// the mounted one, and the JS thread waits for it unless the result is known.
class RNRichTextMeasurer {
	using Key = std::tuple<std::string, std::vector<std::string>, std::string, float, uint64_t>;
	struct Entry {
		Key key;
		Size2 size;
	};
	std::shared_ptr<RNMainThreadQueue> main_thread;
	std::shared_ptr<RNGameServicesHandle> services;
	std::mutex mutex;
	std::list<Entry> recent;
	std::vector<std::string> installed_effects;

public:
	RNRichTextMeasurer(std::shared_ptr<RNMainThreadQueue> p_main_thread, std::shared_ptr<RNGameServicesHandle> p_services);
	Size2 measure(const String &p_bbcode, const std::vector<std::string> &p_effects, const std::string &p_theme_type, const std::shared_ptr<const RNThemeMetrics> &p_metrics, float p_max_width);
};

#pragma once

#include "../mounting/rn_component_view.h"

#include "scene/gui/video_stream_player.h"

#include <functional>

class RNVideoControl : public VideoStreamPlayer {
	GDCLASS(RNVideoControl, VideoStreamPlayer);
	std::function<void()> ended;
	void _finished();

protected:
	static void _bind_methods() {}

public:
	RNVideoControl();
	Size2 get_combined_minimum_size() const override { return Size2(); }
	Size2 get_combined_maximum_size() const override { return Size2(-1, -1); }
	void set_ended_callback(std::function<void()> p_ended) { ended = std::move(p_ended); }
};

class RNVideoComponentView : public RNComponentView {
	RNVideoControl *video;
	String source;
	uint64_t request = 0;

	void _load(const String &p_source);
	void _loaded(const Ref<Resource> &p_resource, const String &p_error);

public:
	explicit RNVideoComponentView(facebook::react::Tag p_tag);
	~RNVideoComponentView() override;
	void update_props(const facebook::react::Props::Shared &p_old, const facebook::react::Props::Shared &p_new) override;
	void update_event_emitter(const facebook::react::EventEmitter::Shared &p_emitter) override;
	void handle_command(const std::string &p_name, const folly::dynamic &p_args) override;
};

#pragma once

#include "rn_service_settings.h"

#include <react/http/IWebSocketClient.h>

#include <functional>
#include <memory>
#include <string>

namespace facebook::react {
class PackagerConnection;
struct ReactInstanceConfig;
} // namespace facebook::react

class RNHTTPService;

// Where a resource downloaded from p_url is cached for ResourceLoader.
String rn_downloaded_resource_path(const std::string &p_url);

// The Metro developer server one runtime loads its bundle from. Metro reload
// requests go to the host's reload instead of upstream's in-place ReactHost
// reload. Destroy it on the main thread, which runs its socket callbacks.
class RNDevServer {
	RNDevServerSettings settings;
	std::string app_id;
	std::unique_ptr<facebook::react::PackagerConnection> packager;

public:
	explicit RNDevServer(const RNDevServerSettings &p_settings);
	~RNDevServer();

	std::string bundle_url() const;
	// A segment's modules without Metro's prelude and polyfills, which the
	// bundle already has.
	std::string segment_url(const String &p_entry) const;
	void configure(facebook::react::ReactInstanceConfig &r_config) const;
	// Stores what p_url serves where rn_downloaded_resource_path(p_url) says.
	void download(RNHTTPService &p_http, const std::string &p_url, std::function<void(bool p_downloaded)> p_done) const;
	void connect(facebook::react::WebSocketClientFactory p_factory, std::function<void()> p_reload);
};

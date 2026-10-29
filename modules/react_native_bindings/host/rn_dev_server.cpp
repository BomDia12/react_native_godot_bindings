#include "rn_dev_server.h"

#include "../native_modules/rn_http_service.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"

#include <react/devsupport/PackagerConnection.h>
#include <react/runtime/ReactInstanceConfig.h>

using namespace facebook::react;

String rn_downloaded_resource_path(const std::string &p_url) {
	return "user://react_native/cache/downloads/" + String::utf8(p_url.c_str()).sha256_text();
}

// Upstream puts the app id into inspector URLs unescaped, so it keeps only
// the characters a URL query allows as they are.
RNDevServer::RNDevServer(const RNDevServerSettings &p_settings) :
		settings(p_settings) {
	const String name = GLOBAL_GET("application/config/name");
	String id;
	for (int i = 0; i < name.length(); ++i) {
		if (is_ascii_alphanumeric_char(name[i]) || name[i] == '.' || name[i] == '_' || name[i] == '~' || name[i] == '-') {
			id += name[i];
		}
	}
	// The editor previews the scene in a runtime of its own, which DevTools
	// lists apart from the game's.
	const String suffix = Engine::get_singleton()->is_editor_hint() ? "-editor" : "";
	app_id = ((id.is_empty() ? String("godot") : id) + suffix).utf8().get_data();
}

RNDevServer::~RNDevServer() = default;

// The query is upstream DevServerHelper's, which always asks Metro for the
// Android platform instead of Godot's.
std::string RNDevServer::bundle_url() const {
	const String url = vformat("http://%s:%d/%s.bundle?platform=godot&dev=true&lazy=true&minify=false&app=%s&modulesOnly=false&runModule=true&inlineSourceMap=false&excludeSource=true&sourcePaths=url-server", settings.host, settings.port, settings.entry, String::utf8(app_id.c_str()));
	return url.utf8().get_data();
}

// The inspector registers the runtime with Metro's DevTools proxy. Dev mode
// stays off: it would load an Android bundle and wire Metro's reload to
// upstream's in-place reload.
void RNDevServer::configure(ReactInstanceConfig &r_config) const {
	r_config.appId = app_id;
	r_config.deviceName = OS::get_singleton()->get_name().utf8().get_data();
	r_config.devServerHost = settings.host.utf8().get_data();
	r_config.devServerPort = uint32_t(settings.port);
	r_config.enableDevMode = false;
	r_config.enableInspector = true;
}

void RNDevServer::download_bundle(RNHTTPService &p_http, std::function<void(bool)> p_done) const {
	const std::string url = bundle_url();
	RNHTTPRequest request;
	request.url = String::utf8(url.c_str());
	RNError error;
	auto completed = [url, p_done](RNHTTPResponse p_response) {
		if (p_response.error.is_set() || p_response.status != 200) {
			WARN_PRINT(vformat("The Metro bundle at %s could not be downloaded (%s); loading the prebuilt bundle.", String::utf8(url.c_str()), p_response.error.is_set() ? p_response.error.describe() : vformat("HTTP %d", p_response.status)));
			p_done(false);
			return;
		}
		const String path = rn_downloaded_resource_path(url);
		DirAccess::make_dir_recursive_absolute(path.get_base_dir());
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		const bool stored = file.is_valid() && file->store_buffer(p_response.body);
		file.unref();
		p_done(stored);
	};
	if (p_http.submit(request, completed, error) == 0) {
		WARN_PRINT(vformat("The Metro bundle at %s could not be requested (%s); loading the prebuilt bundle.", request.url, error.describe()));
		p_done(false);
	}
}

void RNDevServer::connect(WebSocketClientFactory p_factory, std::function<void()> p_reload) {
	const std::string url = vformat("ws://%s:%d/message", settings.host, settings.port).utf8().get_data();
	packager = std::make_unique<PackagerConnection>(std::move(p_factory), url, std::move(p_reload), [] {});
}

#include "rn_logging.h"

#include "core/error/error_macros.h"
#include "core/string/print_string.h"
#include "core/variant/variant.h"

#include <glog/logging.h>

namespace {

// Upstream logs through glog; its messages belong in Godot's output, where
// errors reach the debugger and the rest only shows in verbose mode.
class RNGodotLogSink : public google::LogSink {
public:
	void send(google::LogSeverity p_severity, const char *p_full_filename, const char *p_base_filename, int p_line, const struct ::tm *p_time, const char *p_message, size_t p_message_length) override {
		const String message = vformat("%s:%d] %s", p_base_filename, p_line, String::utf8(p_message, p_message_length));
		if (p_severity >= google::GLOG_ERROR) {
			ERR_PRINT(message);
		} else {
			print_verbose(message);
		}
	}
};

} // namespace

void rn_install_logging() {
	static RNGodotLogSink sink;
	FLAGS_logtostderr = false;
	FLAGS_stderrthreshold = google::NUM_SEVERITIES;
	google::InitGoogleLogging("godot");
	for (int severity = 0; severity < google::NUM_SEVERITIES; ++severity) {
		google::SetLogDestination(severity, "");
	}
	google::AddLogSink(&sink);
}

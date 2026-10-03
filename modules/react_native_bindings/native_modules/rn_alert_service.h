#pragma once

#include "../runtime/rn_execution_scope.h"
#include "rn_native_module_registry.h"

#include "core/object/object.h"

struct RNRuntimeCoordinatorState;
class GodotAlerts : public Object {
	GDCLASS(GodotAlerts, Object);

	struct Request {
		RNCompletionToken completion;
		RNExecutionOrigin origin;
		ObjectID owner;
		ObjectID dialog;
		String session;
		Dictionary payload;
		bool bubbled = false;
	};
	HashMap<String, Request> requests;
	Callable handler;
	std::weak_ptr<RNRuntimeCoordinatorState> state;
	static GodotAlerts *singleton;

	bool bubble(const String &p_request, RNError &r_error);
	void _selected(const String &p_request, int p_button);
	void _dismissed(const String &p_request);
	void discard(const String &p_request, const RNError &p_error);

protected:
	static void _bind_methods();

public:
	GodotAlerts();
	~GodotAlerts() override;
	static GodotAlerts *get_singleton() { return singleton; }
	void configure(const std::shared_ptr<RNRuntimeCoordinatorState> &p_state) { state = p_state; }
	void set_alert_handler(const Callable &p_handler) { handler = p_handler; }
	bool validate_origin(const Variant &p_origin, RNExecutionOrigin &r_origin, ObjectID &r_root, RNError &r_error) const;
	void request(const Variant &p_origin, const Dictionary &p_payload, const String &p_mode, const String &p_session, const RNCallContext &p_context, const RNCompletionToken &p_completion);
	bool complete_alert(const String &p_request, const Dictionary &p_result);
	bool complete_for(ObjectID p_owner, const String &p_request, const Dictionary &p_result, RNError &r_error);
	bool reply(const String &p_session, const String &p_request, const Dictionary &p_result, RNError &r_error);
	void close_session(const String &p_session);
	void close_surface(int p_tag, uint64_t p_epoch);
	void cancel(const String &p_request);
	void shutdown();
};
bool rn_register_alert_module(RNNativeModuleRegistry &p_registry, RNError &r_error);

#pragma once

#include "../interop/rn_error.h"

#include "core/object/object.h"
#include "core/templates/hash_map.h"

#include <functional>
#include <mutex>
#include <vector>

class Node;

using RNAlertCompletion = std::function<void(const Dictionary &p_result, const RNError &p_error)>;
using RNAlertPresenter = std::function<bool(const String &p_request, int p_root_tag, const Dictionary &p_payload)>;

// Presents React Native alerts on the main thread. A request bubbles from its
// root's handler to the application handler and, when neither is installed, to
// a native dialog. Roots can also present natively or through a JS presenter.
class GodotAlerts : public Object {
	GDCLASS(GodotAlerts, Object);

	struct Request {
		RNAlertCompletion completion;
		ObjectID root;
		int root_tag = 0;
		ObjectID owner;
		ObjectID dialog;
		Dictionary payload;
		bool bubbled = false;
	};

	static GodotAlerts *singleton;
	HashMap<String, Request> requests;
	uint64_t next_request = 1;
	Callable handler;
	std::mutex presenter_mutex;
	std::vector<RNAlertPresenter> presenters;

	void _bubble(const String &p_request);
	void _present_dialog(const String &p_request, Node *p_parent);
	void _selected(const String &p_request, int p_button);
	void _dismissed(const String &p_request);
	void _restore_dialog(const String &p_request);
	void _discard(const String &p_request, const RNError &p_error);

protected:
	static void _bind_methods();

public:
	GodotAlerts();
	~GodotAlerts() override;
	static GodotAlerts *get_singleton() { return singleton; }

	void set_alert_handler(const Callable &p_handler) { handler = p_handler; }
	bool complete_alert(const String &p_request, const Dictionary &p_result);

	// JS presenters live in GodotAlert module instances; each stays registered
	// until it reports that its module is gone.
	void add_presenter(RNAlertPresenter p_presenter);
	void clear_presenters();
	void request(Object *p_root, int p_root_tag, const Dictionary &p_payload, const String &p_mode, RNAlertCompletion p_completion);
	bool complete_for(ObjectID p_owner, const String &p_request, const Dictionary &p_result, RNError &r_error);
	bool reply(int p_root_tag, const String &p_request, const Dictionary &p_result, RNError &r_error);
	bool decline(int p_root_tag, const String &p_request, RNError &r_error);
	void close_surface(int p_root_tag);
	void shutdown();
};

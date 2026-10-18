#include "rn_inspector_socket.h"

#include <folly/json.h>

namespace facebook::react {

namespace {

class RNInspectorWebSocketClient : public IWebSocketClient {
	std::shared_ptr<IWebSocketClient> socket;
	std::function<void()> reload;

public:
	RNInspectorWebSocketClient(std::unique_ptr<IWebSocketClient> p_socket, std::function<void()> p_reload) :
			socket(std::move(p_socket)), reload(std::move(p_reload)) {}

	void setOnClosedCallback(OnClosedCallback &&p_callback) noexcept override {
		socket->setOnClosedCallback(std::move(p_callback));
	}

	void setOnMessageCallback(OnMessageCallback &&p_callback) noexcept override {
		if (!p_callback) {
			socket->setOnMessageCallback(nullptr);
			return;
		}
		socket->setOnMessageCallback([weak_socket = std::weak_ptr<IWebSocketClient>(socket), reload = reload, callback = std::move(p_callback)](const std::string &p_message) {
			const std::optional<std::string> reply = rn_inspector_reload_reply(p_message);
			if (!reply) {
				callback(p_message);
				return;
			}
			if (auto alive = weak_socket.lock()) {
				alive->send(*reply);
			}
			reload();
		});
	}

	void connect(const std::string &p_url, OnConnectCallback &&p_callback) override {
		socket->connect(p_url, std::move(p_callback));
	}

	void close(const std::string &p_reason) override {
		socket->close(p_reason);
	}

	void send(const std::string &p_message) override {
		socket->send(p_message);
	}

	void ping() override {
		socket->ping();
	}
};

} // namespace

std::optional<std::string> rn_inspector_reload_reply(const std::string &p_message) {
	try {
		const folly::dynamic message = folly::parseJson(p_message);
		const folly::dynamic *event = message.isObject() ? message.get_ptr("event") : nullptr;
		const folly::dynamic *payload = message.isObject() ? message.get_ptr("payload") : nullptr;
		if (!event || *event != "wrappedEvent" || !payload || !payload->isObject()) {
			return std::nullopt;
		}
		const folly::dynamic *wrapped = payload->get_ptr("wrappedEvent");
		const folly::dynamic *page = payload->get_ptr("pageId");
		if (!wrapped || !wrapped->isString() || !page) {
			return std::nullopt;
		}
		const folly::dynamic request = folly::parseJson(wrapped->getString());
		const folly::dynamic *method = request.isObject() ? request.get_ptr("method") : nullptr;
		const folly::dynamic *id = request.isObject() ? request.get_ptr("id") : nullptr;
		if (!method || *method != "Page.reload" || !id) {
			return std::nullopt;
		}
		const folly::dynamic response = folly::dynamic::object("id", *id)("result", folly::dynamic::object());
		return folly::toJson(folly::dynamic::object("event", "wrappedEvent")("payload", folly::dynamic::object("pageId", *page)("wrappedEvent", folly::toJson(response))));
	} catch (const std::exception &) {
		return std::nullopt;
	}
}

WebSocketClientFactory rn_inspector_web_socket_client_factory(WebSocketClientFactory p_factory, std::function<void()> p_reload) {
	return [factory = std::move(p_factory), reload = std::move(p_reload)]() -> std::unique_ptr<IWebSocketClient> {
		return std::make_unique<RNInspectorWebSocketClient>(factory(), reload);
	};
}

} // namespace facebook::react

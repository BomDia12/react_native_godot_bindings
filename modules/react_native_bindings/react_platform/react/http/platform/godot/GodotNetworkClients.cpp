#include "GodotNetworkClients.h"

#include "host/rn_main_thread_queue.h"
#include "native_modules/rn_http_service.h"

#include "modules/websocket/websocket_peer.h"

#include <folly/io/IOBuf.h>

namespace facebook::react {

namespace {

String godot_string(const std::string &p_value) {
	return String::utf8(p_value.c_str(), p_value.size());
}

class HttpRequestToken : public http::IRequestToken {
	std::weak_ptr<RNHTTPService> service_;
	std::weak_ptr<RNMainThreadQueue> mainThread_;
	std::shared_ptr<uint64_t> request_;

public:
	HttpRequestToken(std::weak_ptr<RNHTTPService> service, std::weak_ptr<RNMainThreadQueue> mainThread, std::shared_ptr<uint64_t> request) :
			service_(std::move(service)), mainThread_(std::move(mainThread)), request_(std::move(request)) {}

	void cancel() noexcept override {
		if (auto mainThread = mainThread_.lock()) {
			mainThread->post([service = service_, request = request_]() {
				if (auto http = service.lock(); http && *request != 0) {
					http->cancel(*request);
				}
			});
		}
	}
};

} // namespace

HttpClientFactory rn_http_client_factory(std::weak_ptr<RNHTTPService> http, std::weak_ptr<RNMainThreadQueue> mainThread) {
	return [http = std::move(http), mainThread = std::move(mainThread)]() {
		return std::make_unique<RNHttpClient>(http, mainThread);
	};
}

WebSocketClientFactory rn_web_socket_client_factory(std::weak_ptr<RNMainThreadQueue> mainThread) {
	return [mainThread = std::move(mainThread)]() {
		return std::make_unique<RNWebSocketClient>(mainThread);
	};
}

RNHttpClient::RNHttpClient(std::weak_ptr<RNHTTPService> service, std::weak_ptr<RNMainThreadQueue> mainThread) :
		service_(std::move(service)), mainThread_(std::move(mainThread)) {
}

std::unique_ptr<http::IRequestToken> RNHttpClient::sendRequest(http::NetworkCallbacks &&callbacks, const std::string &method, const std::string &url, const http::Headers &headers, const http::Body &body, uint32_t timeout, std::optional<std::string>) {
	auto request = std::make_shared<uint64_t>(0);
	RNHTTPRequest spec;
	spec.url = godot_string(url);
	spec.method = godot_string(method);
	spec.timeout_ms = timeout;
	for (const auto &[name, value] : headers) {
		spec.headers.push_back(godot_string(name) + ": " + godot_string(value));
	}
	if (body.string) {
		spec.body.resize(body.string->size());
		memcpy(spec.body.ptrw(), body.string->data(), body.string->size());
	}
	auto completed = [callbacks = std::move(callbacks)](RNHTTPResponse response) {
		if (response.error.is_set()) {
			if (callbacks.onResponseComplete) {
				callbacks.onResponseComplete(response.error.describe().utf8().get_data(), response.error.code == "E_TIMEOUT");
			}
			return;
		}
		if (callbacks.onResponse) {
			http::Headers response_headers;
			for (const String &line : response.headers) {
				const int separator = line.find(":");
				if (separator > 0) {
					response_headers.emplace_back(line.substr(0, separator).strip_edges().utf8().get_data(), line.substr(separator + 1).strip_edges().utf8().get_data());
				}
			}
			callbacks.onResponse(uint16_t(response.status), std::move(response_headers));
		}
		if (callbacks.onBody) {
			callbacks.onBody(folly::IOBuf::copyBuffer(response.body.ptr(), response.body.size()));
		}
		if (callbacks.onResponseComplete) {
			callbacks.onResponseComplete("", false);
		}
	};
	if (auto mainThread = mainThread_.lock()) {
		mainThread->post([service = service_, spec = std::move(spec), completed = std::move(completed), request]() mutable {
			auto http = service.lock();
			RNError error;
			if (http) {
				*request = http->submit(spec, completed, error);
			} else {
				error = RNError::make(RNErrorCode::UNSUPPORTED, "The Godot HTTP service is not running", "http.send");
			}
			if (*request == 0) {
				RNHTTPResponse failed;
				failed.error = error;
				completed(failed);
			}
		});
	}
	return std::make_unique<HttpRequestToken>(service_, mainThread_, request);
}

struct RNWebSocketClient::State {
	std::weak_ptr<RNMainThreadQueue> mainThread;
	Ref<WebSocketPeer> peer;
	OnConnectCallback onConnect;
	OnClosedCallback onClosed;
	OnMessageCallback onMessage;
	bool polling = false;
	bool disposed = false;

	void post(std::function<void(State &)> work, const std::shared_ptr<State> &self) {
		if (auto queue = mainThread.lock()) {
			queue->post([self, work = std::move(work)]() { work(*self); });
		}
	}

	void poll(const std::shared_ptr<State> &self) {
		if (peer.is_null()) {
			polling = false;
			return;
		}
		peer->poll();
		const WebSocketPeer::State ready = peer->get_ready_state();
		if (ready == WebSocketPeer::STATE_OPEN && onConnect) {
			std::exchange(onConnect, nullptr)(true, "");
		}
		while (ready == WebSocketPeer::STATE_OPEN && peer->get_available_packet_count() > 0) {
			const uint8_t *data = nullptr;
			int size = 0;
			if (peer->get_packet(&data, size) == OK && onMessage && !disposed) {
				onMessage(std::string(reinterpret_cast<const char *>(data), size));
			}
		}
		if (ready == WebSocketPeer::STATE_CLOSED) {
			const std::string reason = peer->get_close_reason().utf8().get_data();
			peer.unref();
			polling = false;
			if (onConnect) {
				std::exchange(onConnect, nullptr)(false, reason);
			} else if (onClosed && !disposed) {
				onClosed(reason);
			}
			return;
		}
		post([self](State &state) { state.poll(self); }, self);
	}
};

RNWebSocketClient::RNWebSocketClient(std::weak_ptr<RNMainThreadQueue> mainThread) :
		state_(std::make_shared<State>()) {
	state_->mainThread = std::move(mainThread);
}

RNWebSocketClient::~RNWebSocketClient() {
	state_->post([](State &state) {
		state.disposed = true;
		if (state.peer.is_valid()) {
			state.peer->close();
		}
	},
			state_);
}

void RNWebSocketClient::setOnClosedCallback(OnClosedCallback &&callback) noexcept {
	state_->post([callback = std::move(callback)](State &state) mutable { state.onClosed = std::move(callback); }, state_);
}

void RNWebSocketClient::setOnMessageCallback(OnMessageCallback &&callback) noexcept {
	state_->post([callback = std::move(callback)](State &state) mutable { state.onMessage = std::move(callback); }, state_);
}

void RNWebSocketClient::connect(const std::string &url, OnConnectCallback &&callback) {
	state_->post([url, callback = std::move(callback), self = state_](State &state) mutable {
		state.peer = Ref<WebSocketPeer>(WebSocketPeer::create());
		state.onConnect = std::move(callback);
		if (state.peer->connect_to_url(godot_string(url)) != OK) {
			state.peer.unref();
			if (state.onConnect) {
				std::exchange(state.onConnect, nullptr)(false, "Godot WebSocketPeer could not connect");
			}
			return;
		}
		if (!state.polling) {
			state.polling = true;
			state.poll(self);
		}
	},
			state_);
}

void RNWebSocketClient::close(const std::string &reason) {
	state_->post([reason](State &state) {
		if (state.peer.is_valid()) {
			state.peer->close(1000, godot_string(reason));
		}
	},
			state_);
}

void RNWebSocketClient::send(const std::string &message) {
	state_->post([message](State &state) {
		if (state.peer.is_valid() && state.peer->get_ready_state() == WebSocketPeer::STATE_OPEN) {
			state.peer->send_text(godot_string(message));
		}
	},
			state_);
}

void RNWebSocketClient::ping() {
}

} // namespace facebook::react

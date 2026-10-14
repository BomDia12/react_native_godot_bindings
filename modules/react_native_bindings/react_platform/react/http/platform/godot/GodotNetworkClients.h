#pragma once

#include <react/http/IHttpClient.h>
#include <react/http/IWebSocketClient.h>

#include <memory>

class RNHTTPService;
class RNMainThreadQueue;

namespace facebook::react {

// The factories that getHttpClientFactory() and getWebSocketClientFactory() return
// create clients over these services; ReactNativeHost configures them.
void configureGodotNetworkClients(std::weak_ptr<RNHTTPService> http, std::weak_ptr<RNMainThreadQueue> mainThread);

class RNHttpClient : public IHttpClient {
	std::weak_ptr<RNHTTPService> service_;
	std::weak_ptr<RNMainThreadQueue> mainThread_;

public:
	RNHttpClient(std::weak_ptr<RNHTTPService> service, std::weak_ptr<RNMainThreadQueue> mainThread);

	std::unique_ptr<http::IRequestToken> sendRequest(http::NetworkCallbacks &&callbacks, const std::string &method, const std::string &url, const http::Headers &headers, const http::Body &body, uint32_t timeout, std::optional<std::string> loggingId) override;
};

class RNWebSocketClient : public IWebSocketClient {
public:
	struct State;

private:
	std::shared_ptr<State> state_;

public:
	explicit RNWebSocketClient(std::weak_ptr<RNMainThreadQueue> mainThread);
	~RNWebSocketClient() override;

	void setOnClosedCallback(OnClosedCallback &&callback) noexcept override;
	void setOnMessageCallback(OnMessageCallback &&callback) noexcept override;
	void connect(const std::string &url, OnConnectCallback &&callback) override;
	void close(const std::string &reason) override;
	void send(const std::string &message) override;
	void ping() override;
};

} // namespace facebook::react

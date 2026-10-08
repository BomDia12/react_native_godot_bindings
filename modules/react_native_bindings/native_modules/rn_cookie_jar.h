#pragma once

#include "../interop/rn_error.h"

#include "core/variant/variant.h"

#include <vector>

struct RNParsedURL {
	String scheme;
	String host;
	int port = 0;
	String path;
	static bool parse(const String &p_url, RNParsedURL &r_url);
	static bool resolve(const String &p_base, const String &p_reference, String &r_url);
	String origin() const;
};
class RNCookieJar {
	struct Cookie {
		String name, value, domain, path;
		bool secure = false;
		double expires = 0;
		uint64_t order = 0;
	};
	std::vector<Cookie> cookies;
	uint64_t sequence = 0;
	size_t maximum_entries;
	uint64_t maximum_bytes;
	uint64_t bytes() const;

public:
	RNCookieJar(size_t p_entries, uint64_t p_bytes) :
			maximum_entries(p_entries), maximum_bytes(p_bytes) {}
	void receive(const String &p_url, const PackedStringArray &p_headers, double p_now);
	String header(const String &p_url, double p_now);
	bool clear() {
		bool had = !cookies.empty();
		cookies.clear();
		return had;
	}
	size_t count() const { return cookies.size(); }
};

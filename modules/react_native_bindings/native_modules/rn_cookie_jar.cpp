#include "rn_cookie_jar.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

bool RNParsedURL::parse(const String &p_url, RNParsedURL &r_url) {
	String fragment;
	if (p_url.parse_url(r_url.scheme, r_url.host, r_url.port, r_url.path, fragment) != OK) {
		return false;
	}
	r_url.scheme = r_url.scheme.trim_suffix("://").to_lower();
	r_url.host = r_url.host.to_lower();
	if (r_url.path.is_empty()) {
		r_url.path = "/";
	}
	if (r_url.port == 0) {
		r_url.port = r_url.scheme == "https" || r_url.scheme == "wss" ? 443 : 80;
	}
	return !r_url.host.is_empty() && !r_url.host.contains("@");
}
String RNParsedURL::origin() const {
	return scheme + "://" + host + ":" + itos(port);
}
uint64_t RNCookieJar::bytes() const {
	uint64_t total = 0;
	for (const auto &cookie : cookies) {
		total += cookie.name.utf8().length() + cookie.value.utf8().length() + cookie.domain.utf8().length() + cookie.path.utf8().length() + 64;
	}
	return total;
}
void RNCookieJar::receive(const String &p_url, const PackedStringArray &p_headers, double p_now) {
	RNParsedURL url;
	if (!RNParsedURL::parse(p_url, url)) {
		return;
	}
	for (const String &header : p_headers) {
		const int colon = header.find(":");
		if (colon < 0 || header.substr(0, colon).to_lower() != "set-cookie") {
			continue;
		}
		const PackedStringArray parts = header.substr(colon + 1).strip_edges().split(";");
		if (parts.is_empty()) {
			continue;
		}
		const int equal = parts[0].find("=");
		if (equal <= 0) {
			continue;
		}
		Cookie cookie;
		cookie.name = parts[0].substr(0, equal).strip_edges();
		cookie.value = parts[0].substr(equal + 1).strip_edges();
		cookie.domain = url.host;
		const String request_path = url.path.get_slice("?", 0);
		const int slash = request_path.rfind("/");
		cookie.path = slash > 0 ? request_path.substr(0, slash) : String("/");
		cookie.order = ++sequence;
		bool valid = true;
		bool max_age = false;
		for (int i = 1; i < parts.size(); ++i) {
			const String part = parts[i].strip_edges();
			const int split = part.find("=");
			const String name = (split < 0 ? part : part.substr(0, split)).to_lower();
			const String value = split < 0 ? String() : part.substr(split + 1).strip_edges();
			if (name == "secure") {
				cookie.secure = true;
			} else if (name == "domain") {
				cookie.domain = value.trim_prefix(".").to_lower();
				cookie.host_only = false;
				if (cookie.domain.is_empty() || (url.host != cookie.domain && !url.host.ends_with("." + cookie.domain)) || (!cookie.domain.contains(".") && cookie.domain != "localhost")) {
					valid = false;
				}
			} else if (name == "path" && value.begins_with("/")) {
				cookie.path = value;
			} else if (name == "max-age" && value.is_valid_int()) {
				const int64_t seconds = value.to_int();
				cookie.expires = seconds <= 0 ? -1 : p_now + double(seconds);
				max_age = true;
			} else if (name == "expires" && !max_age) {
				std::tm date{};
				std::istringstream input(value.utf8().get_data());
				input.imbue(std::locale::classic());
				input >> std::get_time(&date, "%a, %d %b %Y %H:%M:%S GMT");
				if (!input.fail()) {
					cookie.expires = double(timegm(&date));
				}
			}
		}
		if (cookie.name.contains("\n") || cookie.name.contains("\r") || cookie.value.contains("\n") || cookie.value.contains("\r") || !valid || (cookie.secure && url.scheme != "https")) {
			continue;
		}
		cookies.erase(std::remove_if(cookies.begin(), cookies.end(), [&](const Cookie &p_existing) { return p_existing.name == cookie.name && p_existing.domain == cookie.domain && p_existing.path == cookie.path; }), cookies.end());
		if (cookie.expires != 0 && cookie.expires <= p_now) {
			continue;
		}
		const uint64_t size = cookie.name.utf8().length() + cookie.value.utf8().length() + cookie.domain.utf8().length() + cookie.path.utf8().length() + 64;
		if (!maximum_entries || size > maximum_bytes) {
			continue;
		}
		while (!cookies.empty() && (cookies.size() >= maximum_entries || bytes() + size > maximum_bytes)) {
			cookies.erase(cookies.begin());
		}
		cookies.push_back(cookie);
	}
}
String RNCookieJar::header(const String &p_url, double p_now) {
	cookies.erase(std::remove_if(cookies.begin(), cookies.end(), [&](const Cookie &p_cookie) { return p_cookie.expires != 0 && p_cookie.expires <= p_now; }), cookies.end());
	RNParsedURL url;
	if (!RNParsedURL::parse(p_url, url)) {
		return String();
	}
	std::vector<const Cookie *> selected;
	const String path = url.path.get_slice("?", 0);
	for (const auto &cookie : cookies) {
		const bool domain = url.host == cookie.domain || (!cookie.host_only && url.host.ends_with("." + cookie.domain));
		const bool matches = path == cookie.path || (path.begins_with(cookie.path) && (cookie.path.ends_with("/") || path.substr(cookie.path.length(), 1) == "/"));
		if (domain && matches && (!cookie.secure || url.scheme == "https")) {
			selected.push_back(&cookie);
		}
	}
	std::stable_sort(selected.begin(), selected.end(), [](const Cookie *p_a, const Cookie *p_b) { return p_a->path.length() > p_b->path.length(); });
	String result;
	for (const auto *cookie : selected) {
		if (!result.is_empty()) {
			result += "; ";
		}
		result += cookie->name + "=" + cookie->value;
	}
	return result;
}

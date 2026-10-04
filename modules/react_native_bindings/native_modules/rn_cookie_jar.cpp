#include "rn_cookie_jar.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

bool RNParsedURL::parse(const String &p_url, RNParsedURL &r_url) {
	String source = p_url;
	const int authority_start = source.find("://") + 3;
	const int query = source.find("?", authority_start);
	const int slash = source.find("/", authority_start);
	const int hash = source.find("#", authority_start);
	if (query >= 0 && (slash < 0 || query < slash) && (hash < 0 || query < hash)) {
		source = source.insert(query, "/");
	}
	String fragment;
	if (source.parse_url(r_url.scheme, r_url.host, r_url.port, r_url.path, fragment) != OK) {
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
	return scheme + "://" + (host.contains(":") ? "[" + host + "]" : host) + ":" + itos(port);
}
bool RNParsedURL::resolve(const String &p_base, const String &p_reference, String &r_url) {
	RNParsedURL base;
	if (!parse(p_base, base)) {
		return false;
	}
	String reference = p_reference;
	String fragment;
	const int hash = reference.find("#");
	if (hash >= 0) {
		fragment = reference.substr(hash);
		reference = reference.substr(0, hash);
	}
	const int colon = reference.find(":");
	const int slash = reference.find("/");
	const int question = reference.find("?");
	String path;
	String authority = base.origin();
	if (reference.begins_with("//") || (colon >= 0 && (slash < 0 || colon < slash) && (question < 0 || colon < question))) {
		RNParsedURL absolute;
		if (!parse(reference.begins_with("//") ? base.scheme + ":" + reference : reference, absolute) || (absolute.scheme != "http" && absolute.scheme != "https")) {
			return false;
		}
		authority = absolute.origin();
		path = absolute.path;
	} else if (reference.is_empty()) {
		path = base.path;
	} else if (reference.begins_with("?")) {
		path = base.path.get_slice("?", 0) + reference;
	} else if (reference.begins_with("/")) {
		path = reference;
	} else {
		const String base_path = base.path.get_slice("?", 0);
		path = base_path.substr(0, base_path.rfind("/") + 1) + reference;
	}
	String query;
	const int query_start = path.find("?");
	if (query_start >= 0) {
		query = path.substr(query_start);
		path = path.substr(0, query_start);
	}
	String output;
	while (!path.is_empty()) {
		if (path.begins_with("../") || path.begins_with("./")) {
			path = path.substr(path.begins_with("../") ? 3 : 2);
		} else if (path.begins_with("/./") || path == "/.") {
			path = path == "/." ? String("/") : "/" + path.substr(3);
		} else if (path.begins_with("/../") || path == "/..") {
			path = path == "/.." ? String("/") : "/" + path.substr(4);
			output = output.substr(0, std::max(0, output.rfind("/")));
		} else if (path == "." || path == "..") {
			path = "";
		} else {
			const int end = path.find("/", path.begins_with("/") ? 1 : 0);
			output += end < 0 ? path : path.substr(0, end);
			path = end < 0 ? String() : path.substr(end);
		}
	}
	r_url = authority + (output.is_empty() ? String("/") : output) + query + fragment;
	return true;
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
		bool root_path_attribute = false;
		for (int i = 1; i < parts.size(); ++i) {
			const String part = parts[i].strip_edges();
			const int split = part.find("=");
			const String name = (split < 0 ? part : part.substr(0, split)).to_lower();
			const String value = split < 0 ? String() : part.substr(split + 1).strip_edges();
			if (name == "secure") {
				cookie.secure = true;
			} else if (name == "domain") {
				valid = false;
			} else if (name == "path") {
				root_path_attribute = value == "/";
				if (value.begins_with("/")) {
					cookie.path = value;
				}
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
		const String cookie_name = cookie.name.to_lower();
		if (((cookie_name.begins_with("__secure-") || cookie_name.begins_with("__host-")) && !cookie.secure) || (cookie_name.begins_with("__host-") && (!root_path_attribute || cookie.path != "/"))) {
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
		const bool domain = url.host == cookie.domain;
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

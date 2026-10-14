#include "rn_dynamic.h"

#include "core/variant/array.h"
#include "core/variant/dictionary.h"

Variant rn_variant_from_dynamic(const folly::dynamic &p_value) {
	switch (p_value.type()) {
		case folly::dynamic::BOOL:
			return p_value.getBool();
		case folly::dynamic::INT64:
			return p_value.getInt();
		case folly::dynamic::DOUBLE:
			return p_value.getDouble();
		case folly::dynamic::STRING:
			return String::utf8(p_value.getString().c_str());
		case folly::dynamic::ARRAY: {
			Array array;
			for (const folly::dynamic &item : p_value) {
				array.push_back(rn_variant_from_dynamic(item));
			}
			return array;
		}
		case folly::dynamic::OBJECT: {
			Dictionary dictionary;
			for (const auto &[key, value] : p_value.items()) {
				dictionary[String::utf8(key.asString().c_str())] = rn_variant_from_dynamic(value);
			}
			return dictionary;
		}
		default:
			return Variant();
	}
}

folly::dynamic rn_dynamic_from_variant(const Variant &p_value) {
	switch (p_value.get_type()) {
		case Variant::BOOL:
			return bool(p_value);
		case Variant::INT:
			return int64_t(p_value);
		case Variant::FLOAT:
			return double(p_value);
		case Variant::STRING:
		case Variant::STRING_NAME:
			return String(p_value).utf8().get_data();
		case Variant::ARRAY: {
			folly::dynamic array = folly::dynamic::array();
			for (const Variant &item : Array(p_value)) {
				array.push_back(rn_dynamic_from_variant(item));
			}
			return array;
		}
		case Variant::DICTIONARY: {
			folly::dynamic object = folly::dynamic::object();
			const Dictionary dictionary = p_value;
			for (const Variant &key : dictionary.keys()) {
				object[String(key).utf8().get_data()] = rn_dynamic_from_variant(dictionary[key]);
			}
			return object;
		}
		default:
			return nullptr;
	}
}

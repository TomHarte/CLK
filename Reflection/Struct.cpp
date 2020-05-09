//
//  Struct.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/03/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "Struct.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

// MARK: - Setters

template <> bool Reflection::set(Struct &target, const std::string &name, int value) {
	const auto target_type = target.type_of(name);
	if(!target_type) return false;

	// No need to convert an int or a registered enum.
	if(*target_type == typeid(int) || !Reflection::Enum::name(*target_type).empty()) {
		target.set(name, &value);
		return true;
	}

	// Promote to an int64_t.
	if(*target_type == typeid(int64_t)) {
		const auto int64 = int64_t(value);
		target.set(name, &int64);
		return true;
	}

	return false;
}

template <> bool Reflection::set(Struct &target, const std::string &name, const std::string &value) {
	const auto target_type = target.type_of(name);
	if(!target_type) return false;

	if(Reflection::Enum::name(*target_type).empty()) {
		return false;
	}

	const int enum_value = Reflection::Enum::from_string(*target_type, value);
	if(enum_value < 0) {
		return false;
	}
	target.set(name, &enum_value);

	return true;
}

template <> bool Reflection::set(Struct &target, const std::string &name, const char *value) {
	const std::string string(value);
	return set<const std::string &>(target, name, string);
}

template <> bool Reflection::set(Struct &target, const std::string &name, bool value) {
	const auto target_type = target.type_of(name);
	if(!target_type) return false;

	if(*target_type == typeid(bool)) {
		target.set(name, &value);;
	}

	return false;
}

// MARK: - Fuzzy setter

bool Reflection::fuzzy_set(Struct &target, const std::string &name, const std::string &value) {
	const auto target_type = target.type_of(name);
	if(!target_type) return false;

	// If the target is a registered enum, ttry to convert the value. Failing that,
	// try to match without case sensitivity.
	if(Reflection::Enum::size(*target_type)) {
		const int from_string = Reflection::Enum::from_string(*target_type, value);
		if(from_string >= 0) {
			target.set(name, &from_string);
			return true;
		}

		const auto all_values = Reflection::Enum::all_values(*target_type);
		const auto value_location = std::find_if(all_values.begin(), all_values.end(),
			[&value] (const auto &entry) {
				if(value.size() != entry.size()) return false;
				const char *v = value.c_str();
				const char *e = entry.c_str();
				while(*v) {
					if(tolower(*v) != tolower(*e)) return false;
					++v;
					++e;
				}
				return true;
			});
		if(value_location != all_values.end()) {
			const int offset = int(value_location - all_values.begin());
			target.set(name, &offset);
			return true;
		}

		return false;
	}

	return false;
}

// MARK: - Getters

template <typename Type> bool Reflection::get(const Struct &target, const std::string &name, Type &value) {
	const auto target_type = target.type_of(name);
	if(!target_type) return false;

	if(*target_type == typeid(Type)) {
		memcpy(&value, target.get(name), sizeof(Type));
		return true;
	}

	return false;
}

template <typename Type> Type Reflection::get(const Struct &target, const std::string &name) {
	Type value{};
	get(target, name, value);
	return value;
}

// MARK: - Description

std::string Reflection::Struct::description() const {
	std::ostringstream stream;

	stream << "{";

	bool is_first = true;
	for(const auto &key: all_keys()) {
		if(!is_first) stream << ", ";
		is_first = false;
		stream << key << ": ";

		const auto type = type_of(key);

		// Output Bools as yes/no.
		if(*type == typeid(bool)) {
			stream << ::Reflection::get<bool>(*this, key);
			continue;
		}

		// Output Ints of all sizes as hex.
#define OutputIntC(int_type, cast_type) if(*type == typeid(int_type)) { stream << std::setfill('0') << std::setw(sizeof(int_type)*2) << std::hex << cast_type(::Reflection::get<int_type>(*this, key)); continue; }
#define OutputInt(int_type) OutputIntC(int_type, int_type)
		OutputIntC(int8_t, int16_t);
		OutputIntC(uint8_t, uint16_t);
		OutputInt(int16_t);
		OutputInt(uint16_t);
		OutputInt(int32_t);
		OutputInt(uint32_t);
		OutputInt(int64_t);
		OutputInt(uint64_t);
#undef OutputInt

		// Output floats and strings natively.
#define OutputNative(val_type) if(*type == typeid(val_type)) { stream << ::Reflection::get<val_type>(*this, key); continue; }
		OutputNative(float);
		OutputNative(double);
		OutputNative(char *);
		OutputNative(std::string);
#undef OutputNAtive

		// Output the current value of any enums.
		if(!Enum::name(*type).empty()) {
			const int value = ::Reflection::get<int>(*this, key);
			stream << Enum::to_string(*type, value);
			continue;
		}

		// Recurse to deal with embedded objects.
		if(*type == typeid(Reflection::Struct)) {
			const Reflection::Struct *const child = reinterpret_cast<const Reflection::Struct *>(get(key));
			stream << child->description();
			continue;
		}
	}

	stream << "}";

	return stream.str();
}

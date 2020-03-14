//
//  Struct.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/03/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "Struct.h"

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

	const auto enum_value = Reflection::Enum::from_string(*target_type, value);
	if(enum_value == std::string::npos) {
		return false;
	}

	int int_value = int(enum_value);
	target.set(name, &int_value);

	return true;
}

template <> bool Reflection::set(Struct &target, const std::string &name, const char *value) {
	const std::string string(value);
	return set<const std::string &>(target, name, string);
}

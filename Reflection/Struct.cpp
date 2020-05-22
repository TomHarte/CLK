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
#include <iterator>
#include <sstream>
#include <type_traits>

#define ForAllInts(x)	\
	x(uint8_t);			\
	x(int8_t);			\
	x(uint16_t);		\
	x(int16_t);			\
	x(uint32_t);		\
	x(int32_t);			\
	x(uint64_t);		\
	x(int64_t);

#define ForAllFloats(x)	\
	x(float);			\
	x(double);

namespace TypeInfo {

static bool is_integral(const std::type_info *type) {
	return
		*type == typeid(uint8_t) || *type == typeid(int8_t) ||
		*type == typeid(uint16_t) || *type == typeid(int16_t) ||
		*type == typeid(uint32_t) || *type == typeid(int32_t) ||
		*type == typeid(uint64_t) || *type == typeid(int64_t);
}

static bool is_floating_point(const std::type_info *type) {
	return *type == typeid(float) || *type == typeid(double);
}

static size_t size(const std::type_info *type) {
#define TestType(x)	if(*type == typeid(x)) return sizeof(x);
	ForAllInts(TestType);
	ForAllFloats(TestType);
	TestType(char *);
#undef TestType

	// This is some sort of struct or object type.
	return 0;
}

}

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

template <typename Type> bool Reflection::get(const Struct &target, const std::string &name, Type &value, size_t offset) {
	const auto target_type = target.type_of(name);
	if(!target_type) return false;

	// If type is a direct match, copy.
	if(*target_type == typeid(Type)) {
		memcpy(&value, reinterpret_cast<const uint8_t *>(target.get(name)) + offset * sizeof(Type), sizeof(Type));
		return true;
	}

	// If the type is a registered enum and the value type is int, copy.
	if constexpr (std::is_integral<Type>::value && sizeof(Type) == sizeof(int)) {
		if(!Enum::name(*target_type).empty()) {
			memcpy(&value, target.get(name), sizeof(int));
			return true;
		}
	}

	// If the type is an int that is larger than the stored type, cast upward.
	if constexpr (std::is_integral<Type>::value) {
		constexpr size_t size = sizeof(Type);
		const bool target_is_integral = TypeInfo::is_integral(target_type);
		const size_t target_size = TypeInfo::size(target_type);

		if(size > target_size && target_is_integral) {
#define Map(x)	if(*target_type == typeid(x)) { value = static_cast<Type>(*reinterpret_cast<const x *>(target.get(name))); }
			ForAllInts(Map);
#undef Map
			return true;
		}
	}

	// If the type is a double and stored type is a float, cast upward.
	if constexpr (std::is_floating_point<Type>::value) {
		constexpr size_t size = sizeof(Type);
		const bool target_is_floating_point = TypeInfo::is_floating_point(target_type);
		const size_t target_size = TypeInfo::size(target_type);

		if(size > target_size && target_is_floating_point) {
#define Map(x)	if(*target_type == typeid(x)) { value = static_cast<Type>(*reinterpret_cast<const x *>(target.get(name))); }
			ForAllFloats(Map);
#undef Map
			return true;
		}
	}

	return false;
}

template <typename Type> Type Reflection::get(const Struct &target, const std::string &name, size_t offset) {
	Type value{};
	get(target, name, value, offset);
	return value;
}

// MARK: - Description

void Reflection::Struct::append(std::ostringstream &stream, const std::string &key, const std::type_info *const type, size_t offset) const {
	// Output Bools as yes/no.
	if(*type == typeid(bool)) {
		stream << ::Reflection::get<bool>(*this, key, offset);
		return;
	}

	// Output Ints of all sizes as hex.
#define OutputIntC(int_type, cast_type) if(*type == typeid(int_type)) { stream << std::setfill('0') << std::setw(sizeof(int_type)*2) << std::hex << cast_type(::Reflection::get<int_type>(*this, key, offset)); return; }
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
#undef OutputIntC

		// Output floats and strings natively.
#define OutputNative(val_type) if(*type == typeid(val_type)) { stream << ::Reflection::get<val_type>(*this, key, offset); return; }
	OutputNative(float);
	OutputNative(double);
	OutputNative(char *);
	OutputNative(std::string);
#undef OutputNative

	// Output the current value of any enums.
	if(!Enum::name(*type).empty()) {
		const int value = ::Reflection::get<int>(*this, key, offset);
		stream << Enum::to_string(*type, value);
		return;
	}

	// Recurse to deal with embedded objects.
	if(*type == typeid(Reflection::Struct)) {
		const Reflection::Struct *const child = reinterpret_cast<const Reflection::Struct *>(get(key));
		stream << child->description();
		return;
	}
}

std::string Reflection::Struct::description() const {
	std::ostringstream stream;

	stream << "{";

	bool is_first = true;
	for(const auto &key: all_keys()) {
		if(!is_first) stream << ", ";
		is_first = false;
		stream << key << ": ";

		const auto count = count_of(key);
		const auto type = type_of(key);

		if(count != 1) {
			stream << "[";
		}

		for(size_t index = 0; index < count; ++index) {
			append(stream, key, type, index);
			if(index != count-1) stream << ", ";
		}

		if(count != 1) {
			stream << "]";
		}
	}

	stream << "}";

	return stream.str();
}

std::vector<uint8_t> Reflection::Struct::serialise() const {
	std::vector<uint8_t> result;

	/* Contractually, this serialises as BSON. */

	for(const auto &key: all_keys()) {
		if(!should_serialise(key)) continue;

		/* Here:	e_list	::=	element e_list | ""		*/
		const auto count = count_of(key);
		const auto type = type_of(key);

		if(count > 1) {
			// TODO: Arrays.
		} else {
			auto push_name = [&result, &key] () {
				std::copy(key.begin(), key.end(), std::back_inserter(result));
				result.push_back(0);
			};

			auto push_int = [push_name, &result] (uint8_t type, auto x) {
				result.push_back(type);
				push_name();
				for(size_t c = 0; c < sizeof(x); ++c)
					result.push_back(uint8_t((x) >> (8 * c)));
			};

			// Test for an exact match on Booleans.
			if(*type == typeid(bool)) {
				result.push_back(0x08);
				push_name();
				result.push_back(uint8_t(Reflection::get<bool>(*this, key)));
				continue;
			}

			// Test for ints that will safely convert to an int32.
			int32_t int32;
			if(Reflection::get(*this, key, int32)) {
				push_int(0x10, int32);
				continue;
			}

			// Test for ints that can be converted to a uint64.
			uint32_t uint64;
			if(Reflection::get(*this, key, uint64)) {
				push_int(0x11, uint64);
				continue;
			}

			// Test for ints that can be converted to an int64.
			int32_t int64;
			if(Reflection::get(*this, key, int64)) {
				push_int(0x12, int64);
				continue;
			}


			/*	All ints should now be dealt with.	*/

			// There's only one potential float type: a double.
			double float64;
			if(Reflection::get(*this, key, float64)) {
				// TODO: place as little-endian IEEE 754-2008.
				continue;
			}

			// Okay, check for a potential recursion.
			if(*type == typeid(Reflection::Struct)) {
				result.push_back(0x03);
				push_name();

				const Reflection::Struct *const child = reinterpret_cast<const Reflection::Struct *>(get(key));
				const auto sub_document = child->serialise();
				std::copy(sub_document.begin(), sub_document.end(), std::back_inserter(result));
				continue;
			}

			// Should never reach here; that means a type was discovered in a struct which is intended for
			// serialisation but which could not be parsed.
			assert(false);
		}
	}

	/*
		document ::= int32 e_list "\x00"
		The int32 is the total number of bytes comprising the document.
	*/
	result.push_back(0);
	const uint32_t size_with_prefix = uint32_t(result.size()) + 2;
	result.insert(result.begin(), uint8_t(size_with_prefix >> 8));
	result.insert(result.begin(), uint8_t(size_with_prefix & 0xff));

	return result;
}

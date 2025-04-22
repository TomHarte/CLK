//
//  Struct.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/03/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "Struct.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <type_traits>

// MARK: - Setters

template <> bool Reflection::set(Struct &target, const std::string &name, const float value, const size_t offset) {
	const auto target_type = target.type_of(name);
	if(!target_type) return false;

	if(*target_type == typeid(float)) {
		target.set(name, &value, offset);
		return true;
	}

	return set<double>(target, name, value);
}

template <> bool Reflection::set(Struct &target, const std::string &name, const double value, const size_t offset) {
	const auto target_type = target.type_of(name);
	if(!target_type) return false;

	if(*target_type == typeid(double)) {
		target.set(name, &value, offset);
		return true;
	}

	if(*target_type == typeid(float)) {
		const float float_value = float(value);
		target.set(name, &float_value, offset);
		return true;
	}

	return false;
}

template <> bool Reflection::set(Struct &target, const std::string &name, const int value, const size_t offset) {
	return set<int64_t>(target, name, value, offset);
}

template <> bool Reflection::set(Struct &target, const std::string &name, const int64_t value, const size_t offset) {
	const auto target_type = target.type_of(name);
	if(!target_type) return false;

	// No need to convert an int or a registered enum.
	if(*target_type == typeid(int) || !Reflection::Enum::name(*target_type).empty()) {
		const int value32 = int(value);
		target.set(name, &value32, offset);
		return true;
	}

	// Set an int64_t directly.
	if(*target_type == typeid(int64_t)) {
		target.set(name, &value, offset);
		return true;
	}

#define SetInt(x)	if(*target_type == typeid(x)) { \
	const auto truncated_value = x(value); \
	target.set(name, &truncated_value, offset); \
}
	ForAllInts(SetInt);
#undef SetInt

	return false;
}

template <> bool Reflection::set(
	Struct &target,
	const std::string &name,
	const std::string &value,
	const size_t offset
) {
	const auto target_type = target.type_of(name);
	if(!target_type) return false;

	// If the target is a string, assign.
	if(*target_type == typeid(std::string)) {
		auto child = reinterpret_cast<std::string *>(target.get(name));
		*child = value;
		return true;
	}

	// From here on, make an attempt to convert to a named enum.
	if(Reflection::Enum::name(*target_type).empty()) {
		return false;
	}

	const int enum_value = Reflection::Enum::from_string(*target_type, value);
	if(enum_value < 0) {
		return false;
	}
	target.set(name, &enum_value, offset);

	return true;
}

template <> bool Reflection::set(Struct &target, const std::string &name, const char *value, const size_t offset) {
	const std::string string(value);
	return set<const std::string &>(target, name, string, offset);
}

template <> bool Reflection::set(Struct &target, const std::string &name, bool value, const size_t offset) {
	const auto target_type = target.type_of(name);
	if(!target_type) return false;

	if(*target_type == typeid(bool)) {
		target.set(name, &value, offset);
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


// MARK: - Description

namespace {

template <typename IntT>
bool append_int(
	const Reflection::Struct &target,
	std::ostringstream &stream,
	const std::string &key,
	const std::type_info *const type,
	const size_t offset
) {
	if(*type != typeid(IntT)) {
		return false;
	}

	stream << std::setfill('0') << std::setw(sizeof(IntT)*2);
	stream << std::hex << +Reflection::get<IntT>(target, key, offset);
	return true;
}

template <typename ValT>
bool append_native(
	const Reflection::Struct &target,
	std::ostringstream &stream,
	const std::string &key,
	const std::type_info *const type,
	const size_t offset
) {
	if(*type != typeid(ValT)) {
		return false;
	}

	stream << Reflection::get<ValT>(target, key, offset);
	return true;
}

}

void Reflection::Struct::append(
	std::ostringstream &stream,
	const std::string &key,
	const std::type_info *const type,
	const size_t offset
) const {
	// Output Bools as yes/no.
	if(*type == typeid(bool)) {
		stream << Reflection::get<bool>(*this, key, offset);
		return;
	}

	// Output Ints of all sizes as hex.
	if(append_int<int8_t>(*this, stream, key, type, offset)) return;
	if(append_int<uint8_t>(*this, stream, key, type, offset)) return;
	if(append_int<int16_t>(*this, stream, key, type, offset)) return;
	if(append_int<uint16_t>(*this, stream, key, type, offset)) return;
	if(append_int<int32_t>(*this, stream, key, type, offset)) return;
	if(append_int<uint32_t>(*this, stream, key, type, offset)) return;
	if(append_int<int64_t>(*this, stream, key, type, offset)) return;
	if(append_int<uint64_t>(*this, stream, key, type, offset)) return;

	// Output floats and strings natively.
	if(append_native<float>(*this, stream, key, type, offset)) return;
	if(append_native<double>(*this, stream, key, type, offset)) return;
	if(append_native<char *>(*this, stream, key, type, offset)) return;
	if(append_native<std::string>(*this, stream, key, type, offset)) return;

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

/* Contractually, this serialises as BSON. */
std::vector<uint8_t> Reflection::Struct::serialise() const {
	auto push_name = [] (std::vector<uint8_t> &result, const std::string &name) {
		std::copy(name.begin(), name.end(), std::back_inserter(result));
		result.push_back(0);
	};

	auto append = [push_name, this] (
		std::vector<uint8_t> &result,
		const std::string &key,
		const std::string &output_name,
		const std::type_info *type,
		const size_t offset
	) {
		const auto push_int = [&result] (auto x) {
			for(size_t c = 0; c < sizeof(x); ++c)
				result.push_back(uint8_t((x) >> (8 * c)));
		};

		auto push_named_int = [push_int, push_name, &result, &output_name] (const uint8_t type, const auto x) {
			result.push_back(type);
			push_name(result, output_name);
			push_int(x);
		};

		auto push_string = [push_int, push_name, &result, &output_name] (const std::string &text) {
			result.push_back(0x02);
			push_name(result, output_name);

			const uint32_t string_length = uint32_t(text.size() + 1);
			push_int(string_length);
			std::copy(text.begin(), text.end(), std::back_inserter(result));
			result.push_back(0);
		};

		// Test for an exact match on Booleans.
		if(*type == typeid(bool)) {
			result.push_back(0x08);
			push_name(result, output_name);
			result.push_back(uint8_t(Reflection::get<bool>(*this, key, offset)));
			return;
		}

		// Record the string value for enums.
		if(!Reflection::Enum::name(*type).empty()) {
			int value;
			Reflection::get(*this, key, value, offset);
			const auto text = Reflection::Enum::to_string(*type, 0);
			push_string(text);
			return;
		}

		// Test for ints that will safely convert to an int32.
		int32_t int32;
		if(Reflection::get(*this, key, int32, offset)) {
			push_named_int(0x10, int32);
			return;
		}

		// Test for ints that can be converted to an int64.
		int64_t int64;
		if(Reflection::get(*this, key, int64, offset)) {
			push_named_int(0x12, int64);
			return;
		}

		// There's only one BSON float type: a double.
		double float64;
		if(Reflection::get(*this, key, float64, offset)) {
			result.push_back(0x01);
			push_name(result, output_name);

			// The following declines to assume an internal representation
			// for doubles, constructing IEEE 708 from first principles.
			// Which is probably absurd given how often I've assumed
			// e.g. two's complement.
			int exponent;
			const double mantissa = frexp(fabs(float64), &exponent);
			exponent += 1022;
			const uint64_t integer_mantissa =
				static_cast<uint64_t>(mantissa * 9007199254740992.0);
			const uint64_t binary64 =
				((float64 < 0) ? 0x8000'0000'0000'0000 : 0) |
				(integer_mantissa & 0x000f'ffff'ffff'ffff) |
				(static_cast<uint64_t>(exponent) << 52);
			push_int(binary64);

			return;
		}

		// Strings are written naturally.
		if(*type == typeid(std::string)) {
			const uint8_t *address = reinterpret_cast<const uint8_t *>(get(key));
			const std::string *const text = reinterpret_cast<const std::string *>(address + offset*sizeof(std::string));
			push_string(*text);
			return;
		}

		// Store std::vector<uint_8>s as binary data.
		if(*type == typeid(std::vector<uint8_t>)) {
			result.push_back(0x05);
			push_name(result, output_name);

			auto source = reinterpret_cast<const std::vector<uint8_t> *>(get(key));
			push_int(uint32_t(source->size()));
			result.push_back(0x00);
			std::copy(source->begin(), source->end(), std::back_inserter(result));
			return;
		}

		// Okay, check for a potential recursion.
		// Not currently supported: arrays of structs.
		if(*type == typeid(Reflection::Struct)) {
			result.push_back(0x03);
			push_name(result, output_name);

			const Reflection::Struct *const child = reinterpret_cast<const Reflection::Struct *>(get(key));
			const auto sub_document = child->serialise();
			std::copy(sub_document.begin(), sub_document.end(), std::back_inserter(result));
			return;
		}

		// Should never reach here; that means a type was discovered in a struct which is intended for
		// serialisation but which could not be parsed.
		assert(false);
	};

	const auto wrap_object = [] (std::vector<uint8_t> &data) {
		/*
			document ::= int32 e_list "\x00"
			The int32 is the total number of bytes comprising the document.
		*/
		data.push_back(0);
		const uint32_t size_with_prefix = uint32_t(data.size()) + 4;
		data.insert(data.begin(), uint8_t(size_with_prefix >> 24));
		data.insert(data.begin(), uint8_t(size_with_prefix >> 16));
		data.insert(data.begin(), uint8_t(size_with_prefix >> 8));
		data.insert(data.begin(), uint8_t(size_with_prefix & 0xff));
	};

	std::vector<uint8_t> result;

	for(const auto &key: all_keys()) {
		if(!should_serialise(key)) continue;

		/* Here:	e_list	::=	element e_list | ""		*/
		const auto count = count_of(key);
		const auto type = type_of(key);

		if(count > 1) {
			// In BSON, an array is a sub-document with ASCII keys '0', '1', etc.
			result.push_back(0x04);
			push_name(result, key);

			std::vector<uint8_t> array;
			for(size_t c = 0; c < count; ++c) {
				append(array, key, std::to_string(c), type, c);
			}
			wrap_object(array);

			std::copy(array.begin(), array.end(), std::back_inserter(result));
		} else {
			append(result, key, key, type, 0);
		}
	}

	wrap_object(result);
	return result;
}

bool Reflection::Struct::deserialise(const std::vector<uint8_t> &bson) {
	return deserialise(bson.data(), bson.size());
}

namespace {

/*!
	Provides a proxy struct that redirects calls to set to another object and property, picking
	an offset based on the propety name specified here.
*/
struct ArrayReceiver: public Reflection::Struct {
	ArrayReceiver(
		Reflection::Struct *const target,
		const std::type_info *type,
		const std::string &key,
		const size_t count
	) :
		target_(target), type_(type), key_(key), count_(count) {}

	std::vector<std::string> all_keys() const final { return {}; }
	const std::type_info *type_of(const std::string &) const final { return type_; }
	size_t count_of(const std::string &) const final { return 0; }

	void set(const std::string &name, const void *value, size_t) final {
		const auto index = size_t(std::stoi(name));
		if(index >= count_) {
			return;
		}
		target_->set(key_, value, index);
	}

	virtual std::vector<std::string> values_for(const std::string &) const final {
		return {};
	}

	void *get(const std::string &) final {
		return nullptr;
	}

private:
	Reflection::Struct *target_;
	const std::type_info *type_;
	std::string key_;
	size_t count_;
};

}

bool Reflection::Struct::deserialise(const uint8_t *bson, const size_t size) {
	// Validate the object's declared size.
	const auto end = bson + size;
	auto read_int = [&bson] (auto &target) {
		constexpr auto shift = 8 * (sizeof(target) - 1);
		target = 0;
		for(size_t c = 0; c < sizeof(target); ++c) {
			target >>= 8;
			target |= decltype(target)(*bson) << shift;
			++bson;
		}
	};

	uint32_t object_size;
	read_int(object_size);
	if(object_size > size) return false;

	while(true) {
		const uint8_t next_type = *bson;
		++bson;
		if(!next_type)
			break;

		std::string key;
		while(*bson) {
			key.push_back(char(*bson));
			++bson;
		}
		++bson;

		switch(next_type) {
			default:
				return false;

			// 0x03: A subdocument; try to install the inner BSON.
			// 0x05: Binary data. Seek to populate a std::vector<uint8_t>.
			case 0x03:
			case 0x05: {
				const auto type = type_of(key);

				uint32_t subobject_size;
				read_int(subobject_size);

				if(next_type == 0x03 && *type == typeid(Reflection::Struct)) {
					auto child = reinterpret_cast<Reflection::Struct *>(get(key));
					child->deserialise(bson - 4, size_t(end - bson + 4));
					bson += subobject_size - 4;
				}

				if(next_type == 0x05 && *type == typeid(std::vector<uint8_t>)) {
					auto child = reinterpret_cast<std::vector<uint8_t> *>(get(key));
					*child = std::vector<uint8_t>(bson, bson + subobject_size);
					bson += subobject_size;
				}
			} break;

			// Array. BSON's encoding of these is a minor pain, but could be worse;
			// they're presented as a subobject with objects serialised in array order
			// but given the string keys "0", "1", etc. So: validate the keys, decode
			// the objects.
			case 0x04: {
				ArrayReceiver receiver(this, type_of(key), key, count_of(key));

				uint32_t subobject_size;
				read_int(subobject_size);

				receiver.deserialise(bson - 4, size_t(end - bson + 4));

				bson += subobject_size - 4;
			} break;

			// String.
			case 0x02: {
				uint32_t length;
				read_int(length);

				const std::string value(bson, bson + length - 1);
				::Reflection::set<const std::string &>(*this, key, value);

				bson += length;
			} break;

			// Boolean.
			case 0x08: {
				const bool value = *bson;
				++bson;
				::Reflection::set(*this, key, value);
			} break;

			// 32-bit int.
			case 0x10: {
				int32_t value;
				read_int(value);
				::Reflection::set(*this, key, value);
			} break;

			// 64-bit int.
			case 0x12: {
				int64_t value;
				read_int(value);
				::Reflection::set(*this, key, value);
			} break;

			// 64-bit double.
			case 0x01: {
				uint64_t value;
				read_int(value);

				const double mantissa = 0.5 + double(value & 0x000f'ffff'ffff'ffff) / 9007199254740992.0;
				const int exponent = ((value >> 52) & 2047) - 1022;
				const double double_value = ldexp(mantissa, exponent);
				const double sign = (value & 0x8000'0000'0000'0000) ? -1 : 1;

				::Reflection::set(*this, key, double_value * sign);
			} break;
		}
	}

	return true;
}

//
//  Struct.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/03/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Struct_hpp
#define Struct_hpp

#include <cassert>
#include <cstdarg>
#include <cstring>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Enum.hpp"

namespace Reflection {

#define DeclareField(Name) declare(&Name, #Name)

struct Struct {
	virtual std::vector<std::string> all_keys() const = 0;
	virtual const std::type_info *type_of(const std::string &name) const = 0;
	virtual size_t count_of(const std::string &name) const = 0;
	virtual void set(const std::string &name, const void *value, size_t offset = 0) = 0;
	virtual void *get(const std::string &name) = 0;
	virtual const void *get(const std::string &name) const {
		return const_cast<Struct *>(this)->get(name);
	}
	virtual std::vector<std::string> values_for(const std::string &name) const = 0;
	virtual ~Struct() {}

	/*!
		@returns A string describing this struct. This string has no guaranteed layout, may not be
			sufficiently formed for a formal language parser, etc.
	*/
	std::string description() const;

	/*!
		Serialises this struct in BSON format.

		Supported field types:

			* [u]int[8/16/32/64]_t;
			* float and double;
			* bool;
			* std::string;
			* plain C-style arrays of any of the above;
			* other reflective structs;
			* std::vector<uint8_t> as raw binary data.

		TODO: vector of string, possibly? Or more general vector support?

		@returns The BSON serialisation.
	*/
	std::vector<uint8_t> serialise() const;

	/*!
		Applies as many fields as possible from the incoming BSON. Supports the same types
		as @c serialise.
	*/
	bool deserialise(const std::vector<uint8_t> &bson);

	/*!
		Called to determine whether @c key should be included in the serialisation of this struct.
	*/
	virtual bool should_serialise([[maybe_unused]] const std::string &key) const { return true; }

	private:
		void append(std::ostringstream &stream, const std::string &key, const std::type_info *type, size_t offset) const;
		bool deserialise(const uint8_t *bson, size_t size);
};

/*!
	Attempts to set the property @c name to @c value ; will perform limited type conversions.

	@returns @c true if the property was successfully set; @c false otherwise.
*/
template <typename Type> bool set(Struct &target, const std::string &name, Type value, size_t offset = 0);

/*!
	Setting an int:

		* to an int copies the int;
		* to a smaller type, truncates the int;
		* to an int64_t promotes the int; and
		* to a registered enum, copies the int.
*/
template <> bool set(Struct &target, const std::string &name, int64_t value, size_t offset);
template <> bool set(Struct &target, const std::string &name, int value, size_t offset);

/*!
	Setting a string:

		* to an enum, if the string names a member of the enum, sets the value.
*/
template <> bool set(Struct &target, const std::string &name, const std::string &value, size_t offset);
template <> bool set(Struct &target, const std::string &name, const char *value, size_t offset);

/*!
	Setting a bool:

		* to a bool, copies the value.
*/
template <> bool set(Struct &target, const std::string &name, bool value, size_t offset);


template <> bool set(Struct &target, const std::string &name, float value, size_t offset);
template <> bool set(Struct &target, const std::string &name, double value, size_t offset);

/*!
	Fuzzy-set attempts to set any property based on a string value. This is intended to allow input provided by the user.

	Amongst other steps, it might:
		* if the target is a bool, map true, false, yes, no, y, n, etc;
		* if the target is an integer, parse like strtrol;
		* if the target is a float, parse like strtod; or
		* if the target is a reflective enum, attempt to match to enum members (possibly doing so in a case insensitive fashion).

	This method reserves the right to perform more or fewer attempted mappings, using any other logic it
	decides is appropriate.

@returns @c true if the property was successfully set; @c false otherwise.
*/
bool fuzzy_set(Struct &target, const std::string &name, const std::string &value);


/*!
	Attempts to get the property @c name to @c value ; will perform limited type conversions.

	@returns @c true if the property was successfully read; @c false otherwise.
*/
template <typename Type> bool get(const Struct &target, const std::string &name, Type &value, size_t offset = 0);

/*!
	Attempts to get the property @c name to @c value ; will perform limited type conversions.

	@returns @c true if the property was successfully read; a default-constructed instance of Type otherwise.
*/
template <typename Type> Type get(const Struct &target, const std::string &name, size_t offset = 0);

template <typename Owner> class StructImpl: public Struct {
	public:
		/*!
			@returns the value of type @c Type that is loaded from the offset registered for the field @c name.
				It is the caller's responsibility to provide an appropriate type of data.
		*/
		void *get(const std::string &name) final {
			const auto iterator = contents_.find(name);
			if(iterator == contents_.end()) return nullptr;
			return reinterpret_cast<uint8_t *>(this) + iterator->second.offset;
		}

		/*!
			Stores the @c value of type @c Type to the offset registered for the field @c name.

			It is the caller's responsibility to provide an appropriate type of data.
		*/
		void set(const std::string &name, const void *value, size_t offset) final {
			const auto iterator = contents_.find(name);
			if(iterator == contents_.end()) return;
			assert(offset < iterator->second.count);
			memcpy(reinterpret_cast<uint8_t *>(this) + iterator->second.offset + offset * iterator->second.size, value, iterator->second.size);
		}

		/*!
			@returns @c type_info for the field @c name.
		*/
		const std::type_info *type_of(const std::string &name) const final {
			const auto iterator = contents_.find(name);
			if(iterator == contents_.end()) return nullptr;
			return iterator->second.type;
		}

		/*!
			@returns The number of instances of objects of the same type as @c name that sit consecutively in memory.
		*/
		size_t count_of(const std::string &name) const final {
			const auto iterator = contents_.find(name);
			if(iterator == contents_.end()) return 0;
			return iterator->second.count;
		}

		/*!
			@returns a list of the valid enum value names for field @c name if it is a declared enum field of this struct;
				the empty list otherwise.
		*/
		std::vector<std::string> values_for(const std::string &name) const final {
			std::vector<std::string> result;

			// Return an empty vector if this field isn't declared.
			const auto type = type_of(name);
			if(!type) return result;

			// Also return an empty vector if this field isn't a registered enum.
			const auto all_values = Enum::all_values(*type);
			if(all_values.empty()) return result;

			// If no restriction is stored, return all values.
			const auto permitted_values = permitted_enum_values_.find(name);
			if(permitted_values == permitted_enum_values_.end()) return all_values;

			// Compile a vector of only those values the stored set indicates.
			auto value = all_values.begin();
			auto flag = permitted_values->second.begin();
			while(value != all_values.end() && flag != permitted_values->second.end()) {
				if(*flag) {
					result.push_back(*value);
				}
				++flag;
				++value;
			}

			return result;
		}

		/*!
			@returns A vector of all declared fields for this struct.
		*/
		std::vector<std::string> all_keys() const final {
			std::vector<std::string> keys;
			for(const auto &pair: contents_) {
				keys.push_back(pair.first);
			}
			return keys;
		}

	protected:
		/*
			This interface requires reflective structs to declare all fields;
			specifically they should call:

			declare_field(&field1, "field1");
			declare_field(&field2, "field2");

			Fields are registered in class storage. So callers can use needs_declare()
			to determine whether a class of this type has already established the
			reflective fields.
		*/

		/*!
			Exposes the field pointed to by @c t for reflection as @c name. If @c t is itself a Reflection::Struct,
			it'll be the struct that's exposed.
		*/
		template <typename Type> void declare(Type *t, const std::string &name) {
			// If the declared item is a class, see whether it can be dynamically cast
			// to a reflectable for emplacement. If so, exit early.
			if constexpr (std::is_class<Type>()) {
				if(declare_reflectable(t, name)) return;
			}

			// If the declared item is an array, record it as a pointer to the
			// first element plus a size.
			if constexpr (std::is_array<Type>()) {
				declare_emplace(&(*t)[0], name, sizeof(*t) / sizeof(*t[0]));
				return;
			}

			declare_emplace(t, name);
		}

		/*!
			If @c t is a previously-declared field that links to a declared enum then the variable
			arguments provide a list of the acceptable values for that field. The list should be terminated
			with a value of -1.
		*/
		template <typename Type> void limit_enum(Type *t, ...) {
			const auto name = name_of(t);
			if(name.empty()) return;

			// The default vector size of '8' isn't especially scientific,
			// but I feel like it's a good choice.
			std::vector<bool> permitted_values(8);

			va_list list;
			va_start(list, t);
			while(true) {
				const int next = va_arg(list, int);
				if(next < 0) break;

				if(permitted_values.size() <= size_t(next)) {
					permitted_values.resize(permitted_values.size() << 1);
				}
				permitted_values[size_t(next)] = true;
			}
			va_end(list);

			permitted_enum_values_.emplace(std::make_pair(name, permitted_values));
		}

		/*!
			@returns @c true if this subclass of @c Struct has not yet declared any fields.
		*/
		bool needs_declare() {
			return contents_.empty();
		}

		/*!
			Performs a reverse lookup from field to name.
		*/
		std::string name_of(void *field) {
			const ssize_t offset = reinterpret_cast<uint8_t *>(field) - reinterpret_cast<uint8_t *>(this);

			auto iterator = contents_.begin();
			while(iterator != contents_.end()) {
				if(iterator->second.offset == offset) break;
				++iterator;
			}

			if(iterator != contents_.end()) {
				return iterator->first;
			} else {
				return "";
			}
		}

	private:
		template <typename Type> bool declare_reflectable(Type *t, const std::string &name) {
			if constexpr (std::is_base_of<Reflection::Struct, Type>::value) {
				Reflection::Struct *const str = static_cast<Reflection::Struct *>(t);
				declare_emplace(str, name);
				return true;
			}

			return false;
		}

		template <typename Type> void declare_emplace(Type *t, const std::string &name, size_t count = 1) {
			contents_.emplace(
				std::make_pair(
					name,
					Field(typeid(Type), reinterpret_cast<uint8_t *>(t) - reinterpret_cast<uint8_t *>(this), sizeof(Type), count)
				));
		}

		struct Field {
			const std::type_info *type;
			ssize_t offset;
			size_t size;
			size_t count;
			Field(const std::type_info &type, ssize_t offset, size_t size, size_t count) :
				type(&type), offset(offset), size(size), count(count) {}
		};
		static inline std::unordered_map<std::string, Field> contents_;
		static inline std::unordered_map<std::string, std::vector<bool>> permitted_enum_values_;
};

}

#endif /* Struct_hpp */

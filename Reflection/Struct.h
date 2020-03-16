//
//  Struct.h
//  Clock Signal
//
//  Created by Thomas Harte on 06/03/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Struct_h
#define Struct_h

#include <cstring>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "Enum.h"

namespace Reflection {

#define DeclareField(Name) declare(&Name, #Name)

struct Struct {
	virtual std::vector<std::string> all_keys() = 0;
	virtual const std::type_info *type_of(const std::string &name) = 0;
	virtual void set(const std::string &name, const void *value) = 0;
	virtual const void *get(const std::string &name) = 0;
	virtual ~Struct() {}
};

/*!
	Attempts to set the property @c name to @c value ; will perform limited type conversions.

	@returns @c true if the property was successfully set; @c false otherwise.
*/
template <typename Type> bool set(Struct &target, const std::string &name, Type value);

/*!
	Setting an int:

		* to an int copies the int;
		* to an int64_t promotes the int; and
		* to a registered enum, copies the int.
*/
template <> bool set(Struct &target, const std::string &name, int value);

/*!
	Setting a string:

		* to an enum, if the string names a member of the enum, sets the value.
*/
template <> bool set(Struct &target, const std::string &name, const std::string &value);
template <> bool set(Struct &target, const std::string &name, const char *value);


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
template <typename Type> bool get(Struct &target, const std::string &name, Type &value);

template <> bool get(Struct &target, const std::string &name, bool &value);


// TODO: move this elsewhere. It's just a sketch anyway.
struct Serialisable {
	/// Serialises this object, appending it to @c target.
	virtual void serialise(std::vector<uint8_t> &target) = 0;
	/// Deserialises this object from @c source.
	/// @returns @c true if the deserialisation was successful; @c false otherwise.
	virtual bool deserialise(const std::vector<uint8_t> &source) = 0;
};

template <typename Owner> class StructImpl: public Struct {
	public:
		/*!
			@returns the value of type @c Type that is loaded from the offset registered for the field @c name.
				It is the caller's responsibility to provide an appropriate type of data.
		*/
		const void *get(const std::string &name) final {
			const auto iterator = contents_.find(name);
			if(iterator == contents_.end()) return nullptr;
			return reinterpret_cast<uint8_t *>(this) + iterator->second.offset;
		}

		/*!
			Stores the @c value of type @c Type to the offset registered for the field @c name.

			It is the caller's responsibility to provide an appropriate type of data.
		*/
		void set(const std::string &name, const void *value) final {
			const auto iterator = contents_.find(name);
			if(iterator == contents_.end()) return;
			memcpy(reinterpret_cast<uint8_t *>(this) + iterator->second.offset, value, iterator->second.size);
		}

		/*!
			@returns @c type_info for the field @c name.
		*/
		const std::type_info *type_of(const std::string &name) final {
			const auto iterator = contents_.find(name);
			if(iterator == contents_.end()) return nullptr;
			return iterator->second.type;
		}

		/*!
			@returns A vector of all declared fields for this struct.
		*/
		std::vector<std::string> all_keys() final {
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
			Exposes the field pointed to by @c t for reflection as @c name.
		*/
		template <typename Type> void declare(Type *t, const std::string &name) {
			contents_.emplace(
				std::make_pair(
					name,
					Field(typeid(Type), reinterpret_cast<uint8_t *>(t) - reinterpret_cast<uint8_t *>(this), sizeof(Type))
				));
		}

		/*!
			@returns @c true if this subclass of @c Struct has not yet declared any fields.
		*/
		bool needs_declare() {
			return !contents_.size();
		}

	private:
		struct Field {
			const std::type_info *type;
			ssize_t offset;
			size_t size;
			Field(const std::type_info &type, ssize_t offset, size_t size) :
				type(&type), offset(offset), size(size) {}
		};
		static inline std::unordered_map<std::string, Field> contents_;
};

}

#endif /* Struct_h */

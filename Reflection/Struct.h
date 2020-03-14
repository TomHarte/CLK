//
//  Struct.h
//  Clock Signal
//
//  Created by Thomas Harte on 06/03/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Struct_h
#define Struct_h

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
	virtual void set_raw(const std::string &name, const void *value) = 0;
	virtual const void *get_raw(const std::string &name) = 0;
	virtual ~Struct() {}

	/*!
		Attempts to set the property @c name to @c value ; will perform limited type conversions.

		@returns @c true if t
	*/
	template <typename Type> bool set(const std::string &name, Type value) {
		return false;
	}

	/*!
		Setting an int:

			* to an int copies the int;
			* to an int64_t promotes the int; and
			* to a registered enum, copies the int.
	*/
	template <> bool set(const std::string &name, int value) {
		const auto target_type = type_of(name);
		if(!target_type) return false;

		// No need to convert an int or a registered enum.
		if(*target_type == typeid(int) || !Reflection::Enum::name(*target_type).empty()) {
			set_raw(name, &value);
			return true;
		}

		// Promote to an int64_t.
		if(*target_type == typeid(int64_t)) {
			const auto int64 = int64_t(value);
			set_raw(name, &int64);
			return true;
		}

		return false;
	}

	/*!
		Setting a string:

			* to an enum, if the string names a member of the enum, sets the value.
	*/
	template <> bool set(const std::string &name, const std::string &value) {
		const auto target_type = type_of(name);
		if(!target_type) return false;

		if(Reflection::Enum::name(*target_type).empty()) {
			return false;
		}

		const auto enum_value = Reflection::Enum::from_string(*target_type, value);
		if(enum_value == std::string::npos) {
			return false;
		}

		int int_value = int(enum_value);
		set_raw(name, &int_value);

		return true;
	}

	template <> bool set(const std::string &name, const char *value) {
		const std::string string(value);
		return set<const std::string &>(name, string);
	}

};

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
		const void *get_raw(const std::string &name) final {
			const auto iterator = contents_.find(name);
			if(iterator == contents_.end()) return nullptr;
			return reinterpret_cast<uint8_t *>(this) + iterator->second.offset;
		}

		/*!
			Stores the @c value of type @c Type to the offset registered for the field @c name.

			It is the caller's responsibility to provide an appropriate type of data.
		*/
		void set_raw(const std::string &name, const void *value) final {
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
			ssize_t offset, size;
			Field(const std::type_info &type, ssize_t offset, size_t size) :
				type(&type), offset(offset), size(size) {}
		};
		static inline std::unordered_map<std::string, Field> contents_;
};

}

#endif /* Struct_h */

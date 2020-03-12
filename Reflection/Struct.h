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

namespace Reflection {

#define DeclareField(Name) declare(&Name, #Name)

template <typename Owner> class Struct {
	public:
		/*!
			@returns the value of type @c Type that is loaded from the offset registered for the field @c name.
				It is the caller's responsibility to provide an appropriate type of data.
		*/
		template <typename Type> const Type *get(const std::string &name) {
			const auto iterator = contents_.find(name);
			if(iterator == contents_.end()) return nullptr;
			return reinterpret_cast<Type *>(reinterpret_cast<uint8_t *>(this) + iterator->second.offset);
		}

		/*!
			Stores the @c value of type @c Type to the offset registered for the field @c name.

			It is the caller's responsibility to provide an appropriate type of data.
		*/
		template <typename Type> void set(const std::string &name, const Type &value) {
			const auto iterator = contents_.find(name);
			if(iterator == contents_.end()) return;
			*reinterpret_cast<Type *>(reinterpret_cast<uint8_t *>(this) + iterator->second.offset) = value;
		}

		/*!
			@returns @c type_info for the field @c name.
		*/
		const std::type_info *type_of(const std::string &name) {
			const auto iterator = contents_.find(name);
			if(iterator == contents_.end()) return nullptr;
			return iterator->second.type;
		}

		/*!
			@returns A vector of all declared fields for this struct.
		*/
		std::vector<std::string> all_keys() {
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
					Field(typeid(Type), reinterpret_cast<uint8_t *>(t) - reinterpret_cast<uint8_t *>(this))
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
			Field(const std::type_info &type, ssize_t offset) :
				type(&type), offset(offset) {}
		};
		static inline std::unordered_map<std::string, Field> contents_;
};

}

#endif /* Struct_h */

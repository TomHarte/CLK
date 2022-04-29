//
//  Enum.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/02/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Enum_hpp
#define Enum_hpp

#include <algorithm>
#include <cctype>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <vector>
#include <unordered_map>

namespace Reflection {

#define ReflectableEnum(Name, ...)	\
	enum class Name { __VA_ARGS__ };	\
	constexpr static const char *__declaration##Name = #__VA_ARGS__

#define EnumDeclaration(Name) #Name, __declaration##Name

#define AnnounceEnum(Name) ::Reflection::Enum::declare<Name>(EnumDeclaration(Name))
#define AnnounceEnumNS(Namespace, Name) ::Reflection::Enum::declare<Namespace::Name>(#Name, Namespace::__declaration##Name)

/*!
	This provides a very slight version of enum reflection; you can introspect only:

		* enums have been registered, along with the text of their declarations;
		* provided that those enums do not declare specific values for their members.

	The macros above help avoid duplication of the declaration, making this just mildly less
	terrible than it might have been.

	No guarantees of speed or any other kind of efficiency are offered.
*/
class Enum {
	public:
		/*!
			Registers @c name and the entries within @c declaration for the enum type @c Type.

			Assuming the caller used the macros above, a standard pattern where both things can be placed in
			the same namespace might look like:

				ReflectableEnum(MyEnum, int, A, B, C);

				...

				AnnounceEnum(MyEnum)

			If AnnounceEnum cannot be placed into the same namespace as ReflectableEnum, see the
			EnumDeclaration macro.
		*/
		template <typename Type> static void declare(const char *name, const char *declaration) {
			const char *d_ptr = declaration;

			std::vector<std::string> result;
			while(true) {
				// Skip non-alphas, and exit if the terminator is found.
				while(*d_ptr && !isalpha(*d_ptr)) ++d_ptr;
				if(!*d_ptr) break;

				// Note the current location and proceed for all alphas and digits.
				const auto start = d_ptr;
				while(isalpha(*d_ptr) || isdigit(*d_ptr)) ++d_ptr;

				// Add a string view.
				result.emplace_back(std::string(start, size_t(d_ptr - start)));
			}

			members_by_type_.emplace(std::make_pair(std::type_index(typeid(Type)), result));
			names_by_type_.emplace(std::make_pair(std::type_index(typeid(Type)), std::string(name)));
		}

		/*!
			@returns the declared name of the enum @c Type if it has been registered; the empty string otherwise.
		*/
		template <typename Type> static const std::string &name() {
			return name(typeid(Type));
		}

		/*!
			@returns the declared name of the enum with type_info @c type if it has been registered; the empty string otherwise.
		*/
		static const std::string &name(std::type_index type) {
			const auto entry = names_by_type_.find(type);
			if(entry == names_by_type_.end()) return empty_string_;
			return entry->second;
		}

		/*!
			@returns the number of members of the enum @c Type if it has been registered; 0 otherwise.
		*/
		template <typename Type> static size_t size() {
			return size(typeid(Type));
		}

		/*!
			@returns the number of members of the enum with type_info @c type if it has been registered; @c std::string::npos otherwise.
		*/
		static size_t size(std::type_index type) {
			const auto entry = members_by_type_.find(type);
			if(entry == members_by_type_.end()) return std::string::npos;
			return entry->second.size();
		}

		/*!
			@returns A @c std::string name for the enum value @c e.
		*/
		template <typename Type> static const std::string &to_string(Type e) {
			return to_string(typeid(Type), int(e));
		}

		/*!
			@returns A @c std::string name for the enum value @c e from the enum with type_info @c type.
		*/
		static const std::string &to_string(std::type_index type, int e) {
			const auto entry = members_by_type_.find(type);
			if(entry == members_by_type_.end()) return empty_string_;
			return entry->second[size_t(e)];
		}

		/*!
			@returns a vector naming the members of the enum with type_info @c type if it has been registered; an empty vector otherwise.
		*/
		static const std::vector<std::string> &all_values(std::type_index type) {
			const auto entry = members_by_type_.find(type);
			if(entry == members_by_type_.end()) return empty_vector_;
			return entry->second;
		}

		/*!
			@returns a vector naming the members of the enum @c Type type if it has been registered; an empty vector otherwise.
		*/
		template <typename Type> static const std::vector<std::string> &all_values() {
			return all_values(typeid(Type));
		}

		/*!
			@returns A value of @c Type for the name @c str, or @c EnumType(std::string::npos) if
				the name is not found.
		*/
		template <typename Type> static Type from_string(const std::string &str) {
			return Type(from_string(typeid(Type), str));
		}

		/*!
			@returns A value for the name @c str in the enum with type_info @c type , or @c -1 if
				the name is not found.
		*/
		static int from_string(std::type_index type, const std::string &str) {
			const auto entry = members_by_type_.find(type);
			if(entry == members_by_type_.end()) return -1;
			const auto iterator = std::find(entry->second.begin(), entry->second.end(), str);
			if(iterator == entry->second.end()) return -1;
			return int(iterator - entry->second.begin());
		}

	private:
		static inline std::unordered_map<std::type_index, std::vector<std::string>> members_by_type_;
		static inline std::unordered_map<std::type_index, std::string> names_by_type_;
		static inline const std::string empty_string_;
		static inline const std::vector<std::string> empty_vector_;
};

}

#endif /* Enum_hpp */

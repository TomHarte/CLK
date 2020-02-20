//
//  Enum.h
//  Clock Signal
//
//  Created by Thomas Harte on 17/02/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef Enum_h
#define Enum_h

#include <cctype>

namespace Reflection {

template <typename EnumType> struct Enum {
		static size_t size() {
			return members().size();
		}

		/*!
			@returns A @c string_view name for the enum value @c e.
		*/
		static std::string_view toString(EnumType e) {
			return members()[size_t(e)];
		}

		/*!
			@returns A value of @c EnumType name for the name @c str, or @c EnumType(-1) if
				the string is not found.
		*/
		static EnumType fromString(const std::string_view &str) {
			const auto member_list = members();
			auto position = std::find(member_list.begin(), member_list.end(), str);
			if(position == member_list.end()) return EnumType(-1);
			return EnumType(position - member_list.begin());
		}

		/*!
			@returns A vector of string_views naming the members of this enum in value order.
		*/
		static std::vector<std::string_view> members() {
			EnumType m;
			const char *const declaration = __declaration(m);
			const char *d_ptr = declaration;

			std::vector<std::string_view> result;
			while(true) {
				// Skip non-alphas, and exit if the terminator is found.
				while(*d_ptr && !isalpha(*d_ptr)) ++d_ptr;
				if(!*d_ptr) break;

				// Note the current location and proceed for all alphas and digits.
				const auto start = d_ptr;
				while(isalpha(*d_ptr) || isdigit(*d_ptr)) ++d_ptr;

				// Add a string view.
				result.emplace_back(start, d_ptr - start);
			}

			return result;
		}
};

}

/*!
	Provides a very limited subset of normal enums, with the addition of reflection.

	Enum members must take default values, and this enum must be in the global scope.
*/
#define ReflectiveEnum(Name, Type, ...)	\
	enum class Name: Type { __VA_ARGS__ };	\
	constexpr const char *__declaration(Name) { return #__VA_ARGS__; }

#endif /* Enum_h */
